#include "ingest_widget.hpp"
#include "project.hpp"

#include <QApplication>
#include <QDir>
#include <QDialog>
#include <QTreeWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QMimeData>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid()) return 1;
    QStandardPaths::setTestModeEnabled(true);
    qputenv("XDG_DATA_HOME", QFile::encodeName(temp.filePath("data")));
    const auto folder = temp.filePath("Camera");
    if (!QDir().mkdir(folder)) return 1;
    const auto clip = folder + "/test.mov";
    QFile fixture(clip);
    if (!fixture.open(QIODevice::WriteOnly)) return 1;
    fixture.close();
    auto path = temp.filePath("Test.openroll");
    Project project(path, true);
    const auto otherPath = temp.filePath("Other.openroll");
    Project other(otherPath, true);
    QString refreshed;
    QTimer promptTimer;
    if (app.arguments().contains("--no-project")) {
        path.clear();
        QObject::connect(&promptTimer, &QTimer::timeout, [&] {
            auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!dialog) return;
            for (auto* button : dialog->buttons())
                if (button->text() == "Open project…") { promptTimer.stop(); button->click(); break; }
        });
        promptTimer.start(5);
    }
    IngestWidget widget([&] { return path; }, [&](const QString& linked) { refreshed = linked; },
                        [&](bool create) { if (!create) path = project.path(); });
    widget.show();
    auto* target = widget.findChild<QLabel*>("sourceDrop");
    if (!target) return 1;
    auto* backup = widget.findChild<QPushButton*>("openBackup");
    auto* dialog = widget.findChild<QDialog*>("verifiedBackup");
    auto* destinations = widget.findChild<QTreeWidget*>("ingestDestinations");
    if (!backup || !dialog || !destinations || backup->isEnabled() || dialog->isVisible() ||
        destinations->window() != dialog) return 1;
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(app.arguments().contains("--single-file") ? clip : folder)});
    QDragEnterEvent enter(QPoint(5, 5), Qt::LinkAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &enter);
    QDragMoveEvent move(QPoint(8, 5), Qt::LinkAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &move);
    QDropEvent drop(QPointF(5, 5), Qt::LinkAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &drop);
    QApplication::processEvents();
    path = otherPath;
    QElapsedTimer deadline;
    deadline.start();
    while (refreshed.isEmpty() && deadline.elapsed() < 10000) {
        QApplication::processEvents();
        QThread::msleep(5);
    }
    if (!backup->isEnabled() || dialog->isVisible()) return 1;
    backup->click();
    if (!dialog->isVisible()) return 1;
    if (app.arguments().size() == 1) {
        const auto backupRoot = temp.filePath("Backup");
        if (!QDir().mkdir(backupRoot)) return 1;
        path = project.path();
        auto* destination = new QTreeWidgetItem(destinations, {backupRoot});
        destinations->setCurrentItem(destination);
        QPushButton* start = nullptr;
        QPushButton* report = nullptr;
        for (auto* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == "Start verified copy") start = button;
            if (button->text() == "Open report folder") report = button;
        }
        if (!start || !report || !start->isEnabled()) return 1;
        start->click();
        deadline.restart();
        while (!report->isVisible() && deadline.elapsed() < 5000) {
            QApplication::processEvents();
            QThread::msleep(5);
        }
        const auto records = project.transfers();
        if (!report->isVisible() || !QFile::exists(backupRoot + "/Camera/test.mov") ||
            records.size() != 1 || records.front().value(1) != "Verified" || !other.transfers().isEmpty()) return 1;
    }
    dialog->reject();
    return !dialog->isVisible() && enter.isAccepted() && move.isAccepted() && drop.isAccepted() && refreshed == project.path() &&
           project.clipPaths() == QStringList{clip} && other.clipPaths().isEmpty() &&
           QFile::exists(clip) ? 0 : 1;
}
