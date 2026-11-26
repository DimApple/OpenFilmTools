#include "project.hpp"
#include "raw.hpp"

#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <cmath>

namespace {
QByteArray command(const QString& program, const QStringList& args, const std::atomic_bool& cancel)
{
    QProcess process;
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(program, args);
    if (!process.waitForStarted(2000)) return {};
    for (int elapsed = 0; elapsed < 10000 && !cancel.load(); elapsed += 50) {
        if (process.waitForFinished(50))
            return process.exitCode() == 0 && process.exitStatus() == QProcess::NormalExit
                ? process.readAllStandardOutput() : QByteArray{};
    }
    process.kill();
    process.waitForFinished(1000);
    return {};
}

double rational(const QString& value)
{
    const auto parts = value.split('/');
    const double divisor = parts.value(1).toDouble();
    const double rate = divisor > 0 ? parts.value(0).toDouble() / divisor : 0;
    return std::isfinite(rate) && rate > 0 ? rate : 0;
}

void conventional(Clip& clip, const std::atomic_bool& cancel)
{
    const auto json = command("ffprobe", {"-v", "error", "-show_entries",
        "format=duration:format_tags=timecode,make,model:stream=codec_name,codec_type,width,height,avg_frame_rate:stream_disposition=attached_pic:stream_tags=timecode",
        "-of", "json", "-i", clip.path}, cancel);
    const auto object = QJsonDocument::fromJson(json).object();
    const auto format = object.value("format").toObject();
    const auto tags = format.value("tags").toObject();
    bool validDuration = false;
    const auto duration = format.value("duration").toString().toDouble(&validDuration);
    if (validDuration && std::isfinite(duration) && duration >= 0) clip.duration = duration;
    clip.timecode = tags.value("timecode").toString();
    clip.camera = (tags.value("make").toString() + ' ' + tags.value("model").toString()).trimmed();

    QString audio;
    for (const auto value : object.value("streams").toArray()) {
        const auto stream = value.toObject();
        const auto type = stream.value("codec_type").toString();
        if (type == "audio" && audio.isEmpty()) audio = stream.value("codec_name").toString();
        if (clip.timecode.isEmpty()) clip.timecode = stream.value("tags").toObject().value("timecode").toString();
        if (type == "video" && stream.value("disposition").toObject().value("attached_pic").toInt() == 0 &&
            !clip.size.isValid()) {
            clip.size = QSize(stream.value("width").toInt(), stream.value("height").toInt());
            clip.codec = stream.value("codec_name").toString();
            clip.fps = rational(stream.value("avg_frame_rate").toString());
        }
    }
    if (clip.size.width() > 0 && clip.size.height() > 0 && !cancel.load())
        clip.thumbnail = command("ffmpeg", {"-nostdin", "-v", "error", "-threads", "2", "-i", clip.path,
            "-map", "0:V:0", "-an", "-frames:v", "1", "-vf", "scale=160:90:force_original_aspect_ratio=decrease",
            "-c:v", "mjpeg", "-threads", "1", "-f", "image2pipe", "pipe:1"}, cancel);
    if (clip.codec.isEmpty()) clip.codec = audio;
    if (clip.codec.isEmpty()) clip.note = "Metadata unavailable. Check the file and FFprobe installation.";
}
}

QList<Clip> scanMedia(const QString& folder, const std::atomic_bool& cancel)
{
    QList<Clip> result;
    const QSet<QString> supported{"braw", "mp4", "mov", "mxf", "mkv", "avi", "webm", "wav", "flac", "mp3", "m4a", "aif", "aiff", "ogg"};
    QFileInfo single(folder);
    bool singlePending = single.isFile() && !single.isSymLink();
    QDirIterator files(singlePending ? QString{} : folder, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
    const auto runtime = brawRuntimeDirectory();
    std::unique_ptr<RawDecoder> raw;
    while (!cancel.load() && (singlePending || (!single.isFile() && files.hasNext()))) {
        QFileInfo file;
        if (singlePending) {
            singlePending = false;
            file = single;
        } else {
            files.next();
            file = files.fileInfo();
        }
        const auto extension = file.suffix().toLower();
        if (!supported.contains(extension)) continue;
        Clip clip;
        clip.path = file.absoluteFilePath();
        clip.bytes = file.size();
        if (extension == "braw") {
            clip.codec = "Blackmagic RAW";
            try {
                if (!raw) raw = std::make_unique<RawDecoder>(runtime);
                const auto info = raw->open(clip.path);
                clip.fps = info.fps;
                clip.duration = info.frames / info.fps;
                clip.size = info.size;
                clip.timecode = info.timecode;
                clip.camera = info.camera;
                const auto frame = raw->read(0, false).image;
                QBuffer buffer(&clip.thumbnail);
                if (!frame.isNull() && buffer.open(QIODevice::WriteOnly))
                    frame.scaled(160, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&buffer, "JPEG");
            } catch (const std::exception& error) {
                clip.note = QString::fromUtf8(error.what());
            }
        } else {
            conventional(clip, cancel);
        }
        result.append(std::move(clip));
    }
    return result;
}
