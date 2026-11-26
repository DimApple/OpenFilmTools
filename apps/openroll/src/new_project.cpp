#include "new_project.hpp"
#include "theme.hpp"

#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

QString chooseNewProject(QWidget* parent, const QString& startingFolder)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("New OpenRoll project");
    auto* layout = new QVBoxLayout(&dialog);
    auto* details = new QFormLayout;
    layout->addLayout(details);

    auto* name = new QLineEdit(&dialog);
    name->setPlaceholderText("Project name");
    name->setObjectName("projectName");
    details->addRow("&Name", name);
    auto* location = new QLineEdit(startingFolder, &dialog);
    location->setAccessibleName("Save folder");
    auto* choose = new QPushButton("Browse…", &dialog);
    auto* locationRow = new QHBoxLayout;
    locationRow->addWidget(location, 1);
    locationRow->addWidget(choose);
    details->addRow("Save &folder", locationRow);
    auto* file = new QLabel(&dialog);
    file->setWordWrap(true);
    file->setTextFormat(Qt::PlainText);
    file->setTextInteractionFlags(Qt::TextSelectableByMouse);
    details->addRow("Project file", file);

    auto* warning = new QLabel(&dialog);
    warning->setTextFormat(Qt::PlainText);
    warning->setWordWrap(true);
    setAction(warning, "warning");
    layout->addWidget(warning);
    auto* actions = new QHBoxLayout;
    actions->addStretch();
    auto* cancel = new QPushButton("Cancel", &dialog);
    auto* create = new QPushButton("Create project", &dialog);
    create->setDefault(true);
    create->setObjectName("createProject");
    setAction(create, "primary");
    actions->addWidget(cancel);
    actions->addWidget(create);
    layout->addLayout(actions);

    QString path;
    auto validate = [&] {
        path.clear();
        auto text = name->text().trimmed();
        if (text.endsWith(".openroll", Qt::CaseInsensitive)) text.chop(9);
        const auto folder = QDir::cleanPath(location->text().trimmed());
        const QFileInfo target(folder);
        QString issue;
        if (text.isEmpty()) issue = "Enter a project name.";
        else if (text == "." || text == ".." || text.contains('/') || text.contains(QChar::Null))
            issue = "Use a name without path separators.";
        else if (location->text().trimmed().isEmpty() || !target.isDir() || !target.isWritable())
            issue = "Choose a writable folder.";
        else {
            path = QDir(target.absoluteFilePath()).filePath(text + ".openroll");
            if (QFileInfo::exists(path) || QFileInfo(path).isSymLink())
                issue = "That project file already exists. Choose a different name or open it.";
        }
        file->setText(path.isEmpty() ? "—" : path);
        warning->setText(issue);
        create->setEnabled(issue.isEmpty());
    };
    QObject::connect(name, &QLineEdit::textChanged, &dialog, validate);
    QObject::connect(location, &QLineEdit::textChanged, &dialog, validate);
    QObject::connect(choose, &QPushButton::clicked, &dialog, [&] {
        const auto chosen = QFileDialog::getExistingDirectory(&dialog, "Choose project folder", location->text());
        if (!chosen.isEmpty()) location->setText(chosen);
    });
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(create, &QPushButton::clicked, &dialog, [&] {
        validate();
        if (create->isEnabled()) dialog.accept();
    });
    validate();
    name->setFocus();
    dialog.resize(575, dialog.sizeHint().height());
    return dialog.exec() == QDialog::Accepted ? path : QString{};
}
