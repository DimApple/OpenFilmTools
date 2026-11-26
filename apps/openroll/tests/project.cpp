#include "ingest.hpp"
#include "project.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

void require(bool success, const char* message)
{
    if (!success) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir tmp;
        require(tmp.isValid(), "Cannot create test workspace");
        const auto source = tmp.filePath("Camera");
        const auto target = tmp.filePath("Backup");
        require(QDir().mkdir(source) && QDir().mkdir(target), "Cannot create fixture folders");

        QProcess ffmpeg;
        ffmpeg.setProcessChannelMode(QProcess::MergedChannels);
        ffmpeg.start("ffmpeg", {"-nostdin", "-v", "error", "-f", "lavfi", "-i",
                     "testsrc2=size=320x180:rate=25", "-t", "1", "-c:v", "mpeg4",
                     "-threads", "1", source + "/sample.mp4"});
        require(ffmpeg.waitForFinished(15000) && ffmpeg.exitCode() == 0,
                qPrintable(QString::fromUtf8("Could not generate the FFmpeg fixture. " + ffmpeg.readAll())));
        const auto arguments = app.arguments();
        const bool includeRaw = arguments.size() == 3;
        if (includeRaw) {
            require(QFile::copy(arguments[1], source + "/sample.braw"), "Cannot copy BRAW fixture");
            qputenv("OPENROLL_BRAW_RUNTIME", QFile::encodeName(arguments[2]));
        }
        const auto database = tmp.filePath("My Test.openroll");
        QString projectId;
        QString reportDirectory;
        QStringList clipIds;
        const std::atomic_bool cancel(false);
        {
            Project p(database, true);
            require(p.name() == "My Test" && !p.id().isEmpty(), "Project identity is missing");
            projectId = p.id();
            const auto scanned = scanMedia(source, cancel);
            require(scanned.size() == (includeRaw ? 2 : 1), "Scan did not find expected clips");
            for (const auto& clip : scanned) {
                require(!clip.thumbnail.isEmpty() && clip.bytes > 0 && clip.duration > 0,
                        qPrintable("Metadata or thumbnail missing: " + clip.path + " " + clip.note));
                if (clip.path.endsWith(".mp4"))
                    require(clip.codec == "mpeg4" && clip.fps == 25 && clip.size == QSize(320, 180),
                            "MP4 metadata differs from fixture");
                else
                    require(clip.codec == "Blackmagic RAW" && clip.size == QSize(4608, 2592),
                            "BRAW metadata differs from fixture");
            }
            p.saveScan(source, scanned);
            for (const auto& clip : p.clips()) clipIds << clip.id;
            p.saveScan(source, scanned);
            QStringList after;
            for (const auto& clip : p.clips()) after << clip.id;
            require(clipIds == after, "Rescan changed clip IDs or added duplicates");
            const auto job = ingest(p.name(), source, {target}, tmp.filePath("reports"), cancel,
                                   [](const IngestProgress&) {});
            require(job.verified, qPrintable(job.message));
            reportDirectory = job.reportDirectory;
            p.recordTransfer(job.reportDirectory, job.verified, job.message);
            p.recordTransfer(job.reportDirectory, job.verified, job.message);
            p.recordTransfer({}, false, "An incomplete transfer.");
        }
        require(QDir(reportDirectory).removeRecursively(), "Could not remove original test report files");
        Project reopened(database, false);
        require(reopened.id() == projectId && reopened.lastScanFolder() == source,
                "Project identity or settings did not survive reopen");
        QStringList reopenedIds;
        for (const auto& clip : reopened.clips()) reopenedIds << clip.id;
        require(clipIds == reopenedIds, "Clip IDs did not survive reopen");
        const auto history = reopened.transfers();
        require(history.size() == 2 && history[0].size() == 5 &&
                ((history[0][1] == "Verified" && history[1][1] == "Incomplete") ||
                 (history[1][1] == "Verified" && history[0][1] == "Incomplete")),
                "Transfer history or verification status did not survive reopen");
        const auto saved = reopened.transferReports(QFileInfo(reportDirectory).fileName());
        require(saved.size() == 2 && saved[0].contains("Final status: VERIFIED") &&
                saved[1].contains("SHA-256"), "Saved report contents did not survive reopen");
        std::cout << "Media metadata, thumbnails, project reopen, persistent IDs, settings and transfer history passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
