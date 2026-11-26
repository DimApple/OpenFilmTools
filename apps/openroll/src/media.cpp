#include "media.hpp"
#include "playback.hpp"
#include "new_project.hpp"
#include "theme.hpp"

#include <QButtonGroup>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QListWidget>
#include <QMap>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QShortcut>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSettings>
#include <QStandardPaths>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

namespace {
constexpr int clipIndexRole = Qt::UserRole;
constexpr int durationRole = Qt::UserRole + 1;

QString duration(double seconds)
{
    if (seconds < 0) return "Unavailable";
    const auto whole = qint64(seconds);
    return QString("%1:%2").arg(whole / 60).arg(whole % 60, 2, 10, QChar('0'));
}

QIcon clipViewIcon(bool grid)
{
    QPixmap bitmap(32, 32);
    bitmap.setDevicePixelRatio(2);
    bitmap.fill(Qt::transparent);
    QPainter painter(&bitmap);
    const QColor ink(OpenRollTheme::tokens.value("@text"));
    if (grid) {
        for (int y : {3, 9})
            for (int x : {3, 9}) painter.fillRect(x, y, 4, 4, ink);
    } else {
        painter.setPen(QPen(ink, 1.5));
        for (int y : {4, 8, 12}) painter.drawLine(3, y, 13, y);
    }
    return QIcon(bitmap);
}

class ClipCard final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return {164, 150}; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRect card = option.rect.adjusted(4, 4, -4, -4);
        const bool selected = option.state & QStyle::State_Selected;
        painter->setPen(Qt::NoPen);
        if (option.state & QStyle::State_MouseOver) {
            painter->setBrush(QColor(OpenRollTheme::tokens.value("@raised")));
            painter->drawRoundedRect(card, 10, 10);
        }
        const int width = card.width() - 8;
        const QRect picture(card.x() + 4, card.y() + 8, width, width * 9 / 16);
        QPainterPath boundary;
        boundary.addRoundedRect(picture, 8, 8);
        painter->fillPath(boundary, QColor(OpenRollTheme::tokens.value("@bg")));
        const auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (!icon.isNull()) {
            const auto pixmap = icon.pixmap(QSize(160, 90));
            const auto size = pixmap.size().scaled(picture.size(), Qt::KeepAspectRatioByExpanding);
            const QRect target(QPoint(picture.center().x() - size.width() / 2,
                                      picture.center().y() - size.height() / 2), size);
            painter->save();
            painter->setClipPath(boundary);
            painter->setRenderHint(QPainter::SmoothPixmapTransform);
            painter->drawPixmap(target, pixmap);
            painter->restore();
        } else {
            painter->setPen(QColor(OpenRollTheme::tokens.value("@muted")));
            painter->drawText(picture, Qt::AlignCenter, "No thumbnail");
        }
        const QRect caption(card.x() + 9, picture.bottom() + 2, card.width() - 18, 22);
        const auto time = index.data(durationRole).toString();
        const int timeWidth = option.fontMetrics.horizontalAdvance(time);
        painter->setPen(QColor(OpenRollTheme::tokens.value("@text")));
        painter->drawText(caption.adjusted(0, 0, -timeWidth - 9, 0), Qt::AlignVCenter,
            option.fontMetrics.elidedText(index.data().toString(), Qt::ElideMiddle, caption.width() - timeWidth - 9));
        painter->setPen(QColor(OpenRollTheme::tokens.value("@muted")));
        painter->drawText(caption, Qt::AlignRight | Qt::AlignVCenter, time);
        painter->setBrush(Qt::NoBrush);
        if (selected) {
            painter->setPen(QPen(QColor(OpenRollTheme::tokens.value("@accent")), 2));
            painter->drawRoundedRect(card, 10, 10);
        }
        painter->restore();
    }
};

QFrame* panel(QWidget* parent)
{
    auto* frame = new QFrame(parent);
    frame->setObjectName("mediaPanel");
    return frame;
}
}

MediaWorkspace::MediaWorkspace(Playback* playback, std::function<void(const QString&)> notifyProject, QWidget* parent)
    : QWidget(parent), projectChanged(std::move(notifyProject)), viewer(playback)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);
    split->setHandleWidth(8);
    sourcePanel = panel(split);
    auto* left = new QVBoxLayout(sourcePanel);
    left->setContentsMargins(12, 12, 12, 12);
    left->setSpacing(8);
    auto* mediaHeading = new QHBoxLayout;
    mediaHeading->addWidget(sectionLabel("Project media", sourcePanel));
    mediaHeading->addStretch();
    left->addLayout(mediaHeading);
    folders = new QTreeWidget(sourcePanel);
    folders->setObjectName("mediaFolders");
    folders->setAccessibleName("Project media folders");
    folders->setHeaderHidden(true);
    folders->setIndentation(16);
    left->addWidget(folders, 1);
    auto* root = new QTreeWidgetItem(folders, {"All clips"});
    root->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    folders->setCurrentItem(root);
    add = new QPushButton("+", sourcePanel);
    add->setObjectName("addMedia");
    add->setFixedSize(24, 24);
    add->setAccessibleName("Add media folder");
    add->setToolTip("Add media folder…");
    add->setEnabled(false);
    setAction(add, "ghost");
    mediaHeading->addWidget(add);
    auto* cancel = new QPushButton("Cancel scan", sourcePanel);
    setAction(cancel, "link");
    cancel->hide();
    mediaHeading->addWidget(cancel);
    status = new QLabel("Create or open a project to add media.", sourcePanel);
    status->setObjectName("quiet");
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    left->addWidget(status);

    auto* center = panel(split);
    auto* monitor = new QVBoxLayout(center);
    monitor->setContentsMargins(0, 0, 0, 0);
    monitor->addWidget(viewer);
    inspector = panel(split);
    inspector->setFixedWidth(280);
    inspector->hide();
    auto* right = new QVBoxLayout(inspector);
    right->addWidget(sectionLabel("Metadata", inspector));
    metadata = new QTreeWidget(inspector);
    metadata->setObjectName("metadataList");
    metadata->setAccessibleName("Clip metadata");
    metadata->setHeaderHidden(true);
    metadata->setColumnCount(2);
    metadata->setRootIsDecorated(false);
    metadata->setSelectionMode(QAbstractItemView::NoSelection);
    metadata->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    right->addWidget(metadata, 1);
    auto* toggle = viewer->findChild<QToolButton*>("metadataToggle");
    const auto visible = QSettings("OpenRoll", "OpenRoll").value("media/showMetadata", false).toBool();
    toggle->setChecked(visible);
    inspector->setVisible(visible);
    connect(toggle, &QToolButton::toggled, this, [this, toggle](bool enabled) {
        QSettings("OpenRoll", "OpenRoll").setValue("media/showMetadata", enabled);
        inspector->setVisible(enabled && toggle->isVisible());
    });
    auto* metadataShortcut = new QShortcut(QKeySequence("I"), this);
    metadataShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(metadataShortcut, &QShortcut::activated, toggle, [toggle] {
        if (toggle->isVisible()) toggle->click();
    });
    split->setStretchFactor(1, 1);
    split->setSizes({240, 715, 280});
    sourcePanel->setMinimumWidth(200);
    sourcePanel->setMaximumWidth(320);
    layout->addWidget(split, 1);

    strip = panel(this);
    auto* stripLayout = new QVBoxLayout(strip);
    stripLayout->setContentsMargins(12, 8, 12, 8);
    stripLayout->setSpacing(8);
    auto* tools = new QHBoxLayout;
    auto* views = new QButtonGroup(strip);
    auto* cards = new QToolButton(strip);
    auto* rows = new QToolButton(strip);
    auto* viewSwitch = new QWidget(strip);
    viewSwitch->setObjectName("clipViews");
    auto* viewLayout = new QHBoxLayout(viewSwitch);
    viewLayout->setContentsMargins(3, 3, 3, 3);
    viewLayout->setSpacing(0);
    cards->setIcon(clipViewIcon(true));
    rows->setIcon(clipViewIcon(false));
    cards->setAccessibleName("Thumbnail view");
    rows->setAccessibleName("List view");
    cards->setToolTip("Thumbnail view");
    rows->setToolTip("List view");
    for (auto* button : {cards, rows}) {
        button->setObjectName("viewToggle");
        button->setCheckable(true);
        views->addButton(button);
        button->setIconSize(QSize(16, 16));
        button->setFixedSize(28, 22);
        viewLayout->addWidget(button);
    }
    tools->addWidget(viewSwitch);
    cards->setChecked(true);
    filter = new QLineEdit(strip);
    filter->setPlaceholderText("Search clips");
    filter->setFixedHeight(28);
    filter->setAccessibleName("Search clips");
    filter->setClearButtonEnabled(true);
    filter->setMaximumWidth(310);
    tools->addWidget(filter, 1);
    tools->addStretch();
    clipCount = new QLabel("0 clips", strip);
    clipCount->setObjectName("quiet");
    tools->addWidget(clipCount);
    stripLayout->addLayout(tools);
    media = new QListWidget(strip);
    media->setObjectName("clipStrip");
    media->setAccessibleName("Project clips");
    media->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    media->setSpacing(0);
    media->setMouseTracking(true);
    stripLayout->addWidget(media);
    layout->addWidget(strip);

    auto* delegate = new ClipCard(media);
    auto* standard = media->itemDelegate();
    auto applyView = [=](bool thumbnails) {
        media->setItemDelegate(thumbnails ? delegate : standard);
        media->setViewMode(thumbnails ? QListView::IconMode : QListView::ListMode);
        media->setFlow(thumbnails ? QListView::LeftToRight : QListView::TopToBottom);
        media->setWrapping(false);
        media->setMovement(QListView::Static);
        media->setIconSize(thumbnails ? QSize(160, 90) : QSize(45, 26));
        media->setFixedHeight(164);
    };
    applyView(true);
    connect(cards, &QToolButton::clicked, this, [applyView] { applyView(true); });
    connect(rows, &QToolButton::clicked, this, [applyView] { applyView(false); });
    connect(filter, &QLineEdit::textChanged, this, &MediaWorkspace::filterMedia);
    connect(folders, &QTreeWidget::currentItemChanged, this, [this] { filterMedia(); });
    connect(media, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (!item) { showMetadata(nullptr); return; }
        const auto& clip = clips.at(item->data(clipIndexRole).toInt());
        selectedPath = clip.path;
        showMetadata(&clip);
    });
    connect(media, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        if (item) openClip(clips.at(item->data(clipIndexRole).toInt()).path);
    });
    connect(media, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item) openClip(clips.at(item->data(clipIndexRole).toInt()).path);
    });
    connect(cancel, &QPushButton::clicked, this, [this, cancel] {
        cancelled.store(true);
        cancel->setEnabled(false);
        status->setText("Cancelling scan…");
    });
    showMetadata(nullptr);
    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [=] {
        cancel->hide();
        add->setEnabled(true);
        scanning = false;
        add->show();
        try {
            status->setText(watcher->result());
            refreshMedia();
        } catch (const std::exception& error) {
            status->setText(QString::fromUtf8(error.what()));
        }
    });
    connect(add, &QPushButton::clicked, this, [=] {
        QString initialFolder;
        try { if (project) initialFolder = project->lastScanFolder(); }
        catch (const std::exception& error) {
            status->setText(QString::fromUtf8(error.what()));
            return;
        }
        const auto folder = QFileDialog::getExistingDirectory(this, "Scan media folder", initialFolder);
        if (!folder.isEmpty()) linkMedia({folder});
    });
    scanSources = [=](const QStringList& sources) {
        const auto path = projectPath();
        cancelled.store(false);
        status->setText("Indexing media…");
        add->setEnabled(false);
        scanning = true;
        add->hide();
        cancel->setEnabled(true);
        cancel->show();
        watcher->setFuture(QtConcurrent::run(&worker, [this, path, sources] {
            QStringList errors;
            qsizetype saved = 0;
            for (const auto& source : sources) {
                if (cancelled.load()) break;
                try {
                    const QFileInfo input(QFileInfo(source).canonicalFilePath());
                    if (!input.exists()) throw std::runtime_error("Source is unavailable.");
                    const auto records = scanMedia(input.filePath(), cancelled);
                    if (cancelled.load()) break;
                    if (records.isEmpty()) throw std::runtime_error("No supported media found.");
                    Project destination(path, false);
                    destination.saveScan(input.isDir() ? input.filePath() : input.absolutePath(), records);
                    saved += records.size();
                } catch (const std::exception& error) {
                    errors << QFileInfo(source).fileName() + ": " + QString::fromUtf8(error.what());
                }
            }
            QStringList result;
            if (saved) result << QString("Linked %1 %2.").arg(saved).arg(saved == 1 ? "clip" : "clips");
            if (cancelled.load()) result << "Scan cancelled.";
            return (result + errors).join('\n');
        }));
    };
}

MediaWorkspace::~MediaWorkspace()
{
    cancelled.store(true);
    worker.waitForDone();
}

void MediaWorkspace::linkMedia(const QStringList& sources)
{
    if (sources.isEmpty() || !project) return;
    if (scanning) {
        QMessageBox::information(this, "Add media", "Wait for the current scan to finish, or cancel it and drop the media again.");
        return;
    }
    filter->clear();
    scanSources(sources);
}

void MediaWorkspace::chooseProject(bool create)
{
    if (scanning) return;
    const QString projectFilter = "OpenRoll project (*.openroll)";
    QSettings preferences("OpenRoll", "OpenRoll");
    auto folder = preferences.value("projects/folder", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    if (!QFileInfo(folder).isDir()) folder = QDir::homePath();
    const auto file = create ? chooseNewProject(this, folder)
                             : QFileDialog::getOpenFileName(this, "Open OpenRoll project", folder, projectFilter);
    openProjectFile(file, create);
}

void MediaWorkspace::openProjectFile(const QString& file, bool create)
{
    if (scanning || file.isEmpty()) return;
    QSettings preferences("OpenRoll", "OpenRoll");
    try {
        auto opened = std::make_unique<Project>(file, create);
        project = std::move(opened);
        preferences.setValue("projects/folder", QFileInfo(file).absolutePath());
        projectChanged(projectName());
        selectedPath.clear();
        filter->clear();
        add->setEnabled(true);
        status->clear();
        refreshMedia();
    } catch (const std::exception& error) {
        QMessageBox::warning(this, "OpenRoll project", QString::fromUtf8(error.what()));
    }
}

void MediaWorkspace::closeProject()
{
    if (!project) return;
    if (scanning) {
        QMessageBox::information(this, "Close project", "Wait for the media scan to finish or cancel it.");
        return;
    }
    project.reset();
    projectChanged({});
    add->setEnabled(false);
    selectedPath.clear();
    clips.clear();
    {
        const QSignalBlocker block(folders);
        folders->clear();
    }
    filter->clear();
    filterMedia();
    status->setText("Create or open a project to add media.");
}

void MediaWorkspace::setMonitorMode(bool enabled)
{
    sourcePanel->setVisible(!enabled);
    auto* toggle = viewer->findChild<QToolButton*>("metadataToggle");
    toggle->setVisible(!enabled);
    inspector->setVisible(!enabled && toggle->isChecked());
    auto* counter = viewer->findChild<QLabel*>("quiet");
    counter->setProperty("monitor", enabled);
    counter->style()->unpolish(counter);
    counter->style()->polish(counter);
    strip->setVisible(!enabled);
}

void MediaWorkspace::openClip(const QString& path)
{
    if (!QFileInfo(path).isFile()) {
        status->setText("Media is offline: " + path);
        return;
    }
    if (viewer->activePath() == path && viewer->error().isEmpty()) return;
    viewer->open(path);
}

void MediaWorkspace::refreshMedia()
{
    if (!project) return;
    clips = project->clips();
    {
        const QSignalBlocker block(folders);
        folders->clear();
        auto* root = new QTreeWidgetItem(folders, {"All clips"});
        root->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        QMap<QString, QTreeWidgetItem*> found;
        const auto sources = project->sourceFolders();
        for (const auto& source : sources) {
            auto* item = new QTreeWidgetItem(root, {QFileInfo(source).fileName().isEmpty() ? source : QFileInfo(source).fileName()});
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            item->setData(0, Qt::UserRole, source);
            item->setToolTip(0, source);
            found.insert(source, item);
        }
        for (const auto& clip : clips) {
            const auto dir = QFileInfo(clip.path).absolutePath();
            QString base;
            for (const auto& source : sources) {
                if ((dir == source || dir.startsWith(source.endsWith('/') ? source : source + '/')) &&
                    source.size() > base.size()) base = source;
            }
            if (base.isEmpty() || base == dir) continue;
            auto* parent = found.value(base);
            for (const auto& part : QDir(base).relativeFilePath(dir).split('/')) {
                base = QDir(base).filePath(part);
                if (!found.contains(base)) {
                    auto* child = new QTreeWidgetItem(parent, {part});
                    child->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
                    child->setData(0, Qt::UserRole, base);
                    child->setToolTip(0, base);
                    found.insert(base, child);
                }
                parent = found.value(base);
            }
        }
        root->setExpanded(true);
        folders->setCurrentItem(root);
    }
    filterMedia();
}

void MediaWorkspace::filterMedia()
{
    const auto query = filter->text().trimmed();
    const auto* node = folders->currentItem();
    const auto folder = node ? node->data(0, Qt::UserRole).toString() : QString{};
    const auto prefix = folder.endsWith('/') ? folder : folder + '/';
    const QSignalBlocker block(media);
    media->clear();
    QListWidgetItem* selected = nullptr;
    for (int i = 0; i < clips.size(); ++i) {
        const auto& clip = clips.at(i);
        if (!folder.isEmpty() && !clip.path.startsWith(prefix)) continue;
        const QFileInfo file(clip.path);
        if (!query.isEmpty() && !file.fileName().contains(query, Qt::CaseInsensitive) &&
            !clip.codec.contains(query, Qt::CaseInsensitive) && !clip.camera.contains(query, Qt::CaseInsensitive)) continue;
        auto* item = new QListWidgetItem(file.fileName() + (file.isFile() ? "" : " (offline)"), media);
        item->setData(clipIndexRole, i);
        item->setData(durationRole, clip.duration < 0 ? "—" : duration(clip.duration));
        item->setToolTip(clip.note.isEmpty() ? clip.path : clip.path + '\n' + clip.note);
        QPixmap thumb;
        if (thumb.loadFromData(clip.thumbnail)) item->setIcon(thumb);
        if (clip.path == selectedPath) selected = item;
    }
    if (selected) media->setCurrentItem(selected);
    showMetadata(selected ? &clips.at(selected->data(clipIndexRole).toInt()) : nullptr);
    clipCount->setText(QString("%1 %2").arg(media->count()).arg(media->count() == 1 ? "clip" : "clips"));
}

void MediaWorkspace::showMetadata(const Clip* clip)
{
    metadata->clear();
    if (!clip) {
        auto* empty = new QTreeWidgetItem(metadata, {"Select a clip to see its metadata"});
        empty->setTextAlignment(0, Qt::AlignHCenter);
        empty->setFirstColumnSpanned(true);
        return;
    }
    const auto unknown = [](const QString& text) { return text.isEmpty() ? QString("Unavailable") : text; };
    const QList<QStringList> values{
        {"Filename", QFileInfo(clip->path).fileName()}, {"Camera", unknown(clip->camera)},
        {"Resolution", clip->size.isValid() ? QString("%1 × %2").arg(clip->size.width()).arg(clip->size.height()) : "Unavailable"},
        {"Frame rate", clip->fps > 0 ? QString::number(clip->fps, 'f', 3) + " fps" : "Unavailable"},
        {"Duration", duration(clip->duration)}, {"Timecode", unknown(clip->timecode)},
        {"Codec", unknown(clip->codec)}, {"Size", QLocale().formattedDataSize(clip->bytes)}
    };
    for (const auto& value : values) {
        auto* item = new QTreeWidgetItem(metadata, {value.at(0)});
        item->setToolTip(1, value.at(1));
        item->setForeground(0, QColor(OpenRollTheme::tokens.value("@muted")));
        auto* label = new QLabel(value.at(1), metadata);
        label->setObjectName("metadataValue");
        label->setTextFormat(Qt::PlainText);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setWordWrap(true);
        metadata->setItemWidget(item, 1, label);
    }
}
