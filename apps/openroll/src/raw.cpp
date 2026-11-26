#include "raw.hpp"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <oleauto.h>
#define OPENROLL_IID(interfaceType) __uuidof(interfaceType)
using SdkString = BSTR;
#else
#define OPENROLL_IID(interfaceType) IID_##interfaceType
using SdkString = const char*;
#endif
#include <BlackmagicRawAPI.h>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QLibrary>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {
template<class T> struct Release {
    void operator()(T* p) const { if (p) p->Release(); }
};
template<class T> using Com = std::unique_ptr<T, Release<T>>;

void check(HRESULT result, const char* operation)
{
    if (result != S_OK)
        throw std::runtime_error(QString("%1 (SDK status 0x%2)")
                                     .arg(operation).arg(quint32(result), 8, 16, QChar('0')).toStdString());
}

class Callback final : public IBlackmagicRawCallback {
public:
    HRESULT result = E_FAIL;
    const char* stage = "No SDK completion callback";
    BlackmagicRawResolutionScale scale = blackmagicRawResolutionScaleQuarter;
    QImage image;

    HRESULT QueryInterface(REFIID id, void** object) override
    {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (!(id == OPENROLL_IID(IUnknown)) && !(id == OPENROLL_IID(IBlackmagicRawCallback))) return E_NOINTERFACE;
        *object = static_cast<IBlackmagicRawCallback*>(this);
        AddRef();
        return S_OK;
    }

    ULONG AddRef() override { return ++refs; }
    ULONG Release() override
    {
        const auto count = --refs;
        if (!count) delete this;
        return count;
    }

    void ReadComplete(IBlackmagicRawJob*, HRESULT status, IBlackmagicRawFrame* frame) override
    {
        IBlackmagicRawJob* job = nullptr;
        const char* operation = "Read completion";
        if (status == S_OK) {
            operation = "Set preview resolution";
            status = frame->SetResolutionScale(scale);
        }
        if (status == S_OK) {
            operation = "Set pixel format";
            status = frame->SetResourceFormat(blackmagicRawResourceFormatRGBAU8);
        }
        if (status == S_OK) {
            operation = "Create decode job";
            status = frame->CreateJobDecodeAndProcessFrame(nullptr, nullptr, &job);
        }
        Com<IBlackmagicRawJob> guard(job);
        if (status == S_OK) {
            operation = "Submit decode job";
            status = job->Submit();
        }
        if (status != S_OK) {
            stage = operation;
            result = status;
        }
    }

    void ProcessComplete(IBlackmagicRawJob*, HRESULT status, IBlackmagicRawProcessedImage* processed) override
    {
        stage = status == S_OK ? "Read decoded pixel buffer" : "SDK processing completion";
        uint32_t width = 0, height = 0, bytes = 0;
        void* pixels = nullptr;
        if (status == S_OK) status = processed->GetWidth(&width);
        if (status == S_OK) status = processed->GetHeight(&height);
        if (status == S_OK) status = processed->GetResourceSizeBytes(&bytes);
        if (status == S_OK) status = processed->GetResource(&pixels);
        const auto expected = quint64(width) * height * 4;
        // The SDK can round the CPU image allocation up to a 4 KiB boundary.
        const auto roundedSize = (expected + 4095) & ~quint64(4095);
        const bool validSize = bytes == expected || bytes == roundedSize;
        if (status == S_OK && pixels && validSize && width && height &&
            width <= 32768 && height <= 32768) {
            image = QImage(static_cast<const uchar*>(pixels), int(width), int(height),
                           qsizetype(width) * 4, QImage::Format_RGBX8888).copy();
            if (image.isNull()) status = E_FAIL;
        } else if (status == S_OK) {
            status = E_FAIL;
        }
        result = status;
    }

    void ReadAudioComplete(IBlackmagicRawJob*, HRESULT, IBlackmagicRawAudioBuffer*) override {}
    void DecodeComplete(IBlackmagicRawJob*, HRESULT) override {}
    void TrimProgress(IBlackmagicRawJob*, float) override {}
    void TrimComplete(IBlackmagicRawJob*, HRESULT) override {}
    void SidecarMetadataParseWarning(IBlackmagicRawClip*, SdkString, uint32_t, SdkString) override {}
    void SidecarMetadataParseError(IBlackmagicRawClip*, SdkString, uint32_t, SdkString) override {}
    void PreparePipelineComplete(void*, HRESULT) override {}

private:
    std::atomic<ULONG> refs{1};
};
}

struct RawDecoder::State {
    QLibrary library;
    Com<IBlackmagicRawFactory> factory;
    Com<IBlackmagicRaw> codec;
    Com<IBlackmagicRawClip> clip;
    Com<IBlackmagicRawClipAudio> audio;
    Com<Callback> callback;
    RawInfo info;
    uint64_t audioSamples = 0;
    uint32_t sourceAudioBytes = 0;
};

RawDecoder::RawDecoder(const QString& directory) : state(std::make_unique<State>())
{
#ifdef Q_OS_WIN
    state->library.setFileName(QDir(directory).absoluteFilePath("BlackmagicRawAPI.dll"));
#else
    state->library.setFileName(QDir(directory).absoluteFilePath("libBlackmagicRawAPI.so"));
#endif
    // The SDK manages auxiliary decoder libraries and may retain internal process state.
    state->library.setLoadHints(QLibrary::PreventUnloadHint | QLibrary::ExportExternalSymbolsHint);
    using Factory = IBlackmagicRawFactory* (*)();
    auto create = reinterpret_cast<Factory>(state->library.resolve("CreateBlackmagicRawFactoryInstance"));
    if (!create) throw std::runtime_error(state->library.errorString().toStdString());
    state->factory.reset(create());
    if (!state->factory) throw std::runtime_error("Blackmagic RAW factory creation failed.");
}

RawDecoder::~RawDecoder() = default;

RawInfo RawDecoder::open(const QString& path)
{
    auto& s = *state;
    s.audio.reset();
    s.clip.reset();
    s.codec.reset();
    s.callback.reset();
    s.info = {};
    s.audioSamples = 0;
    s.sourceAudioBytes = 0;

    IBlackmagicRaw* codec = nullptr;
    check(s.factory->CreateCodec(&codec), "Could not create the BRAW decoder");
    s.codec.reset(codec);
    IBlackmagicRawConfiguration* configuration = nullptr;
    check(codec->QueryInterface(OPENROLL_IID(IBlackmagicRawConfiguration), reinterpret_cast<void**>(&configuration)),
          "Could not configure the BRAW decoder");
    Com<IBlackmagicRawConfiguration> config(configuration);
    check(config->SetPipeline(blackmagicRawPipelineCPU, nullptr, nullptr), "Could not select CPU decoding");

    IBlackmagicRawClip* clip = nullptr;
#ifdef Q_OS_WIN
    const auto native = QDir::toNativeSeparators(path).toStdWString();
    const auto filename = SysAllocStringLen(native.data(), UINT(native.size()));
    if (!filename) throw std::bad_alloc();
    const auto opened = codec->OpenClip(filename, &clip);
    SysFreeString(filename);
    check(opened, "Could not open BRAW clip");
#else
    check(codec->OpenClip(QFile::encodeName(path).constData(), &clip), "Could not open BRAW clip");
#endif
    s.clip.reset(clip);
    s.callback.reset(new Callback);
    check(codec->SetCallback(s.callback.get()), "Could not register BRAW callbacks");

    uint32_t width = 0, height = 0;
    uint64_t frames = 0;
    float fps = 0;
    check(clip->GetWidth(&width), "Could not read BRAW width");
    check(clip->GetHeight(&height), "Could not read BRAW height");
    check(clip->GetFrameRate(&fps), "Could not read BRAW frame rate");
    check(clip->GetFrameCount(&frames), "Could not read BRAW frame count");
    if (!width || !height || width > 32768 || height > 32768 || !std::isfinite(fps) ||
        fps < 1 || fps > 1000 || !frames || frames > uint64_t(std::numeric_limits<qint64>::max() / 1000000))
        throw std::runtime_error("BRAW clip dimensions or timing are invalid.");

    s.info.size = QSize(int(width), int(height));
    s.info.fps = fps;
    s.info.frames = qint64(frames);
    SdkString timecode = nullptr;
    if (clip->GetTimecodeForFrame(0, &timecode) == S_OK && timecode) {
#ifdef Q_OS_WIN
        s.info.timecode = QString::fromWCharArray(timecode, SysStringLen(timecode));
#else
        s.info.timecode = QString::fromUtf8(timecode);
#endif
    }
#ifdef Q_OS_WIN
    SysFreeString(timecode);
    BSTR camera = nullptr;
    if (clip->GetCameraType(&camera) == S_OK && camera)
        s.info.camera = QString::fromWCharArray(camera, SysStringLen(camera));
    SysFreeString(camera);
#else
    std::free(const_cast<char*>(timecode));

    using VariantFunction = HRESULT (*)(Variant*);
    const auto initVariant = reinterpret_cast<VariantFunction>(s.library.resolve("VariantInit"));
    const auto clearVariant = reinterpret_cast<VariantFunction>(s.library.resolve("VariantClear"));
    Variant value{};
    if (initVariant && clearVariant && initVariant(&value) == S_OK) {
        if (clip->GetMetadata("camera_type", &value) == S_OK &&
            value.vt == blackmagicRawVariantTypeString && value.bstrVal)
            s.info.camera = QString::fromUtf8(value.bstrVal);
        clearVariant(&value);
    }
#endif

    IBlackmagicRawClipAudio* audio = nullptr;
    if (clip->QueryInterface(OPENROLL_IID(IBlackmagicRawClipAudio), reinterpret_cast<void**>(&audio)) == S_OK && audio) {
        s.audio.reset(audio);
        BlackmagicRawAudioFormat format{};
        uint32_t rate = 0, channels = 0, bits = 0;
        if (audio->GetAudioFormat(&format) == S_OK && format == blackmagicRawAudioFormatPCMLittleEndian &&
            audio->GetAudioSampleRate(&rate) == S_OK && rate > 0 && rate <= 192000 &&
            audio->GetAudioChannelCount(&channels) == S_OK && channels > 0 && channels <= 32 &&
            audio->GetAudioBitDepth(&bits) == S_OK && (bits == 16 || bits == 24 || bits == 32) &&
            audio->GetAudioSampleCount(&s.audioSamples) == S_OK && s.audioSamples > 0) {
            s.info.audioRate = int(rate);
            s.info.audioChannels = int(channels);
            s.info.audioSamples = s.audioSamples;
            s.sourceAudioBytes = bits / 8;
        }
    }
    return s.info;
}

RawFrame RawDecoder::read(qint64 index, bool withAudio, bool fullResolution)
{
    auto& s = *state;
    if (!s.clip || index < 0 || index >= s.info.frames)
        throw std::runtime_error("BRAW frame index is out of range.");
    auto decode = [&](BlackmagicRawResolutionScale scale) {
        s.callback->image = {};
        s.callback->result = E_FAIL;
        s.callback->stage = "No SDK completion callback";
        s.callback->scale = scale;
        IBlackmagicRawJob* job = nullptr;
        check(s.clip->CreateJobReadFrame(uint64_t(index), &job), "Could not create BRAW read job");
        Com<IBlackmagicRawJob> guard(job);
        const auto submitted = job->Submit();
        guard.reset();
        check(submitted, "Could not submit BRAW read job");
        check(s.codec->FlushJobs(), "Could not finish BRAW jobs");
        return s.callback->result;
    };
    auto result = decode(fullResolution ? blackmagicRawResolutionScaleFull : blackmagicRawResolutionScaleQuarter);
    const auto previewFailed = [&] {
        const auto stage = QString::fromLatin1(s.callback->stage);
        return result != S_OK && (stage == "Read decoded pixel buffer" || stage == "Set preview resolution");
    };
    if (!fullResolution && previewFailed()) {
        result = decode(blackmagicRawResolutionScaleHalf);
        if (previewFailed())
            result = decode(blackmagicRawResolutionScaleFull);
    }
    // A read failure precedes resolution selection. Retry at the requested scale.
    if (result == E_UNEXPECTED && QString::fromLatin1(s.callback->stage) == "Read completion") {
        qWarning().noquote() << QString("BRAW frame %1: SDK read failed (0x8000ffff). Retrying once.")
                                   .arg(index + 1);
        result = decode(s.callback->scale);
    }
    if (result != S_OK) {
        const auto message = QString("Could not decode BRAW frame %1 of %2. %3")
                                 .arg(index + 1).arg(s.info.frames)
                                 .arg(QString::fromLatin1(s.callback->stage));
        check(result, message.toUtf8().constData());
    }
    RawFrame output{std::move(s.callback->image), {}};
    if (!withAudio || !s.sourceAudioBytes)
        return output;

    const auto first = qint64(std::llround(index * s.info.audioRate / s.info.fps));
    const auto last = qint64(std::llround((index + 1) * s.info.audioRate / s.info.fps));
    const int count = int(last - first);
    const auto channels = uint32_t(s.info.audioChannels);
    QByteArray input(qsizetype(count) * channels * s.sourceAudioBytes, '\0');
    uint32_t read = 0, readBytes = 0;
    if (uint64_t(first) < s.audioSamples) {
        const auto requested = qMin(uint64_t(count), s.audioSamples - uint64_t(first));
        check(s.audio->GetAudioSamples(first, input.data(), uint32_t(input.size()), uint32_t(requested),
                                       &read, &readBytes), "Could not read BRAW audio");
        if (read > requested || readBytes != read * channels * s.sourceAudioBytes)
            throw std::runtime_error("Unexpected BRAW audio buffer size.");
    }
    // Preserve mono/stereo and preview channels 1–2 for multichannel recordings.
    const auto outputChannels = fullResolution ? channels : qMin(channels, 2u);
    output.pcm.resize(qsizetype(count) * outputChannels * 4);
    auto* dest = output.pcm.data();
    for (auto sample = 0; sample < count; ++sample) {
        for (uint32_t channel = 0; channel < outputChannels; ++channel) {
            const auto* source = input.constData() + (sample * channels + channel) * s.sourceAudioBytes;
            for (unsigned byte = 0; byte < 4 - s.sourceAudioBytes; ++byte)
                *dest++ = 0;
            for (unsigned byte = 0; byte < s.sourceAudioBytes; ++byte)
                *dest++ = source[byte];
        }
    }
    return output;
}


QString brawRuntimeDirectory()
{
    const auto configured = qEnvironmentVariable("OPENROLL_BRAW_RUNTIME");
    if (!configured.isEmpty()) return configured;
#ifdef Q_OS_WIN
    const auto programFiles = qEnvironmentVariable("ProgramFiles");
    return programFiles + "/Blackmagic Design/Blackmagic RAW/Blackmagic RAW SDK/Win/Libraries";
#else
    return QDir::home().absoluteFilePath(".local/opt/BlackmagicRAWSDK/Linux/Libraries");
#endif
}
