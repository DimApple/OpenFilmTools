#include "new_project.hpp"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir folder;
    if (!folder.isValid()) return 1;
    QFile existing(folder.filePath("Existing.openroll"));
    if (!existing.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return 1;
    existing.close();

    bool valid = false;
    QTimer::singleShot(0, &app, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) { app.closeAllWindows(); return; }
        auto* name = dialog->findChild<QLineEdit*>("projectName");
        auto* accept = dialog->findChild<QPushButton*>("createProject");
        if (!name || !accept || accept->isEnabled()) { dialog->reject(); return; }
        name->setText("Existing");
        if (accept->isEnabled()) { dialog->reject(); return; }
        name->setText("../Escaped");
        if (accept->isEnabled()) { dialog->reject(); return; }
        name->setText(" Desert Transit.openroll ");
        valid = accept->isEnabled();
        if (valid) accept->click();
        else dialog->reject();
    });
    const auto result = chooseNewProject(nullptr, folder.path());
    return valid && result == folder.filePath("Desert Transit.openroll") ? 0 : 1;
}
