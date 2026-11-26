#pragma once
#include <QThreadPool>
#include <QWidget>
#include <atomic>
#include <functional>

class QListWidget;
class QLabel;
class QShowEvent;

class ProxyWidget final : public QWidget {
public:
    explicit ProxyWidget(std::function<QString()> currentProject, QWidget* parent = nullptr);
    ~ProxyWidget() override;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refreshProjectClips();
    std::function<QString()> currentProject;
    QListWidget* queue;
    QLabel* sourceStatus;
    QString displayedProject;
    bool exporting = false;
    QThreadPool worker;
    std::atomic_bool cancelled{false};
};
