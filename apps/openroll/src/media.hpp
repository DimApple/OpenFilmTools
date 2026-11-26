#pragma once
#include "project.hpp"
#include <QThreadPool>
#include <QWidget>
#include <functional>
#include <memory>

class Playback;
class QLabel;
class QLineEdit;
class QListWidget;
class QTreeWidget;
class QPushButton;

class MediaWorkspace : public QWidget {
public:
    explicit MediaWorkspace(Playback* playback,
                            std::function<void(const QString&)> projectChanged, QWidget* parent = nullptr);
    ~MediaWorkspace() override;
    QString projectPath() const { return project ? project->path() : QString{}; }
    QString projectName() const { return project ? project->name() : QString{}; }
    void chooseProject(bool create);
    void closeProject();
    void openProjectFile(const QString& file, bool create = false);
    void setMonitorMode(bool enabled);
    void openClip(const QString& path);

    void refreshMedia();
    void linkMedia(const QStringList& sources);

private:
    void filterMedia();
    void showMetadata(const Clip* clip);
    std::unique_ptr<Project> project;
    std::function<void(const QString&)> projectChanged;
    std::function<void(const QStringList&)> scanSources;
    QThreadPool worker;
    std::atomic_bool cancelled{false};
    bool scanning = false;
    Playback* viewer;
    QWidget* sourcePanel;
    QWidget* inspector;
    QWidget* strip;
    QLabel* status;
    QTreeWidget* folders;
    QListWidget* media;
    QTreeWidget* metadata;
    QPushButton* add;
    QLineEdit* filter;
    QLabel* clipCount;
    QList<Clip> clips;
    QString selectedPath;
};
