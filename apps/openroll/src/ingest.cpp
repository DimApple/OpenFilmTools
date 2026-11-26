#include "ingest.hpp"
#include "native_files.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStorageInfo>
#include <QUuid>

#include <filesystem>
#include <limits>
#include <stdexcept>
#include <vector>
#include <cerrno>
#include <cstring>
#ifdef Q_OS_WIN
#include <io.h>
#endif

namespace {
namespace fs = std::filesystem;

struct Entry { QString relative; qint64 size; };
struct Destination { QString root; QString final; QString stage; };

[[noreturn]] void fail(const QString& message)
{
    throw std::runtime_error(message.toUtf8().constData());
}

void checkCancel(const std::atomic_bool& cancel)
{
    if (cancel.load()) fail("Cancelled. Transfer is incomplete.");
}

QString csv(QString field)
{
    field.replace('"', "\"\"");
    return '"' + field + '"';
}

void append(QFile& file, const QString& line)
{
    const auto bytes = (line + '\n').toUtf8();
    if (file.write(bytes) != bytes.size() || !file.flush())
        fail("Could not write report " + file.fileName() + ": " + file.errorString());
}

void syncFile(QFile& file)
{
#ifdef Q_OS_WIN
    if (::_commit(file.handle()) != 0)
#else
    if (::fsync(file.handle()) != 0)
#endif
        fail("Could not sync " + file.fileName() + ": " + QString::fromLocal8Bit(std::strerror(errno)));
}

void syncDirectory(const QString& path)
{
#ifdef Q_OS_WIN
    // Files are flushed individually. The final move requests write-through.
    // Windows offers no equivalent to POSIX directory fsync for these copies.
    Q_UNUSED(path);
#else
    const int descriptor = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (descriptor < 0) fail("Could not open directory for sync: " + path);
    const int status = ::fsync(descriptor);
    const int error = errno;
    ::close(descriptor);
    if (status != 0)
        fail("Could not sync " + path + ": " + QString::fromLocal8Bit(std::strerror(error)));
#endif
}

bool inside(const QString& path, const QString& parent)
{
    const QString prefix = parent.endsWith('/') ? parent : parent + '/';
#ifdef Q_OS_WIN
    return path.compare(parent, Qt::CaseInsensitive) == 0 || path.startsWith(prefix, Qt::CaseInsensitive);
#else
    return path == parent || path.startsWith(prefix);
#endif
}

void openRead(QFile& file)
{
    if (!file.open(QIODevice::ReadOnly))
        fail("Could not read " + file.fileName() + ": " + file.errorString());
}

void publish(const Destination& dest)
{
    nativeFiles::publish(dest.stage, dest.final);
    syncDirectory(dest.root);
}
fs::path nativePath(const QString& path)
{
#ifdef Q_OS_WIN
    return fs::path(path.toStdWString());
#else
    return fs::path(QFile::encodeName(path).constData());
#endif
}

QString displayPath(const fs::path& path)
{
#ifdef Q_OS_WIN
    return QDir::fromNativeSeparators(QString::fromStdWString(path.native()));
#else
    const auto name = QFile::decodeName(path.c_str());
    if (QFile::encodeName(name).toStdString() != path.native())
        fail("A source filename cannot be represented on this system.");
    return name;
#endif
}

struct SourceIndex {
    std::vector<Entry> files;
    QStringList directories;
    qint64 totalBytes;
};

SourceIndex scanSource(const QString& root, const QString& single, const std::atomic_bool& cancelled)
{
    std::vector<Entry> files;
    QStringList directories;
    qint64 totalBytes = 0;
    const auto nativeRoot = nativePath(root);
    auto add = [&](const fs::directory_entry& entry) {
        checkCancel(cancelled);
        const auto path = displayPath(entry.path().lexically_relative(nativeRoot));
#ifdef Q_OS_WIN
        const auto attributes = GetFileAttributesW(entry.path().c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            fail("Source entry is unavailable or is a reparse point: " + path);
#endif
        const auto status = entry.symlink_status();
        if (fs::is_directory(status)) {
            directories.append(path);
        } else if (fs::is_regular_file(status)) {
            const auto bytes = entry.file_size();
            if (bytes > quint64(std::numeric_limits<qint64>::max() - totalBytes))
                fail("Source size exceeds the supported limit.");
            files.push_back({path, qint64(bytes)});
            totalBytes += qint64(bytes);
        } else {
            fail("Source contains a symbolic link or special file: " + path);
        }
    };
    if (single.isEmpty()) {
        for (const auto& entry : fs::recursive_directory_iterator(nativeRoot)) add(entry);
    } else {
        add(fs::directory_entry(nativeRoot / nativePath(single)));
    }
    if (files.empty()) fail("The source folder contains no regular media files.");
    return {std::move(files), std::move(directories), totalBytes};
}

QString sourceRoot(const QString& source)
{
    const QFileInfo input(source);
    if (source.isEmpty() || input.isSymLink() || (!input.isFile() && !input.isDir()))
        fail("Choose a local file or folder.");
    return input.isFile() ? QFileInfo(input.canonicalFilePath()).absolutePath() : input.canonicalFilePath();
}
}

IngestInventory indexIngestSource(const QString& source, const std::atomic_bool& cancelled)
{
    const auto root = sourceRoot(source);
    const QFileInfo input(source);
    const auto index = scanSource(root, input.isFile() ? input.fileName() : QString{}, cancelled);
    return {qint64(index.files.size()), index.totalBytes};
}

QStringList ingestOutputPaths(const QString& source, const QStringList& destinations, const QString& folderName)
{
    const auto root = sourceRoot(source);
    const QFileInfo input(source);
    const auto sourceName = input.isFile() ? input.completeBaseName() : QFileInfo(root).fileName();
    if (root.isEmpty() || sourceName.isEmpty()) fail("Choose a source below the filesystem root.");
    if (destinations.isEmpty()) fail("Add at least one destination.");
    const auto name = folderName.isEmpty() ? sourceName : folderName;
    if (name == "." || name == ".." || name.contains('/') || name.contains(QChar::Null) ||
        name.contains('\n') || name.contains('\r'))
        fail("Enter a folder name without slashes or line breaks.");
    QStringList paths;
#ifdef Q_OS_WIN
    if (name.contains('\\') || name.contains(':'))
        fail("Copy folder name must be a single Windows folder name.");
#endif
    QSet<QString> used;
    for (const auto& chosen : destinations) {
        const auto destination = QFileInfo(chosen).canonicalFilePath();
        if (destination.isEmpty() || !QFileInfo(destination).isDir())
            fail("Destination unavailable: " + chosen);
        const auto final = QDir(destination).filePath(name);
        if ((!input.isFile() && (inside(destination, root) || inside(root, final))) || used.contains(destination))
            fail("Destination overlaps the source or another selection: " + chosen);
        if (QFileInfo::exists(final) || QFileInfo(final).isSymLink())
            fail("Destination already exists: " + final);
        if (!QFileInfo(destination).isWritable())
            fail("Destination is not writable: " + destination);
        used.insert(destination);
        paths.append(final);
    }
    return paths;
}

IngestResult ingest(const QString& project, const QString& source, const QStringList& destinations,
                    const QString& reports, const std::atomic_bool& cancelled,
                    const std::function<void(const IngestProgress&)>& progress, const QString& folderName)
{
    IngestResult result;
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto started = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QFile txt;
    QFile table;
    std::vector<Destination> targets;
    QString activeFile;
    QString activeDestination;
    try {
        const auto root = sourceRoot(source);
        if (destinations.isEmpty()) fail("Add at least one destination.");

        // Resolve existing ancestors too, so a report-directory symlink cannot target the source.
        const auto reportRoot = fs::weakly_canonical(nativePath(QDir(reports).absolutePath()));
        const auto reportPath = displayPath(reportRoot);
        if (!QFileInfo(source).isFile() && inside(reportPath, root)) fail("Reports must be stored outside the source folder.");
        result.reportDirectory = QDir(reportPath).filePath(id);
        if (!QDir().mkpath(result.reportDirectory)) fail("Could not create the job report directory.");
        txt.setFileName(QDir(result.reportDirectory).filePath("transfer.txt"));
        table.setFileName(QDir(result.reportDirectory).filePath("transfer.csv"));
        if (!txt.open(QIODevice::WriteOnly | QIODevice::NewOnly) ||
            !table.open(QIODevice::WriteOnly | QIODevice::NewOnly)) fail("Could not create transfer reports.");
        const auto dateAndProject = csv(id) + ',' + csv(project) + ',' + csv(started);
        append(txt, "OpenRoll verified ingest\nJob: " + id + "\nProject: " + project +
                    "\nStarted (UTC): " + started + "\nSHA-256\nSource: " + source +
                    "\nInitial status: INCOMPLETE until the final VERIFIED line is recorded.");
        append(table, "job_id,project,started_utc,source,destination,file,bytes,algorithm,source_hash,destination_hash,status,details");
        auto row = [&](const QString& dest, const QString& file, const QString& bytes, const QString& sourceHash,
                       const QString& destHash, const QString& status, const QString& details) {
            append(table, dateAndProject + ',' + csv(source) + ',' + csv(dest) + ',' + csv(file) + ',' +
                          csv(bytes) + ",SHA-256," + csv(sourceHash) + ',' + csv(destHash) + ',' +
                          csv(status) + ',' + csv(details));
        };
        row({}, {}, {}, {}, {}, "INCOMPLETE", "Job started");
        syncFile(txt);
        syncFile(table);
        syncDirectory(result.reportDirectory);
        syncDirectory(reportPath);
        for (const auto& final : ingestOutputPaths(source, destinations, folderName)) {
            const auto destination = QFileInfo(final).absolutePath();
            const auto stage = QDir(destination).filePath(".openroll-incomplete-" + id);
            targets.push_back({destination, final, stage});
            append(txt, "Destination: " + final + "\nIncomplete staging: " + stage);
        }

        progress({{}, "Scanning source"});
        const auto [files, directories, totalBytes] = scanSource(
            root, QFileInfo(source).isFile() ? QFileInfo(source).fileName() : QString{}, cancelled);
        if (totalBytes > std::numeric_limits<qint64>::max() / destinations.size() / 2)
            fail("Transfer size exceeds the supported limit.");

        for (const auto& target : targets) {
            const QStorageInfo storage(target.root);
            qint64 copies = 0;
            for (const auto& other : targets)
                if (QStorageInfo(other.root).rootPath() == storage.rootPath()) ++copies;
            if (!storage.isValid() || !storage.isReady() || storage.isReadOnly() ||
                storage.bytesAvailable() < totalBytes * copies)
                fail("Insufficient or unavailable storage: " + target.root);
            if (!QDir().mkdir(target.stage)) fail("Could not create staging folder: " + target.stage);
            for (const auto& directory : directories) {
                checkCancel(cancelled);
                if (!QDir().mkpath(QDir(target.stage).filePath(directory)))
                    fail("Could not create destination directory: " + directory);
            }
        }

        IngestProgress state;
        state.total = totalBytes * destinations.size() * 2;
        QByteArray buffer(1024 * 1024, Qt::Uninitialized);
        for (const auto& entry : files) {
            activeFile = entry.relative;
            activeDestination.clear();
            const auto from = QDir(root).filePath(entry.relative);
            if (QFileInfo(from).canonicalFilePath() != from || !QFileInfo(from).isFile())
                fail("Source entry changed: " + from);
            QFile input(from);
            openRead(input);
            const auto expected = QFileInfo(from).lastModified();
            QCryptographicHash sourceHash(QCryptographicHash::Sha256);
            for (const auto& target : targets) {
                activeDestination = target.final;
                checkCancel(cancelled);
                if (!input.seek(0)) fail("Could not rewind source " + from);
                const auto to = QDir(target.stage).filePath(entry.relative);
                QFile output(to);
                if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly))
                    fail("Could not create " + to + ": " + output.errorString());

                state.file = entry.relative;
                state.fileSize = entry.size;
                auto pump = [&](QFile& read, QFile* write, QCryptographicHash& hash, const QString& phase) {
                    state.phase = phase;
                    state.fileDone = 0;
                    progress(state);
                    while (true) {
                        checkCancel(cancelled);
                        const auto n = read.read(buffer.data(), buffer.size());
                        if (n < 0) fail("Read failed for " + read.fileName() + ": " + read.errorString());
                        if (n == 0) break;
                        if (n > entry.size - state.fileDone)
                            fail("File size changed during transfer: " + read.fileName());
                        hash.addData(QByteArrayView(buffer.constData(), n));
                        if (write && write->write(buffer.constData(), n) != n)
                            fail("Write failed for " + write->fileName() + ": " + write->errorString());
                        state.done += n;
                        state.fileDone += n;
                        progress(state);
                    }
                    if (state.fileDone != entry.size)
                        fail("Unexpected file size: " + read.fileName());
                };

                QCryptographicHash copiedHash(QCryptographicHash::Sha256);
                pump(input, &output, copiedHash, "Copying");
                if (!output.flush()) fail("Could not flush " + to + ": " + output.errorString());
                syncFile(output);
                output.close();
                if (&target == &targets.front()) {
                    sourceHash = std::move(copiedHash);
                } else if (copiedHash.result() != sourceHash.result()) {
                    fail("Source bytes changed during transfer: " + from);
                }
                if (QFileInfo(from).lastModified() != expected)
                    fail("Source modification time changed during transfer: " + from);

                QFile verify(to);
                openRead(verify);
                QCryptographicHash destinationHash(QCryptographicHash::Sha256);
                pump(verify, nullptr, destinationHash, "Verifying");
                const QString sha = QString::fromLatin1(sourceHash.result().toHex());
                const QString destSha = QString::fromLatin1(destinationHash.result().toHex());
                const bool match = sha == destSha;
                row(target.final, entry.relative, QString::number(entry.size), sha, destSha,
                    match ? "STAGED_VERIFIED" : "CHECKSUM_MISMATCH", {});
                append(txt, target.final + " | " + entry.relative + " | " + QString::number(entry.size) +
                            " bytes | source " + sha + " | destination " + destSha +
                            (match ? " | STAGED_VERIFIED" : " | CHECKSUM_MISMATCH"));
                if (!match) fail("Checksum mismatch: " + to);
            }
        }

        activeFile.clear();
        activeDestination.clear();
        checkCancel(cancelled);
        state.phase = "Publishing verified copies";
        state.file.clear();
        progress(state);
        for (const auto& target : targets) {
            activeDestination = target.final;
            for (auto it = directories.crbegin(); it != directories.crend(); ++it)
                syncDirectory(QDir(target.stage).filePath(*it));
            syncDirectory(target.stage);
            publish(target);
            append(txt, "Published verified destination: " + target.final);
            row(target.final, {}, {}, {}, {}, "DESTINATION_PUBLISHED", {});
        }
        row({}, {}, {}, {}, {}, "VERIFIED", "All destinations published");
        syncFile(table);
        append(txt, "Final status: VERIFIED. All destinations published.");
        syncFile(txt);
        syncDirectory(result.reportDirectory);
        result.verified = true;
        result.message = "All destinations verified.";
    } catch (const std::exception& error) {
        result.message = QString::fromUtf8(error.what());
        try {
            if (txt.isOpen()) {
                append(txt, "Final status: INCOMPLETE. " + result.message);
                syncFile(txt);
            }
            if (table.isOpen()) {
                append(table, csv(id) + ',' + csv(project) + ',' + csv(started) + ',' + csv(source) +
                    ',' + csv(activeDestination) + ',' + csv(activeFile) + ",,SHA-256,,,INCOMPLETE," + csv(result.message));
                syncFile(table);
            }
        } catch (const std::exception& reportError) {
            result.message += " Report error: " + QString::fromUtf8(reportError.what());
        }
    }
    return result;
}
