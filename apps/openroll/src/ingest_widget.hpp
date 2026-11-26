#pragma once
#include <QThreadPool>
#include <QWidget>
#include <atomic>
#include <functional>

class IngestWidget : public QWidget {
public:
    IngestWidget(std::function<QString()> projectPath, std::function<void(const QString&)> mediaLinked, std::function<void(bool)> chooseProject, QWidget* parent = nullptr);
    ~IngestWidget() override;
    std::function<void(const QString&)> indexSource;

private:
    std::function<QString()> projectPath;
    std::atomic_bool cancelled{false};
    QThreadPool worker;
    QString reportDirectory;
};
