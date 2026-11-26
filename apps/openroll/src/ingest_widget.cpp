#include "ingest_widget.hpp"
#include "theme.hpp"
#include "ingest.hpp"
#include "project.hpp"

#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLocale>
#include <QMimeData>
#include <QMessageBox>
#include <QFrame>
#include <QScrollArea>
#include <QDragLeaveEvent>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrentRun>
#include <memory>

namespace {
class DestinationRow final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return {QStyledItemDelegate::sizeHint(option, index).width(), 48};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const auto card = option.rect.adjusted(0, 0, 0, -4);
        const auto text = card.adjusted(12, 0, -12, 0);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const auto& tokens = OpenRollTheme::tokens;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(tokens.value(selected ? "@accentSoft" : "@raised")));
        painter->drawRoundedRect(card, 10, 10);
        painter->setFont(option.font);
        painter->setPen(option.palette.text().color());
        painter->drawText(text.adjusted(0, 4, 0, -22), Qt::AlignVCenter,
                          option.fontMetrics.elidedText(index.data().toString(), Qt::ElideMiddle, text.width()));
        auto subfont = option.font;
        subfont.setPixelSize(11);
        painter->setFont(subfont);
        painter->setPen(QColor(tokens.value("@muted")));
        const auto target = index.data(Qt::UserRole).toString();
        if (!target.isEmpty())
            painter->drawText(text.adjusted(0, 22, 0, -4), Qt::AlignVCenter,
                              QFontMetrics(subfont).elidedText("Final copy path · " + target, Qt::ElideMiddle, text.width()));
        painter->restore();
    }
};

class SourceDrop final : public QLabel {
public:
    explicit SourceDrop(QWidget* parent) : QLabel(parent)
    {
        setAcceptDrops(true);
        setAlignment(Qt::AlignCenter);
        setWordWrap(true);
        setTextFormat(Qt::PlainText);
        setObjectName("sourceDrop");
        setMinimumHeight(200);
        setText("Drop footage or a camera folder here\nto add it to the project");
    }
    std::function<void(const QString&)> picked;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override { highlight(acceptSource(event)); }
    void dragLeaveEvent(QDragLeaveEvent* event) override { highlight(false); event->accept(); }
    void dragMoveEvent(QDragMoveEvent* event) override { acceptSource(event); }
    void dropEvent(QDropEvent* event) override
    {
        highlight(false);
        if (!acceptSource(event)) return;
        const auto path = event->mimeData()->urls().front().toLocalFile();
        // Finish the native drop before opening a project dialog.
        QMetaObject::invokeMethod(this, [this, path] { picked(path); }, Qt::QueuedConnection);
    }

private:
    void highlight(bool active)
    {
        setProperty("dragActive", active);
        style()->unpolish(this);
        style()->polish(this);
    }

    static bool acceptSource(QDropEvent* event)
    {
        const auto urls = event->mimeData()->urls();
        if (urls.size() != 1 || !urls.front().isLocalFile()) return false;
        if (QFileInfo(urls.front().toLocalFile()).suffix().compare("openroll", Qt::CaseInsensitive) == 0)
            return false;
        const auto actions = event->possibleActions();
        if (actions.testFlag(Qt::CopyAction)) event->setDropAction(Qt::CopyAction);
        else if (actions.testFlag(Qt::LinkAction)) event->setDropAction(Qt::LinkAction);
        else return false;
        event->accept();
        return true;
    }
};
}

IngestWidget::IngestWidget(std::function<QString()> currentProject,
                           std::function<void(const QString&)> mediaLinked, std::function<void(bool)> chooseProject, QWidget* parent)
    : QWidget(parent), projectPath(std::move(currentProject))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    layout->addWidget(scroll);
    auto* canvas = new QWidget;
    scroll->setWidget(canvas);
    auto* centered = new QHBoxLayout(canvas);
    centered->setContentsMargins(24, 16, 24, 16);
    centered->addStretch();
    auto* column = new QWidget(canvas);
    column->setMaximumWidth(880);
    centered->addWidget(column, 1);
    centered->addStretch();
    auto* sections = new QVBoxLayout(column);
    sections->setContentsMargins(0, 0, 0, 0);
    sections->setSpacing(16);
    auto section = [](QVBoxLayout* parent, const QString& name) {
        parent->addWidget(sectionLabel(name, parent->parentWidget()));
        auto* card = new QFrame(parent->parentWidget());
        card->setObjectName("card");
        parent->addWidget(card);
        auto* contents = new QVBoxLayout(card);
        contents->setContentsMargins(16, 16, 16, 16);
        contents->setSpacing(8);
        return contents;
    };
    sections->addStretch();
    auto* sourceLayout = section(sections, "Import footage");
    auto* sourceGroup = sourceLayout->parentWidget();
    auto* source = new QLineEdit(this);
    source->setPlaceholderText("Source folder or mounted camera card");
    source->setAccessibleName("Source folder");
    source->setReadOnly(true);
    source->hide();
    auto* drop = new SourceDrop(sourceGroup);
    sourceLayout->addWidget(drop);
    auto* browse = new QPushButton("Choose folder…", this);
    auto* browseFile = new QPushButton("Choose file…", this);
    auto* sourceRow = new QHBoxLayout;
    sourceRow->addStretch();
    sourceRow->addWidget(browseFile);
    sourceRow->addWidget(browse);
    sourceLayout->addLayout(sourceRow);

    sections->addStretch();
    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(24, 4, 24, 16);
    footer->addStretch();
    auto* backup = new QPushButton("Back up…", this);
    backup->setObjectName("openBackup");
    backup->setToolTip("Import footage to prepare a verified backup");
    footer->addWidget(backup);
    layout->addLayout(footer);

    auto* backupDialog = new QDialog(this);
    backupDialog->setObjectName("verifiedBackup");
    backupDialog->setWindowTitle("Verified backup");
    backupDialog->setWindowModality(Qt::WindowModal);
    backupDialog->resize(650, 380);
    auto* backupLayout = new QVBoxLayout(backupDialog);
    backupLayout->setContentsMargins(16, 16, 16, 16);
    backupLayout->setSpacing(12);
    auto* backupSource = new QLabel(backupDialog);
    backupSource->setObjectName("quiet");
    backupSource->setTextFormat(Qt::PlainText);
    backupSource->setWordWrap(true);
    backupLayout->addWidget(backupSource);
    auto* copiesLayout = section(backupLayout, "Destinations");
    auto* copies = copiesLayout->parentWidget();
    auto* copyName = new QLineEdit(this);
    copyName->setAccessibleName("Copy folder name");
    copyName->setPlaceholderText("Defaults to the source folder name");
    auto* naming = new QWidget(copies);
    auto* nameRow = new QHBoxLayout(naming);
    nameRow->setContentsMargins(0, 0, 0, 0);
    auto* nameLabel = new QLabel("Copy &folder name", this);
    nameLabel->setBuddy(copyName);
    nameRow->addWidget(nameLabel);
    nameRow->addWidget(copyName, 1);


    auto* destinationActions = new QWidget(this);
    destinationActions->setObjectName("destinationActions");
    auto* destinationsRow = new QHBoxLayout(destinationActions);
    destinationsRow->setContentsMargins(12, 4, 12, 4);
    destinationsRow->setSpacing(8);


    auto* add = new QPushButton("Add destination…", this);
    auto* remove = new QPushButton("Remove selected", this);
    auto* emptyDestinations = new QLabel("Add a backup destination", this);
    emptyDestinations->setObjectName("quiet");
    destinationsRow->addWidget(emptyDestinations, 1);
    destinationsRow->addWidget(add);
    destinationsRow->addWidget(remove);

    auto* destinations = new QTreeWidget(this);
    destinations->setHeaderHidden(true);
    destinations->setRootIsDecorated(false);
    destinations->setItemDelegate(new DestinationRow(destinations));
    destinations->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    destinations->setAccessibleName("Backup destinations and final copy paths");
    destinations->setObjectName("ingestDestinations");
    copiesLayout->addWidget(destinations);
    copiesLayout->addWidget(destinationActions);
    copiesLayout->addWidget(naming);

    auto* validation = new QLabel(this);
    validation->setWordWrap(true);
    validation->setTextFormat(Qt::PlainText);
    validation->setObjectName("quiet");
    auto* transferLayout = section(backupLayout, "Verify");
    auto* status = new QLabel(this);
    status->setWordWrap(true);
    status->setTextFormat(Qt::PlainText);
    status->hide();
    transferLayout->addWidget(status);
    auto* jobBar = new QProgressBar(this);
    jobBar->setTextVisible(false);
    jobBar->hide();
    transferLayout->addWidget(jobBar);
    auto* actions = new QHBoxLayout;
    auto* start = new QPushButton("Start verified copy", this);
    setAction(start, "primary");
    start->setFixedHeight(40);
    auto* cancel = new QPushButton("Cancel transfer", this);
    auto* report = new QPushButton("Open report folder", this);
    cancel->hide();
    report->hide();
    actions->addWidget(validation, 1);
    actions->addWidget(report);
    actions->addWidget(cancel);
    actions->addWidget(start);
    transferLayout->addLayout(actions);
    backupLayout->addStretch();
    auto* close = new QPushButton("Close", backupDialog);
    backupLayout->addWidget(close, 0, Qt::AlignRight);
    connect(close, &QPushButton::clicked, backupDialog, &QDialog::reject);
    connect(backup, &QPushButton::clicked, backupDialog, [=] {
        const auto path = source->text();
        backupSource->setText("Source: " + path);
        backupDialog->open();
    });

    auto* watcher = new QFutureWatcher<IngestResult>(this);
    auto* indexer = new QFutureWatcher<QStringList>(this);
    auto indexed = std::make_shared<bool>(false);
    auto paths = [destinations] {
        QStringList result;
        for (int i = 0; i < destinations->topLevelItemCount(); ++i)
            result << destinations->topLevelItem(i)->text(0);
        return result;
    };
    auto updatePlan = [=] {
        const bool busy = watcher->isRunning();
        const auto roots = paths();
        backup->setEnabled(*indexed);
        backup->setToolTip(*indexed ? "Set up a checksum-verified copy of the current source"
                                    : "Import footage to prepare a verified backup");
        copies->setEnabled(*indexed && !busy);
        emptyDestinations->setVisible(roots.isEmpty());
        destinations->setVisible(!roots.isEmpty());
        destinations->setFixedHeight(qMin(roots.size(), qsizetype(4)) * 48 + 2);
        destinationActions->setFixedHeight(roots.isEmpty() ? 56 : 48);
        setAction(destinationActions, roots.isEmpty() ? "empty" : "");
        setAction(add, roots.isEmpty() ? "" : "ghost");
        naming->setVisible(!roots.isEmpty());
        remove->setVisible(!roots.isEmpty());
        const auto name = copyName->text().trimmed();
        QString error;
        QStringList planned;
        try {
            if (!source->text().isEmpty() && !roots.isEmpty())
                planned = ingestOutputPaths(source->text(), roots, name);
        } catch (const std::exception& problem) {
            error = QString::fromUtf8(problem.what());
        }
        for (int i = 0; i < roots.size(); ++i) {
            const auto target = planned.value(i);
            auto* item = destinations->topLevelItem(i);
            item->setData(0, Qt::UserRole, target);
            item->setToolTip(0, target.isEmpty() ? roots.at(i) : roots.at(i) + "\n" + target);
        }
        const auto prompt = source->text().isEmpty() ? QString("Choose a camera card or folder to copy.")
                          : roots.isEmpty() ? QString(*indexed ? "Add a backup destination to continue." : "Index the source before adding destinations.")
                          : !error.isEmpty() ? error
                          : QString("Ready to copy and verify %1 %2 with SHA-256.").arg(planned.size())
                                .arg(planned.size() == 1 ? "copy" : "copies");
        validation->setText(prompt);
        validation->setVisible(!busy && !prompt.isEmpty());
        setAction(validation, error.isEmpty() ? "" : "warning");
        start->setEnabled(!busy && *indexed && !planned.isEmpty());
        remove->setEnabled(!busy && destinations->currentItem());
    };
    indexSource = [=](const QString& picked) {
        if (picked.isEmpty()) return;
        if (watcher->isRunning() || indexer->isRunning()) {
            QMessageBox::information(this, "Add media", "Wait for the current ingest operation to finish, then drop the media again.");
            return;
        }
        const QFileInfo input(picked);
        if ((input.isDir() || input.isFile()) && projectPath().isEmpty()) {
            QMessageBox prompt(this);
            prompt.setWindowTitle("Link footage to a project");
            prompt.setTextFormat(Qt::PlainText);
            prompt.setText("Add “" + input.fileName() + "” to a project?");
            auto* create = prompt.addButton("New project…", QMessageBox::AcceptRole);
            auto* open = prompt.addButton("Open project…", QMessageBox::ActionRole);
            prompt.addButton("Continue without a project", QMessageBox::RejectRole);
            prompt.exec();
            if (prompt.clickedButton() == create || prompt.clickedButton() == open)
                chooseProject(prompt.clickedButton() == create);
        }
        *indexed = false;
        status->hide();
        jobBar->hide();
        report->hide();
        cancelled.store(false);
        source->setText(input.absoluteFilePath());
        copyName->clear();
        copyName->setPlaceholderText(input.isFile() ? input.completeBaseName() : input.fileName());
        drop->setText("Indexing " + input.fileName() + "…");
        browse->setEnabled(false);
        browseFile->setEnabled(false);
        updatePlan();
        const auto database = projectPath();
        indexer->setFuture(QtConcurrent::run(&worker, [this, path = source->text(), database] {
            QString summary;
            QString ready;
            try {
                const auto inventory = indexIngestSource(path, cancelled);
                summary = QString("%1 %2 · %3").arg(inventory.files)
                    .arg(inventory.files == 1 ? "file" : "files")
                    .arg(QLocale().formattedDataSize(inventory.bytes));
                ready = "ready";
            } catch (const std::exception& error) {
                summary = "Backup unavailable: " + QString::fromUtf8(error.what());
            }
            if (database.isEmpty()) {
                summary += "\nStandalone ingest · source indexed for backup.";
                return QStringList{summary, {}, ready};
            }
            try {
                const QFileInfo input(QFileInfo(path).canonicalFilePath());
                if (input.filePath().isEmpty()) throw std::runtime_error("Folder is unavailable.");
                const auto clips = scanMedia(input.filePath(), cancelled);
                if (cancelled.load()) return QStringList{summary, {}, ready};
                if (clips.isEmpty()) return QStringList{summary + "\nNo supported media found.", {}, ready};
                Project project(database, false);
                project.saveScan(input.isDir() ? input.filePath() : input.absolutePath(), clips);
                return QStringList{summary + QString("\n%1 clips linked to %2")
                    .arg(clips.size()).arg(project.name()), database, ready};
            } catch (const std::exception& error) {
                return QStringList{summary + "\nCould not link project media: " + QString::fromUtf8(error.what()), {}, ready};
            }
        }));
    };
    drop->picked = indexSource;
    connect(indexer, &QFutureWatcher<QStringList>::finished, this, [=] {
        browse->setEnabled(true);
        browseFile->setEnabled(true);
        try {
            const auto result = indexer->result();
            drop->setText(QFileInfo(source->text()).fileName() + '\n' + result.at(0));
            *indexed = !result.at(2).isEmpty();
            if (!result.at(1).isEmpty()) {
                try { mediaLinked(result.at(1)); }
                catch (const std::exception& error) {
                    drop->setText(drop->text() + "\nCould not refresh Media: " + QString::fromUtf8(error.what()));
                }
            }
        } catch (...) {
            drop->setText("Could not index " + QFileInfo(source->text()).fileName() +
                          "\nChoose a readable local file or folder without symbolic links.");
        }
        drop->setToolTip(source->text());
        updatePlan();
    });
    connect(browse, &QPushButton::clicked, this, [=] {
        drop->picked(QFileDialog::getExistingDirectory(this, "Choose source folder"));
    });
    connect(browseFile, &QPushButton::clicked, this, [=] {
        drop->picked(QFileDialog::getOpenFileName(this, "Choose source file"));
    });
    connect(add, &QPushButton::clicked, this, [=] {
        const auto picked = QFileDialog::getExistingDirectory(this, "Choose destination folder");
        if (picked.isEmpty()) return;
        const auto canonical = QFileInfo(picked).canonicalFilePath();
        if (canonical.isEmpty() || paths().contains(canonical)) return;
        auto* item = new QTreeWidgetItem(destinations, {canonical});
        item->setToolTip(0, canonical);
        destinations->setCurrentItem(item);
        updatePlan();
    });
    connect(remove, &QPushButton::clicked, this, [=] {
        delete destinations->currentItem();
        updatePlan();
    });
    connect(destinations, &QTreeWidget::currentItemChanged, this, updatePlan);
    connect(copyName, &QLineEdit::textChanged, this, updatePlan);
    updatePlan();

    connect(cancel, &QPushButton::clicked, this, [this, status, cancel] {
        cancelled.store(true);
        cancel->setEnabled(false);
        status->setText("Cancelling…");
    });
    connect(report, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(reportDirectory));
    });
    auto setActive = [=](bool active) {
        copies->setEnabled(!active);
        browse->setEnabled(!active);
        browseFile->setEnabled(!active);
        drop->setAcceptDrops(!active);
        copyName->setEnabled(!active);
        add->setEnabled(!active);
        cancel->setVisible(active);
        cancel->setEnabled(active);
        if (active) {
            validation->hide();
            start->setEnabled(false);
            remove->setEnabled(false);
        } else {
            updatePlan();
        }
    };
    connect(watcher, &QFutureWatcher<IngestResult>::finished, this, [=] {
        const auto result = watcher->result();
        setActive(false);
        reportDirectory = result.reportDirectory;
        report->setVisible(!reportDirectory.isEmpty());
        setAction(status, result.verified ? "success" : "danger");
        status->setText(result.message);
        jobBar->setRange(0, 100);
        if (result.verified) {
            jobBar->setValue(100);
        }
    });
    connect(start, &QPushButton::clicked, this, [=] {
        const auto databasePath = projectPath();
        const auto name = databasePath.isEmpty() ? QString("Standalone ingest") : QFileInfo(databasePath).completeBaseName();
        const auto targets = paths();
        const auto origin = source->text();
        const auto folder = copyName->text().trimmed();
        try {
            ingestOutputPaths(origin, targets, folder);
        } catch (const std::exception&) {
            updatePlan();
            return;
        }
        const auto reports = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/transfers";
        cancelled.store(false);
        setActive(true);
        report->hide();
        status->show();
        jobBar->show();
        setAction(status, "");
        status->setText("Starting transfer…");
        jobBar->setRange(0, 0);
        watcher->setFuture(QtConcurrent::run(&worker, [=] {
            QElapsedTimer elapsed;
            elapsed.start();
            qint64 lastUpdate = -100;
            auto result = ingest(name, origin, targets, reports, cancelled, [&](const IngestProgress& progress) {
                const auto ms = elapsed.elapsed();
                if (ms - lastUpdate < 100) return;
                lastUpdate = ms;
                QMetaObject::invokeMethod(this, [=] {
                    if (cancelled.load()) return;
                    if (progress.total > 0) {
                        jobBar->setRange(0, 100);
                        jobBar->setValue(qMin(99, int(100.0 * progress.done / progress.total)));
                    }
                    QString text = progress.phase;
                    if (progress.total > 0) text += QString(" · %1%").arg(jobBar->value());
                    if (!progress.file.isEmpty()) text += "  ·  " + progress.file;
                    if (progress.done > 0 && ms > 0) {
                        const double speed = double(progress.done) / ms;
                        const double remaining = (progress.total - progress.done) / speed / 1000;
                        text += QString("  ·  %1 MB/s  ·  %2 s remaining")
                                    .arg(speed / 1000, 0, 'f', 1).arg(remaining, 0, 'f', 0);
                    }
                    status->setText(text);
                }, Qt::QueuedConnection);
            }, folder);
            if (!databasePath.isEmpty()) {
                try {
                    Project destination(databasePath, false);
                    destination.recordTransfer(result.reportDirectory, result.verified, result.message);
                } catch (const std::exception& error) {
                    result.message += " Could not save to project history: " + QString::fromUtf8(error.what());
                }
            }
            return result;
        }));
    });
}

IngestWidget::~IngestWidget()
{
    cancelled.store(true);
    worker.waitForDone();
}
