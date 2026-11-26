#include "project.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QUuid>
#include <stdexcept>

namespace {
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

void check(bool ok, const QString& error)
{
    if (!ok) throw std::runtime_error(error.toStdString());
}

QSqlQuery run(const QSqlDatabase& db, const QString& sql, const QVariantList& values = {})
{
    QSqlQuery q(db);
    check(q.prepare(sql), q.lastError().text());
    for (const auto& value : values) q.addBindValue(value.isNull() && value.metaType().id() == QMetaType::QString
                                                       ? QVariant(QStringLiteral("")) : value);
    check(q.exec(), q.lastError().text());
    return q;
}

QByteArray readReport(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray{};
}
}

Project::Project(const QString& path, bool create) : connection(uuid())
{
    const QFileInfo f(path);
    if (create) {
        QFile reserve(f.absoluteFilePath());
        check(reserve.open(QIODevice::WriteOnly | QIODevice::NewOnly), reserve.errorString());
    } else {
        check(f.isFile(), "Project file is unavailable.");
    }
    try {
        database = QSqlDatabase::addDatabase("QSQLITE", connection);
        database.setDatabaseName(f.absoluteFilePath());
        database.setConnectOptions("QSQLITE_BUSY_TIMEOUT=1200");
        check(database.open(), database.lastError().text());
        run(database, "PRAGMA foreign_keys=ON");
        if (create) {
            check(database.transaction(), database.lastError().text());
            run(database, "PRAGMA application_id=1330794316");
            run(database, "PRAGMA user_version=1");
            run(database, "CREATE TABLE project(id TEXT PRIMARY KEY, name TEXT NOT NULL)");
            run(database, "INSERT INTO project VALUES(?,?)", {uuid(), f.completeBaseName()});
            run(database, "CREATE TABLE sources(path TEXT PRIMARY KEY)");
            run(database, "CREATE TABLE settings(key TEXT PRIMARY KEY, value TEXT NOT NULL)");
            run(database, "CREATE TABLE clips(id TEXT PRIMARY KEY, kind TEXT NOT NULL, codec TEXT, timecode TEXT, camera TEXT, note TEXT, duration REAL, fps REAL, width INTEGER, height INTEGER, thumbnail BLOB)");
            run(database, "CREATE TABLE files(path TEXT PRIMARY KEY, source TEXT REFERENCES sources(path), bytes INTEGER)");
            run(database, "CREATE TABLE clip_files(clip_id TEXT REFERENCES clips(id), file_path TEXT REFERENCES files(path), ordinal INTEGER, PRIMARY KEY(clip_id, ordinal))");
            run(database, "CREATE UNIQUE INDEX main_file ON clip_files(file_path) WHERE ordinal=0");
            run(database, "CREATE TABLE transfers(id TEXT PRIMARY KEY, recorded_utc TEXT, directory TEXT, verified INTEGER, message TEXT, txt BLOB, csv BLOB)");
            check(database.commit(), database.lastError().text());
        } else {
            auto type = run(database, "PRAGMA application_id");
            auto version = run(database, "PRAGMA user_version");
            check(type.next() && type.value(0).toInt() == 1330794316 &&
                  version.next() && version.value(0).toInt() == 1 && !id().isEmpty(),
                  "This file is not a supported OpenRoll project.");
        }
    } catch (...) {
        database = {};
        QSqlDatabase::removeDatabase(connection);
        if (create) QFile::remove(f.absoluteFilePath());
        throw;
    }
}

Project::~Project()
{
    database = {};
    QSqlDatabase::removeDatabase(connection);
}

QString Project::name() const
{
    auto q = run(database, "SELECT name FROM project");
    return q.next() ? q.value(0).toString() : QString{};
}

QString Project::id() const
{
    auto q = run(database, "SELECT id FROM project");
    return q.next() ? q.value(0).toString() : QString{};
}

QString Project::lastScanFolder() const
{
    auto q = run(database, "SELECT value FROM settings WHERE key='last_scan_folder'");
    return q.next() ? q.value(0).toString() : QString{};
}

QList<Clip> Project::clips() const
{
    QList<Clip> result;
    auto q = run(database, "SELECT c.id,f.path,c.codec,c.timecode,c.camera,c.note,c.duration,c.fps,"
                          "c.width,c.height,f.bytes,c.thumbnail FROM clips c JOIN clip_files m "
                          "ON c.id=m.clip_id JOIN files f ON m.file_path=f.path WHERE m.ordinal=0 ORDER BY f.path");
    while (q.next()) {
        result.append({q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),
                       q.value(3).toString(), q.value(4).toString(), q.value(5).toString(),
                       q.value(6).toDouble(), q.value(7).toDouble(), QSize(q.value(8).toInt(), q.value(9).toInt()),
                       q.value(10).toLongLong(), q.value(11).toByteArray()});
    }
    return result;
}

QStringList Project::clipPaths() const
{
    QStringList paths;
    auto query = run(database, "SELECT file_path FROM clip_files WHERE ordinal=0 ORDER BY file_path");
    while (query.next()) paths.append(query.value(0).toString());
    return paths;
}

void Project::saveScan(const QString& folder, const QList<Clip>& clips)
{
    check(database.transaction(), database.lastError().text());
    try {
        run(database, "INSERT OR IGNORE INTO sources VALUES(?)", {folder});
        for (const auto& clip : clips) {
            auto lookup = run(database, "SELECT clip_id FROM clip_files WHERE file_path=? AND ordinal=0", {clip.path});
            const auto clipId = lookup.next() ? lookup.value(0).toString() : uuid();
            lookup.finish();
            run(database, "INSERT OR IGNORE INTO clips(id,kind) VALUES(?, 'single')", {clipId});
            run(database, "INSERT INTO files VALUES(?,?,?) ON CONFLICT(path) DO UPDATE SET bytes=excluded.bytes",
                {clip.path, folder, clip.bytes});
            run(database, "INSERT OR IGNORE INTO clip_files VALUES(?,?,0)", {clipId, clip.path});
            run(database, "UPDATE clips SET codec=?,timecode=?,camera=?,note=?,duration=?,fps=?,width=?,height=?,thumbnail=? WHERE id=?",
                {clip.codec, clip.timecode, clip.camera, clip.note, clip.duration, clip.fps,
                 clip.size.width(), clip.size.height(), clip.thumbnail, clipId});
        }
        run(database, "INSERT OR REPLACE INTO settings VALUES('last_scan_folder',?)", {folder});
        check(database.commit(), database.lastError().text());
    } catch (...) {
        database.rollback();
        throw;
    }
}

void Project::recordTransfer(const QString& directory, bool verified, const QString& message)
{
    const auto id = directory.isEmpty() ? uuid() : QFileInfo(directory).fileName();
    run(database, "INSERT OR REPLACE INTO transfers VALUES(?,?,?,?,?,?,?)",
        {id, QDateTime::currentDateTimeUtc().toString(Qt::ISODate), directory, verified, message,
         directory.isEmpty() ? QByteArray{} : readReport(QDir(directory).filePath("transfer.txt")),
         directory.isEmpty() ? QByteArray{} : readReport(QDir(directory).filePath("transfer.csv"))});
}

QList<QStringList> Project::transfers() const
{
    QList<QStringList> result;
    auto q = run(database, "SELECT recorded_utc,verified,message,directory,id FROM transfers ORDER BY recorded_utc DESC");
    while (q.next())
        result.append({q.value(0).toString(), q.value(1).toBool() ? "Verified" : "Incomplete",
                       q.value(2).toString(), q.value(3).toString(), q.value(4).toString()});
    return result;
}


QList<QByteArray> Project::transferReports(const QString& id) const
{
    auto q = run(database, "SELECT txt,csv FROM transfers WHERE id=?", {id});
    return q.next() ? QList<QByteArray>{q.value(0).toByteArray(), q.value(1).toByteArray()}
                    : QList<QByteArray>{};
}

QStringList Project::sourceFolders() const
{
    QStringList result;
    auto q = run(database, "SELECT path FROM sources ORDER BY path");
    while (q.next()) result.append(q.value(0).toString());
    return result;
}
