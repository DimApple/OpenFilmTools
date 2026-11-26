#pragma once

#include <QByteArray>
#include <QList>
#include <QSize>
#include <QSqlDatabase>
#include <QString>
#include <atomic>

struct Clip {
    QString id;
    QString path;
    QString codec;
    QString timecode;
    QString camera;
    QString note;
    double duration = -1;
    double fps = 0;
    QSize size;
    qint64 bytes = 0;
    QByteArray thumbnail;
};

QList<Clip> scanMedia(const QString& folder, const std::atomic_bool& cancelled);

class Project {
public:
    Project(const QString& path, bool create);
    ~Project();
    Project(const Project&) = delete;
    Project& operator=(const Project&) = delete;

    QString name() const;
    QString path() const { return database.databaseName(); }
    QString id() const;
    QString lastScanFolder() const;
    QStringList sourceFolders() const;
    QList<Clip> clips() const;
    QStringList clipPaths() const;
    void saveScan(const QString& folder, const QList<Clip>& clips);
    void recordTransfer(const QString& reportDirectory, bool verified, const QString& message);
    QList<QStringList> transfers() const;
    QList<QByteArray> transferReports(const QString& id) const;

private:
    QString connection;
    QSqlDatabase database;
};
