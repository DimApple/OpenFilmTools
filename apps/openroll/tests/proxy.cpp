#include "proxy.hpp"
#include "raw.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

QByteArray command(const QString& program, const QStringList& args)
{
    QProcess p;
    p.start(program, args);
    require(p.waitForFinished(30000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0,
            qPrintable(program + ": " + QString::fromUtf8(p.readAllStandardError())));
    return p.readAllStandardOutput();
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const auto arguments = app.arguments();
    if (arguments.size() != 3 && arguments.size() != 4) return 2;
    try {
        const auto file = arguments[1];
        qputenv("OPENROLL_BRAW_RUNTIME", QFile::encodeName(arguments[2]));
        QTemporaryDir directory;
        require(directory.isValid(), "Cannot create a proxy test directory");
        RawDecoder source(arguments[2]);
        const auto info = source.open(file);
        const auto dest = directory.filePath("proxy.mov");
        std::atomic_bool cancelled(false);
        const auto quiet = [](qint64, qint64, const QString&) {};
        if (arguments.size() == 4) {
            struct PresetCheck { ExportPreset preset; const char* codec; const char* profile; };
            const PresetCheck presets[] = {
                {ExportPreset::ProResProxy, "prores", "Proxy"}, {ExportPreset::ProResLT, "prores", "LT"},
                {ExportPreset::ProResStandard, "prores", "Standard"}, {ExportPreset::ProResHQ, "prores", "HQ"},
                {ExportPreset::ProRes4444, "prores", "4444"}, {ExportPreset::ProRes4444XQ, "prores", "XQ"},
                {ExportPreset::DnxLB, "dnxhd", "DNXHR LB"}, {ExportPreset::DnxSQ, "dnxhd", "DNXHR SQ"},
                {ExportPreset::DnxHQ, "dnxhd", "DNXHR HQ"}, {ExportPreset::DnxHQX, "dnxhd", "DNXHR HQX"},
                {ExportPreset::H264Mkv, "h264", ""}, {ExportPreset::H264Mp4, "h264", ""},
                {ExportPreset::Wav, "pcm_s24le", ""}
            };
            for (const auto& entry : presets) {
                const auto out = directory.filePath("conventional" + exportSuffix(entry.preset));
                bool measured = false;
                transcodeMedia(arguments[3], out, 720, entry.preset, cancelled,
                               [&](qint64 done, qint64 total, const QString& phase) {
                    measured |= phase == "Encoding" && done > 0 && total >= done;
                });
                require(measured, "FFmpeg did not report measured export progress");
                const auto probe = command("ffprobe", {"-v", "error", "-select_streams",
                    entry.preset == ExportPreset::Wav ? "a:0" : "v:0", "-show_entries", "stream=codec_name,profile",
                    "-of", "default=nw=1", out});
                require(probe.contains(QByteArray("codec_name=") + entry.codec) &&
                        (!*entry.profile || probe.toLower().contains(QByteArray(entry.profile).toLower())),
                        qPrintable(out + ": " + QString::fromUtf8(probe)));
                std::cout << "Verified " << exportSuffix(entry.preset).toStdString() << '\n';
            }
            for (auto preset : {ExportPreset::DnxLB, ExportPreset::H264Mkv, ExportPreset::H264Mp4, ExportPreset::Wav}) {
                const auto out = directory.filePath("braw" + exportSuffix(preset));
                transcodeMedia(file, out, 720, preset, cancelled, quiet);
                command("ffmpeg", {"-v", "error", "-xerror", "-i", out, "-f", "null", "-"});
                std::cout << "Decoded BRAW export " << exportSuffix(preset).toStdString() << '\n';
            }
        }
        transcodeMedia(file, dest, 720, ExportPreset::ProResProxy, cancelled, quiet);
        const auto data = command("ffprobe", {"-v", "error", "-show_streams", "-of", "json", dest});
        const auto streams = QJsonDocument::fromJson(data).object().value("streams").toArray();
        QJsonObject video;
        QJsonObject audio;
        for (const auto stream : streams) {
            const auto entry = stream.toObject();
            if (entry.value("codec_type") == "video") video = entry;
            if (entry.value("codec_type") == "audio") audio = entry;
        }
        require(video.value("codec_name") == "prores" && video.value("codec_tag_string") == "apco",
                "Output is not ProRes Proxy");
        require(video.value("height").toInt() == (qMin(720, info.size.height()) & ~1) &&
                video.value("nb_frames").toString().toLongLong() == info.frames,
                "Output has wrong dimensions or frame count");
        const auto rationals = video.value("avg_frame_rate").toString().split('/');
        const auto fps = rationals.value(0).toDouble() / rationals.value(1).toDouble();
        const auto length = info.frames / info.fps;
        require(std::abs(fps - info.fps) < 0.0002 &&
                std::abs(video.value("duration").toString().toDouble() - length) < 0.001,
                "Video frame rate or duration changed");
        if (info.audioChannels > 0) {
            require(audio.value("codec_name") == "pcm_s24le" &&
                    audio.value("channels").toInt() == info.audioChannels &&
                    audio.value("sample_rate").toString().toInt() == info.audioRate,
                    "Output audio channel count or sample rate differs");
            const auto audioDuration = audio.value("duration").toString().toDouble();
            require(std::abs(audioDuration - length) < 0.001 &&
                    std::abs(audio.value("start_time").toString().toDouble() -
                             video.value("start_time").toString().toDouble()) < 0.001,
                    "Output audio and video timing differs");
        }
        if (!info.timecode.isEmpty()) {
            require(video.value("tags").toObject().value("timecode").toString() == info.timecode,
                    "Output timecode differs from source");
        }
        command("ffmpeg", {"-v", "error", "-xerror", "-i", dest, "-map", "0:v:0", "-map", "0:a?",
                           "-f", "null", "-"});
        const auto outputSize = QFileInfo(dest).size();
        bool refused = false;
        try { transcodeMedia(file, dest, 720, ExportPreset::ProResProxy, cancelled, quiet); }
        catch (const std::exception&) { refused = true; }
        require(refused && QFileInfo(dest).size() == outputSize, "Export overwrote an existing file");
        const auto stopped = directory.filePath("cancelled.mov");
        bool stoppedByCancel = false;
        try {
            transcodeMedia(file, stopped, 720, ExportPreset::ProResProxy, cancelled, [&](qint64, qint64, const QString&) { cancelled.store(true); });
        } catch (const std::exception& error) {
            stoppedByCancel = QString::fromUtf8(error.what()).contains("cancelled");
        }
        require(stoppedByCancel && !QFileInfo::exists(stopped), "Cancelled output was published or failed unexpectedly");
        require(QDir(directory.path()).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty(),
                "Temporary files survived cancellation");
        std::cout << "BRAW to ProRes Proxy: " << info.frames << " frames at " << fps << " fps, "
                  << info.audioChannels << " audio channels, " << length
                  << " seconds. Independent decode, timing, no-overwrite and cancellation passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
