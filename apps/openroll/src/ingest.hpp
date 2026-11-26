#pragma once

#include <QStringList>
#include <atomic>
#include <functional>

struct IngestProgress {
    QString file;
    QString phase;
    qint64 fileDone = 0;
    qint64 fileSize = 0;
    qint64 done = 0;
    qint64 total = 0;
};

struct IngestResult {
    bool verified = false;
    QString message;
    QString reportDirectory;
};

struct IngestInventory { qint64 files; qint64 bytes; };
IngestInventory indexIngestSource(const QString& source, const std::atomic_bool& cancelled);

QStringList ingestOutputPaths(const QString& source, const QStringList& destinations, const QString& folderName);

IngestResult ingest(const QString& project, const QString& source, const QStringList& destinations,
                    const QString& reports, const std::atomic_bool& cancelled,
                    const std::function<void(const IngestProgress&)>& progress, const QString& folderName = {});
