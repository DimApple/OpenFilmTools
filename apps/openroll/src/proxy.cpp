#include "proxy.hpp"
#include "raw.hpp"
#include "native_files.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cmath>
#include <stdexcept>

namespace {
[[noreturn]] void fail(const QString& message) { throw std::runtime_error(message.toStdString()); }
void checkCancel(const std::atomic_bool& cancelled)
{
    if (cancelled.load()) fail("Export cancelled.");
}

class Encoder {
public:
    Encoder(const QStringList& arguments, const std::atomic_bool& cancelled) : cancel(cancelled)
    {
        
        process.start("ffmpeg", arguments);
        if (!process.waitForStarted(5000)) fail("Cannot start FFmpeg: " + process.errorString());
    }

    ~Encoder()
    {
        if (process.state() != QProcess::NotRunning) {
            process.kill();
            process.waitForFinished(3000);
        }
    }

    void write(const QImage& image)
    {
        checkCancel(cancel);
        QElapsedTimer deadline;
        deadline.start();
        while (process.bytesToWrite() && process.state() != QProcess::NotRunning) {
            checkCancel(cancel);
            process.waitForBytesWritten(50);
            drain();
            if (deadline.elapsed() > 30000) fail("FFmpeg stopped accepting frame data.");
        }
        const auto bytes = image.sizeInBytes();
        if (process.state() == QProcess::NotRunning ||
            process.write(reinterpret_cast<const char*>(image.constBits()), bytes) != bytes)
            fail("FFmpeg stopped accepting frames. " + QString::fromUtf8(errors));
    }

    void finish(bool streamed = true, const std::function<void(qint64)>& report = {})
    {
        process.closeWriteChannel();
        QElapsedTimer deadline;
        deadline.start();
        while (process.state() != QProcess::NotRunning) {
            checkCancel(cancel);
            process.waitForFinished(50);
            drain();
            if (report) readProgress(report);
            if (streamed && deadline.elapsed() > 180000) fail("FFmpeg did not finish within three minutes.");
        }
        drain();
        if (report) readProgress(report);
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
            fail("FFmpeg export failed. " + QString::fromUtf8(errors));
    }

private:
    void readProgress(const std::function<void(qint64)>& report)
    {
        pending += process.readAllStandardOutput();
        qsizetype end;
        while ((end = pending.indexOf('\n')) >= 0) {
            const auto line = pending.left(end);
            pending.remove(0, end + 1);
            if (!line.startsWith("out_time_us=")) continue;
            bool valid = false;
            const auto value = line.mid(12).trimmed().toLongLong(&valid);
            if (valid && value >= 0) report(value);
        }
    }
    QByteArray pending;
    void drain() { errors = (errors + process.readAllStandardError()).right(4096); }
    const std::atomic_bool& cancel;
    QProcess process;
    QByteArray errors;
};

QString frameRate(double fps)
{
    for (const int numerator : {24000, 30000, 48000, 60000, 120000}) {
        if (std::abs(fps - numerator / 1001.0) < 0.0002)
            return QString::number(numerator) + "/1001";
    }
    return QString::number(fps, 'f', 6);
}
}

QString exportSuffix(ExportPreset preset)
{
    switch (preset) {
    case ExportPreset::H264Mkv: return "_H264.mkv";
    case ExportPreset::H264Mp4: return "_H264.mp4";
    case ExportPreset::Wav: return "_Audio.wav";
    case ExportPreset::DnxLB: return "_DNxHR_LB.mov";
    case ExportPreset::DnxSQ: return "_DNxHR_SQ.mov";
    case ExportPreset::DnxHQ: return "_DNxHR_HQ.mov";
    case ExportPreset::DnxHQX: return "_DNxHR_HQX.mov";
    case ExportPreset::ProResProxy: return "_Proxy.mov";
    case ExportPreset::ProResLT: return "_ProRes_LT.mov";
    case ExportPreset::ProResStandard: return "_ProRes_422.mov";
    case ExportPreset::ProResHQ: return "_ProRes_HQ.mov";
    case ExportPreset::ProRes4444: return "_ProRes_4444.mov";
    case ExportPreset::ProRes4444XQ: return "_ProRes_4444_XQ.mov";
    }
    return {};
}

namespace {
bool h264(ExportPreset preset)
{
    return preset == ExportPreset::H264Mkv || preset == ExportPreset::H264Mp4;
}

QStringList videoOptions(ExportPreset preset, int height)
{
    QStringList args{"-vf", QString("scale=-2:trunc(min(ih\\,%1)/2)*2").arg(height), "-threads", "4"};
    if (h264(preset))
        return args + QStringList{"-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "18"};
    if (preset >= ExportPreset::DnxLB && preset <= ExportPreset::DnxHQX) {
        const QStringList profiles{"dnxhr_lb", "dnxhr_sq", "dnxhr_hq", "dnxhr_hqx"};
        return args + QStringList{"-c:v", "dnxhd", "-profile:v", profiles.at(int(preset) - int(ExportPreset::DnxLB)),
                                  "-pix_fmt", preset == ExportPreset::DnxHQX ? "yuv422p10le" : "yuv422p"};
    }
    return args + QStringList{"-c:v", "prores_ks", "-profile:v", QString::number(int(preset)),
                              "-pix_fmt", preset >= ExportPreset::ProRes4444 ? "yuv444p10le" : "yuv422p10le"};
}

QStringList audioOptions(ExportPreset preset)
{
    if (preset == ExportPreset::H264Mp4) return {"-c:a", "aac", "-ac", "2", "-b:a", "192k"};
    return {"-c:a", "pcm_s24le"};
}

void publish(const QString& from, const QFileInfo& output)
{
    nativeFiles::publish(from, output.absoluteFilePath());
}
}

void transcodeMedia(const QString& source, const QString& destination, int maxHeight, ExportPreset preset,
                     const std::atomic_bool& cancelled,
                     const std::function<void(qint64, qint64, const QString&)>& progress)
{
    checkCancel(cancelled);
    const QFileInfo output(destination);
    if (output.exists() || output.isSymLink()) fail("Output already exists: " + destination);
    if (!QFileInfo(source).isFile() || !QFileInfo(output.absolutePath()).isDir() || maxHeight < 2)
        fail("Choose an available source and output folder.");

    QTemporaryDir temp(output.absolutePath() + "/.openroll-proxy-XXXXXX");
    if (!temp.isValid()) fail("Could not create a temporary export folder.");
    const auto final = temp.filePath("export" + exportSuffix(preset));
    const QStringList base{"-nostdin", "-v", "error", "-xerror", "-n"};
    const bool audioOnly = preset == ExportPreset::Wav;
    if (QFileInfo(source).suffix().compare("braw", Qt::CaseInsensitive) != 0) {
        QStringList args = base + QStringList{"-i", QFileInfo(source).absoluteFilePath()};
        if (audioOnly) {
            args << "-map" << "0:a:0";
        } else {
            args << "-map" << "0:V:0" << "-map" << "0:a?";
            args += videoOptions(preset, maxHeight);
            args << "-fps_mode" << "passthrough";
        }
        args += audioOptions(preset);
        progress(0, 0, "Reading media duration");
        QProcess probe;
        probe.start("ffprobe", {"-v", "error", "-show_entries", "format=duration",
                    "-of", "json", "-i", QFileInfo(source).absoluteFilePath()});
        QElapsedTimer timer;
        timer.start();
        while (probe.state() != QProcess::NotRunning && timer.elapsed() < 5000 && !cancelled.load())
            probe.waitForFinished(50);
        if (probe.state() != QProcess::NotRunning) {
            probe.kill();
            probe.waitForFinished(1000);
        }
        checkCancel(cancelled);
        const auto seconds = QJsonDocument::fromJson(probe.readAllStandardOutput()).object()
                                 .value("format").toObject().value("duration").toString().toDouble();
        const qint64 total = std::isfinite(seconds) && seconds > 0 && seconds < 1e10
                                ? qint64(seconds * 1000000) : 0;
        args << "-progress" << "pipe:1" << "-nostats";
        progress(0, total, "Encoding");
        Encoder encoder(args + QStringList{final}, cancelled);
        encoder.finish(false, [&](qint64 done) {
            progress(total > 0 ? qMin(done, total) : done, total, "Encoding");
        });
        progress(total, total, "Finalizing export");
        checkCancel(cancelled);
        publish(final, output);
        return;
    }
    const auto video = temp.filePath("picture" + exportSuffix(preset));
    QFile audio(temp.filePath("audio.pcm"));
    const auto runtime = brawRuntimeDirectory();
    RawDecoder decoder(runtime);
    const auto info = decoder.open(source);
    const auto fps = frameRate(info.fps);
    const bool hasAudio = info.audioChannels > 0;
    if (hasAudio && !audio.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        fail("Could not create temporary audio: " + audio.errorString());

    if (audioOnly && !hasAudio) fail("This BRAW clip has no supported audio track.");
    std::unique_ptr<Encoder> encoder;
    if (!audioOnly) {
        auto args = base + QStringList{"-f", "rawvideo", "-pixel_format", "rgb0", "-video_size",
                      QString("%1x%2").arg(info.size.width()).arg(info.size.height()), "-framerate", fps,
                      "-i", "pipe:0", "-an"};
        args += videoOptions(preset, maxHeight);
        encoder = std::make_unique<Encoder>(args + QStringList{video}, cancelled);
    }
    for (qint64 i = 0; i < info.frames; ++i) {
        checkCancel(cancelled);
        auto frame = decoder.read(i, hasAudio, true);
        if (frame.image.size() != info.size || frame.image.bytesPerLine() != info.size.width() * 4)
            fail(QString("Unexpected BRAW dimensions at frame %1.").arg(i + 1));
        if (encoder) encoder->write(frame.image);
        if (hasAudio && audio.write(frame.pcm) != frame.pcm.size())
            fail("Could not spool audio: " + audio.errorString());
        progress(i + 1, info.frames, audioOnly ? "Extracting audio" : "Encoding picture");
    }
    progress(info.frames, info.frames, "Finalizing export");
    if (encoder) encoder->finish();
    if (!hasAudio && (info.timecode.isEmpty() || h264(preset))) {
        checkCancel(cancelled);
        publish(video, output);
        return;
    }
    progress(info.frames, info.frames, "Finalizing export");
    if (hasAudio && !audio.flush()) fail("Could not flush temporary audio.");
    audio.close();
    auto args = base;
    if (!audioOnly) args << "-i" << video;
    if (hasAudio)
        args << "-f" << "s32le" << "-ar" << QString::number(info.audioRate) << "-ac"
             << QString::number(info.audioChannels) << "-i" << audio.fileName();
    if (!audioOnly) {
        args << "-map" << "0:v:0" << "-c:v" << "copy";
        if (!info.timecode.isEmpty() && !h264(preset)) args << "-timecode" << info.timecode;
    }
    if (hasAudio) {
        args << "-map" << (audioOnly ? "0:a:0" : "1:a:0");
        args += audioOptions(preset);
    }
    Encoder muxer(args + QStringList{final}, cancelled);
    muxer.finish();
    checkCancel(cancelled);
    publish(final, output);
}
