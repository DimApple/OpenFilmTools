#include "ingest.hpp"

#include <QDir>
#include <QDateTime>
#include <QProcess>
#include <QStorageInfo>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

namespace {
void require(bool success, const char* message)
{
    if (!success) throw std::runtime_error(message);
}

void save(const QString& path, const QByteArray& data)
{
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "Could not create fixture");
    require(file.write(data) == data.size() && file.flush(), "Could not write fixture");
}

QByteArray contents(const QString& path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Could not open fixture or report");
    return file.readAll();
}

QString stage(const QString& target)
{
    const auto folders = QDir(target).entryList({".openroll-incomplete-*"}, QDir::Dirs | QDir::Hidden);
    require(folders.size() == 1, "Expected one retained staging folder");
    return QDir(target).filePath(folders.front());
}
}

int main(int argc, char** argv)
{
    try {
        QTemporaryDir tmp;
        require(tmp.isValid(), "Could not create fixture directory");
        const QString root = argc > 2 ? QString::fromLocal8Bit(argv[2]) : tmp.path();
        const QDir fixture(root);
        const auto source = fixture.filePath("CARD");
        const auto original = source + "/DCIM/clip,01.braw";
        const auto one = fixture.filePath("one");
        const auto two = fixture.filePath("two");
        const auto reports = fixture.filePath("reports");
        require(QDir().mkpath(source + "/DCIM/EMPTY") && QDir().mkpath(one) && QDir().mkpath(two),
                "Could not create fixture tree");
        save(original, "abc");
        save(source + "/.camera", "sidecar");
        const auto modified = QFileInfo(original).lastModified();
        std::atomic_bool cancel(false);
        const auto noProgress = [](const IngestProgress&) {};
        if (argc > 1 && QByteArray(argv[1]) == "--interrupt") {
            ingest("Interruption fixture", source, {one}, reports, cancel,
                   [](const IngestProgress& p) {
                if (p.phase == "Copying" && p.fileDone > 0) std::_Exit(23);
            });
            return 1;
        }
        const auto good = ingest("Test", source, {one, two}, reports, cancel, noProgress);
        require(good.verified, qPrintable(good.message));
        for (const auto& destination : {one, two}) {
            require(contents(destination + "/CARD/DCIM/clip,01.braw") == "abc", "Copied content differs");
            require(QFileInfo(destination + "/CARD/DCIM/EMPTY").isDir(), "Empty directory missing");
            require(contents(destination + "/CARD/.camera") == "sidecar", "Hidden file missing");
        }
        const auto text = contents(good.reportDirectory + "/transfer.txt");
        const auto csv = contents(good.reportDirectory + "/transfer.csv");
        require(text.contains("Final status: VERIFIED") && csv.contains("\"VERIFIED\",\"All destinations published\""),
                "Report lacks verified job status");
        require(csv.count("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 4,
                "Report does not contain independent SHA-256 values for both copies");

        const auto planned = ingestOutputPaths(source, {one, two}, "Card 02");
        require(planned == QStringList{one + "/Card 02", two + "/Card 02"}, "Copy paths do not match the chosen folder name");
        const auto named = ingest("Named offload", source, {one, two}, reports, cancel, noProgress, "Card 02");
        require(named.verified && contents(planned[0] + "/DCIM/clip,01.braw") == "abc" &&
                contents(planned[1] + "/DCIM/clip,01.braw") == "abc", "Custom copy folder was not verified");
        const auto invalid = ingest("Invalid offload", source, {one}, reports, cancel, noProgress, "../Escape");
        require(!invalid.verified && invalid.message.contains("without slashes") &&
                !QFileInfo::exists(fixture.filePath("Escape")), "A copy folder escaped the destination");

        const auto inventory = indexIngestSource(source, cancel);
        const auto single = indexIngestSource(original, cancel);
        require(inventory.files == 2 && inventory.bytes == 10 &&
                single.files == 1 && single.bytes == 3, "Dropped source inventory is incorrect");
        const auto singleCopy = ingest("Single file", original, {two}, reports, cancel, noProgress, "Single");
        require(singleCopy.verified && contents(two + "/Single/clip,01.braw") == "abc" &&
                !QFileInfo::exists(two + "/Single/.camera"), "Single source copied neighboring files");

        const auto existing = ingest("Repeat", source, {one}, reports, cancel, noProgress);
        require(!existing.verified && existing.message.contains("already exists") &&
                contents(one + "/CARD/DCIM/clip,01.braw") == "abc", "Collision handling failed");

        const auto corrupt = fixture.filePath("corrupt");
        require(QDir().mkpath(corrupt), "Could not create corruption fixture");
        const auto damaged = ingest("Corruption", source, {corrupt}, reports, cancel, [&](const IngestProgress& p) {
            if (p.phase == "Verifying" && p.fileDone == 0 && p.file == "DCIM/clip,01.braw")
                save(QDir(stage(corrupt)).filePath(p.file), "xyz");
        });
        require(!damaged.verified && damaged.message.contains("Checksum mismatch") &&
                !QFileInfo::exists(corrupt + "/CARD"), "Corrupted destination was published");
        require(contents(damaged.reportDirectory + "/transfer.csv").contains("CHECKSUM_MISMATCH"),
                "Checksum failure missing from report");

        const auto interrupted = fixture.filePath("interrupted");
        require(QDir().mkpath(interrupted), "Could not create interruption fixture");
        const auto stopped = ingest("Cancellation", source, {interrupted}, reports, cancel, [&](const IngestProgress& p) {
            if (p.phase == "Copying" && p.fileDone > 0) cancel.store(true);
        });
        require(!stopped.verified && stopped.message.contains("Cancelled") &&
                !QFileInfo::exists(interrupted + "/CARD"), "Cancelled transfer was published");
        stage(interrupted);
        require(contents(stopped.reportDirectory + "/transfer.txt").contains("Final status: INCOMPLETE"),
                "Cancelled job report does not show incomplete status");
        cancel.store(false);

        const auto overlap = ingest("Overlap", source, {source}, reports, cancel, noProgress);
        require(!overlap.verified && overlap.message.contains("overlaps"), "Destination inside source accepted");
        const auto unavailable = ingest("Unavailable", source, {fixture.filePath("missing")}, reports, cancel, noProgress);
        require(!unavailable.verified && unavailable.message.contains("unavailable"), "Missing destination accepted");
        require(contents(original) == "abc" && QFileInfo(original).lastModified() == modified,
                "Original media changed");
        const auto space = fixture.filePath("space");
        const auto big = fixture.filePath("BIG");
        require(QDir().mkpath(space) && QDir().mkpath(big), "Could not create disk-space fixture");
        QFile sparse(big + "/sparse.braw");
        require(sparse.open(QIODevice::WriteOnly) && sparse.resize(QStorageInfo(space).bytesAvailable() + 1),
                "Could not size the sparse test file");
        sparse.close();
        const auto insufficient = ingest("Disk space", big, {space}, reports, cancel, noProgress);
        require(!insufficient.verified && insufficient.message.contains("Insufficient") &&
                !QFileInfo::exists(space + "/BIG"), "Insufficient storage preflight failed");

        const auto race = fixture.filePath("race");
        require(QDir().mkpath(race), "Could not create publish collision fixture");
        const auto conflict = ingest("Publish collision", source, {race}, reports, cancel, [&](const IngestProgress& p) {
            if (p.phase == "Publishing verified copies") {
                require(QDir().mkpath(race + "/CARD"), "Could not create competing destination");
                save(race + "/CARD/existing", "keep");
            }
        });
        require(!conflict.verified && contents(race + "/CARD/existing") == "keep",
                "A competing destination was overwritten");
        stage(race);
        const auto crashRoot = fixture.filePath("crash");
        require(QDir().mkpath(crashRoot), "Could not create crash fixture");
        QProcess child;
        child.start(QString::fromLocal8Bit(argv[0]), {"--interrupt", crashRoot});
        require(child.waitForFinished(10000) && child.exitStatus() == QProcess::NormalExit &&
                child.exitCode() == 23, "Forced interruption process did not stop during copying");
        stage(crashRoot + "/one");
        require(!QFileInfo::exists(crashRoot + "/one/CARD"), "Interrupted transfer was published");
        const auto jobs = QDir(crashRoot + "/reports").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        require(jobs.size() == 1, "Interrupted job report missing");
        const auto crashReport = contents(crashRoot + "/reports/" + jobs.front() + "/transfer.txt");
        require(crashReport.contains("Initial status: INCOMPLETE") &&
                !crashReport.contains("Final status: VERIFIED"), "Interrupted report looks successful");
        std::cout << "Ingest: multiple copies, SHA-256, original paths, reports, collisions, corruption, cancellation, forced interruption, overlap, missing destination and storage preflight passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
