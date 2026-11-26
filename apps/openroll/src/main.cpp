#include "playback.hpp"
#include "ingest_widget.hpp"
#include "media.hpp"
#include "proxy_widget.hpp"
#include "theme.hpp"

#include <QButtonGroup>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QGridLayout>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QListWidget>
#include <QMimeData>
#include <QSettings>
#include <QUrl>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QTemporaryDir>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

namespace {
QString droppedProject(const QMimeData* mime)
{
    const auto urls = mime->urls();
    if (urls.size() != 1 || !urls.front().isLocalFile()) return {};
    const auto path = urls.front().toLocalFile();
    const QFileInfo info(path);
    return !info.isDir() && info.suffix().compare("openroll", Qt::CaseInsensitive) == 0 ? path : QString{};
}

QStringList droppedMedia(const QMimeData* mime)
{
    QStringList paths;
    for (const auto& url : mime->urls()) {
        if (!url.isLocalFile()) return {};
        const auto path = url.toLocalFile();
        const QFileInfo info(path);
        if (info.isFile() && info.suffix().compare("openroll", Qt::CaseInsensitive) == 0) return {};
        if (!paths.contains(path)) paths << path;
    }
    return paths;
}

bool acceptLocalDrop(QDropEvent* event)
{
    const auto actions = event->possibleActions();
    if (actions.testFlag(Qt::CopyAction)) event->setDropAction(Qt::CopyAction);
    else if (actions.testFlag(Qt::LinkAction)) event->setDropAction(Qt::LinkAction);
    else return false;
    event->accept();
    return true;
}

class OpenRollWindow final : public QMainWindow {
public:
    OpenRollWindow()
    {
        setAcceptDrops(true);
        qApp->installEventFilter(this);
    }
    std::function<void(const QString&)> openProject;
    std::function<bool()> acceptsMedia;
    std::function<void(const QStringList&)> addMedia;
protected:
    bool eventFilter(QObject* target, QEvent* event) override
    {
        const auto type = event->type();
        if (type != QEvent::DragEnter && type != QEvent::DragMove && type != QEvent::Drop)
            return false;
        auto* widget = qobject_cast<QWidget*>(target);
        if (!widget || widget == this || widget->window() != this) return false;
        auto* drop = static_cast<QDropEvent*>(event);
        if (widget->objectName() == "sourceDrop" && droppedMedia(drop->mimeData()).size() == 1)
            return false; // Keep native highlighting and single-source indexing.
        if (!acceptDrop(drop)) return false;
        if (type == QEvent::Drop) dropEvent(drop);
        return true;
    }
    void dragEnterEvent(QDragEnterEvent* event) override { acceptDrop(event); }
    void dragMoveEvent(QDragMoveEvent* event) override { acceptDrop(event); }
    void dropEvent(QDropEvent* event) override
    {
        if (!acceptDrop(event)) return;
        const auto project = droppedProject(event->mimeData());
        if (!project.isEmpty()) {
            QTimer::singleShot(0, this, [this, project] { openProject(project); });
        } else {
            const auto paths = droppedMedia(event->mimeData());
            QTimer::singleShot(0, this, [this, paths] { addMedia(paths); });
        }
    }
private:
    bool acceptDrop(QDropEvent* event)
    {
        const auto* mime = event->mimeData();
        if (droppedProject(mime).isEmpty() && (!acceptsMedia() || droppedMedia(mime).isEmpty())) return false;
        return acceptLocalDrop(event);
    }
};

QPixmap workspacePixmap(int index, const QColor& color)
{
    QPixmap bitmap(32, 32);
    bitmap.setDevicePixelRatio(2);
    bitmap.fill(Qt::transparent);
    QPainter p(&bitmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(16.0 / 24, 16.0 / 24);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (index == 0) {
        p.drawLine(12, 3, 12, 14);
        p.drawPolyline(QPolygon{{7, 10}, {12, 15}, {17, 10}});
        p.drawPolyline(QPolygon{{4, 15}, {4, 20}, {20, 20}, {20, 15}});
    } else if (index == 1) {
        p.drawRoundedRect(QRectF(3, 4, 18, 16), 2, 2);
        for (int x : {7, 17}) {
            p.drawLine(x, 4, x, 20);
            for (int y : {9, 15}) p.drawLine(x < 12 ? 3 : 17, y, x < 12 ? 7 : 21, y);
        }
    } else if (index == 2) {
        p.drawRoundedRect(QRectF(2.5, 3.5, 19, 13), 1.5, 1.5);
        p.drawLine(12, 17, 12, 21);
        p.drawLine(8, 21, 16, 21);
    } else {
        p.drawPolyline(QPolygon{{4, 8}, {20, 8}, {16, 4}});
        p.drawPolyline(QPolygon{{20, 16}, {4, 16}, {8, 20}});
    }
    return bitmap;
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    std::unique_ptr<QTemporaryDir> testDirectory;
    if (app.arguments().contains("--media-drop-test")) {
        testDirectory = std::make_unique<QTemporaryDir>();
        if (!testDirectory->isValid()) return 1;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, testDirectory->path());
    }
    QApplication::setApplicationName("OpenRoll");
    applyTheme(app);

    OpenRollWindow window;
    window.setWindowTitle("OpenRoll");
    auto* shell = new QWidget(&window);
    auto* shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    auto* header = new QWidget(shell);
    header->setObjectName("appHeader");
    header->setFixedHeight(48);
    auto* headerLayout = new QGridLayout(header);
    headerLayout->setContentsMargins(16, 0, 16, 0);
    headerLayout->setColumnStretch(0, 1);
    headerLayout->setColumnStretch(2, 1);
    headerLayout->setColumnMinimumWidth(0, 240);
    headerLayout->setColumnMinimumWidth(2, 240);
    auto* brand = new QWidget(header);
    auto* brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(8);
    auto* name = new QLabel("OpenRoll", header);
    name->setObjectName("appName");
    auto* project = new QToolButton(header);
    project->setObjectName("projectMenu");
    project->setText("No project");
    project->setFixedHeight(32);
    project->setMaximumWidth(240);
    project->setProperty("emptyProject", true);
    project->setPopupMode(QToolButton::InstantPopup);
    project->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto* menu = new QMenu(project);
    project->setMenu(menu);
    QPixmap mark(40, 40);
    mark.setDevicePixelRatio(2);
    mark.fill(Qt::transparent);
    {
        QPainter painter(&mark);
        painter.setRenderHint(QPainter::Antialiasing);
        const QList<QColor> colors{QColor(OpenRollTheme::tokens.value("@muted")), QColor(OpenRollTheme::tokens.value("@text")), QColor(OpenRollTheme::tokens.value("@text"))};
        for (int i = 0; i < colors.size(); ++i) {
            painter.setPen(QPen(colors[i], 2.5, Qt::SolidLine, Qt::FlatCap));
            painter.drawArc(QRectF(2.5, 2.5, 15, 15), (28 + i * 120) * 16, 103 * 16);
        }
    }
    auto* logo = new QLabel(header);
    logo->setPixmap(mark);
    brandLayout->addWidget(logo);
    brandLayout->addWidget(name);
    brandLayout->addStretch();
    headerLayout->addWidget(brand, 0, 0);
    headerLayout->addWidget(project, 0, 2, Qt::AlignRight);
    shellLayout->addWidget(header);

    auto* pages = new QStackedWidget(shell);
    auto* playback = new Playback(pages);
    auto* closeProject = new QAction("Close project", menu);
    closeProject->setEnabled(false);
    QSettings settings("OpenRoll", "OpenRoll");
    MediaWorkspace* media = nullptr;
    std::function<void()> refreshRecent;
    media = new MediaWorkspace(playback, [&, project, closeProject](const QString& text) {
        project->setText(text.isEmpty() ? "No project" : project->fontMetrics().elidedText(text, Qt::ElideMiddle, 200));
        project->setToolTip(text);
        project->setProperty("emptyProject", text.isEmpty());
        project->style()->unpolish(project);
        project->style()->polish(project);
        closeProject->setEnabled(!text.isEmpty());
        if (!text.isEmpty()) {
            const auto path = QFileInfo(media->projectPath()).absoluteFilePath();
            auto recent = settings.value("recentProjects").toList();
            for (qsizetype i = recent.size(); i-- > 0;)
                if (recent.at(i).toMap().value("path").toString() == path) recent.removeAt(i);
            recent.prepend(QVariantMap{{"path", path}, {"openedAt", QDateTime::currentDateTime()}});
            while (recent.size() > 8) recent.removeLast();
            settings.setValue("recentProjects", recent);
        }
        if (refreshRecent) refreshRecent();
    }, pages);
    auto* ingest = new IngestWidget([media] { return media->projectPath(); }, [media](const QString& path) {
        if (media->projectPath() == path) media->refreshMedia();
    }, [media](bool create) { media->chooseProject(create); }, pages);
    auto* exports = new ProxyWidget([media] { return media->projectPath(); }, pages);
    const QList<QWidget*> workspaces{ingest, media, media, exports};
    const QStringList labels{"Ingest", "Media", "Monitor", "Transcode"};
    for (auto* workspace : {static_cast<QWidget*>(ingest), static_cast<QWidget*>(media),
                            static_cast<QWidget*>(exports)}) pages->addWidget(workspace);
    shellLayout->addWidget(pages, 1);

    auto* nav = new QWidget(header);
    nav->setObjectName("workspaceSwitch");
    nav->setFixedHeight(36);
    auto* navLayout = new QHBoxLayout(nav);
    navLayout->setContentsMargins(3, 3, 3, 3);
    navLayout->setSpacing(0);
    auto* group = new QButtonGroup(nav);
    for (int i = 0; i < labels.size(); ++i) {
        auto* button = new QToolButton(nav);
        button->setObjectName("workspaceButton");
        button->setCheckable(true);
        button->setText(labels[i]);
        QIcon icon;
        icon.addPixmap(workspacePixmap(i, QColor(OpenRollTheme::tokens.value("@muted"))), QIcon::Normal, QIcon::Off);
        icon.addPixmap(workspacePixmap(i, QColor(OpenRollTheme::tokens.value("@text"))), QIcon::Normal, QIcon::On);
        button->setIcon(icon);
        button->setIconSize(QSize(16, 16));
        button->setFixedHeight(30);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setToolTip(QString("%1 (Ctrl+%2)").arg(labels[i]).arg(i + 1));
        button->setAccessibleName(labels[i] + " workspace");
        group->addButton(button, i);
        navLayout->addWidget(button);
        auto* shortcut = new QShortcut(QKeySequence(QString("Ctrl+%1").arg(i + 1)), &window);
        QObject::connect(shortcut, &QShortcut::activated, group, [=] { if (nav->isVisible()) group->button(i)->click(); });
    }
    QObject::connect(group, &QButtonGroup::idClicked, pages, [=](int i) {
        if (i == 1 || i == 2) media->setMonitorMode(i == 2);
        pages->setCurrentWidget(workspaces[i]);
    });
    headerLayout->addWidget(nav, 0, 1, Qt::AlignCenter);
    auto* welcome = new QWidget(pages);
    auto* welcomeLayout = new QVBoxLayout(welcome);
    welcomeLayout->setContentsMargins(24, 24, 24, 24);
    welcomeLayout->setSpacing(16);
    welcomeLayout->addStretch(2);
    auto* column = new QWidget(welcome);
    column->setFixedWidth(480);
    auto* content = new QVBoxLayout(column);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(8);
    auto* welcomeLogo = new QLabel(column);
    welcomeLogo->setPixmap(mark.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    content->addWidget(welcomeLogo, 0, Qt::AlignHCenter);
    auto* heading = new QLabel("OpenRoll", column);
    heading->setObjectName("welcomeName");
    content->addWidget(heading, 0, Qt::AlignHCenter);
    content->addSpacing(16);
    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    auto* createProject = new QPushButton("New project…", column);
    auto* openProject = new QPushButton("Open project…", column);
    createProject->setToolTip("New project (Ctrl+N)");
    openProject->setToolTip("Open project (Ctrl+O)");
    createProject->setFixedHeight(40);
    openProject->setFixedHeight(40);
    setAction(createProject, "primary");
    actions->addWidget(createProject, 1);
    actions->addWidget(openProject, 1);
    content->addLayout(actions);
    content->addSpacing(16);
    content->addWidget(sectionLabel("Recent projects", column));
    auto* recentList = new QListWidget(column);
    recentList->setObjectName("recentList");
    recentList->setSpacing(2);
    content->addWidget(recentList);
    auto* noRecent = new QLabel("No recent projects yet", column);
    noRecent->setObjectName("recentEmpty");
    noRecent->setAlignment(Qt::AlignCenter);
    noRecent->setFixedHeight(56);
    content->addWidget(noRecent);
    auto* standalone = new QPushButton("Continue without a project", column);
    setAction(standalone, "link");
    content->addSpacing(16);
    content->addWidget(standalone, 0, Qt::AlignHCenter);
    welcomeLayout->addWidget(column, 0, Qt::AlignHCenter);
    welcomeLayout->addStretch(3);
    pages->addWidget(welcome);
    pages->setCurrentWidget(welcome);
    nav->hide();
    project->hide();

    const auto leaveWelcome = [=](int workspace) {
        nav->show();
        project->show();
        group->button(workspace)->click();
    };
    const auto selectProject = [=](bool create) {
        media->chooseProject(create);
        if (!media->projectPath().isEmpty()) leaveWelcome(1);
    };
    window.acceptsMedia = [=] {
        return pages->currentWidget() == ingest || group->checkedId() == 1;
    };
    window.addMedia = [=, &window](const QStringList& paths) {
        if (pages->currentWidget() == ingest && paths.size() == 1) {
            ingest->indexSource(paths.front());
            return;
        }
        if (media->projectPath().isEmpty()) {
            QMessageBox prompt(QMessageBox::Question, "Add media to a project",
                               "Create or open a project to add the dropped footage.",
                               QMessageBox::Cancel, &window);
            auto* create = prompt.addButton("New project…", QMessageBox::AcceptRole);
            auto* open = prompt.addButton("Open project…", QMessageBox::ActionRole);
            prompt.exec();
            if (prompt.clickedButton() == create || prompt.clickedButton() == open)
                selectProject(prompt.clickedButton() == create);
            if (media->projectPath().isEmpty()) return;
        }
        if (pages->currentWidget() == ingest) leaveWelcome(1);
        media->linkMedia(paths);
    };
    const auto registerAction = [&](const QString& title, const QString& shortcut, auto callback) {
        auto* action = menu->addAction(title);
        action->setShortcut(QKeySequence(shortcut));
        window.addAction(action);
        QObject::connect(action, &QAction::triggered, &window, callback);
    };
    registerAction("New project…", "Ctrl+N", [=] { selectProject(true); });
    registerAction("Open project…", "Ctrl+O", [=] { selectProject(false); });
    auto* recentMenu = menu->addMenu("Recent projects");
    recentMenu->setToolTipsVisible(true);
    window.openProject = [=](const QString& path) {
        media->openProjectFile(path);
        if (media->projectPath() == QFileInfo(path).absoluteFilePath()) leaveWelcome(1);
    };
    refreshRecent = [&, recentList, noRecent, recentMenu] {
        recentMenu->clear();
        recentList->clear();
        const auto entries = settings.value("recentProjects").toList();
        for (const auto& entry : entries) {
            const auto record = entry.toMap();
            const auto path = record.value("path").toString();
            const QFileInfo file(path);
            const bool exists = file.isFile();
            auto* action = recentMenu->addAction(file.completeBaseName());
            action->setToolTip(path);
            action->setEnabled(exists);
            QObject::connect(action, &QAction::triggered, &window, [&, path] { window.openProject(path); });
            if (recentList->count() == 6) continue;
            auto* item = new QListWidgetItem(recentList);
            item->setSizeHint(QSize(0, 56));
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
            auto* row = new QWidget;
            row->setObjectName("recentRow");
            row->setAttribute(Qt::WA_TransparentForMouseEvents);
            auto* layout = new QGridLayout(row);
            layout->setContentsMargins(12, 4, 12, 4);
            layout->setVerticalSpacing(0);
            auto* name = new QLabel(row);
            name->setText(name->fontMetrics().elidedText(file.completeBaseName(), Qt::ElideMiddle, 340));
            name->setTextFormat(Qt::PlainText);
            auto* folder = new QLabel(row);
            folder->setObjectName("quiet");
            folder->setTextFormat(Qt::PlainText);
            folder->setText(folder->fontMetrics().elidedText(file.absolutePath(), Qt::ElideMiddle, 340));
            const auto date = record.value("openedAt").toDateTime().date();
            const auto today = QDate::currentDate();
            auto* when = new QLabel(!exists ? "Not found" : date == today ? "Today"
                : date == today.addDays(-1) ? "Yesterday" : date.toString("MMM d"), row);
            when->setObjectName("quiet");
            name->setEnabled(exists);
            folder->setEnabled(exists);
            layout->addWidget(name, 0, 0);
            layout->addWidget(folder, 1, 0);
            layout->addWidget(when, 0, 1, 2, 1, Qt::AlignRight);
            layout->setColumnStretch(0, 1);
            recentList->setItemWidget(item, row);
        }
        recentList->setFixedHeight(recentList->count() * 60);
        recentList->setVisible(!entries.isEmpty());
        noRecent->setVisible(entries.isEmpty());
        recentMenu->addSeparator();
        auto* clear = recentMenu->addAction("Clear recent");
        QObject::connect(clear, &QAction::triggered, &window, [&] {
            settings.remove("recentProjects");
            refreshRecent();
        });
        recentMenu->setEnabled(!entries.isEmpty());
    };
    const auto activateRecent = [&window](QListWidgetItem* item) {
        const auto path = item->data(Qt::UserRole).toString();
        if (QFileInfo(path).isFile()) window.openProject(path);
    };
    QObject::connect(recentList, &QListWidget::itemClicked, &window, activateRecent);
    QObject::connect(recentList, &QListWidget::itemActivated, &window, activateRecent);
    recentList->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(recentList, &QListWidget::customContextMenuRequested, &window, [&, recentList](const QPoint& point) {
        auto* item = recentList->itemAt(point);
        if (!item) return;
        const auto path = item->data(Qt::UserRole).toString();
        if (QFileInfo(path).isFile()) return;
        QMenu removal;
        auto* remove = removal.addAction("Remove from list");
        if (removal.exec(recentList->viewport()->mapToGlobal(point)) != remove) return;
        auto entries = settings.value("recentProjects").toList();
        for (qsizetype i = entries.size(); i-- > 0;)
            if (entries.at(i).toMap().value("path").toString() == path) entries.removeAt(i);
        settings.setValue("recentProjects", entries);
        refreshRecent();
    });
    refreshRecent();
    menu->addSeparator();
    registerAction("Open clip…", "Ctrl+Shift+O", [&window, media, leaveWelcome] {
        const auto file = QFileDialog::getOpenFileName(&window, "Open clip", {},
            "Media (*.braw *.BRAW *.mp4 *.mov *.mxf *.mkv *.wav *.flac *.mp3 *.m4a);;All files (*)");
        if (!file.isEmpty()) {
            leaveWelcome(2);
            media->openClip(file);
        }
    });
    menu->addSeparator();
    menu->addAction(closeProject);
    QObject::connect(closeProject, &QAction::triggered, media, &MediaWorkspace::closeProject);
    QObject::connect(createProject, &QPushButton::clicked, welcome, [=] { selectProject(true); });
    QObject::connect(openProject, &QPushButton::clicked, welcome, [=] { selectProject(false); });
    QObject::connect(standalone, &QPushButton::clicked, welcome, [=] { leaveWelcome(0); });

    window.setCentralWidget(shell);
    window.resize(1280, 790);
    window.show();

    const auto arguments = app.arguments();
    if (arguments.contains("--smoke-test")) {
        QTimer::singleShot(200, &window, [&] {
            bool okay = window.isVisible() && window.windowHandle() && window.windowHandle()->isExposed();
            okay = okay && pages->currentWidget() == welcome && !nav->isVisible();
            standalone->click();
            okay = okay && pages->currentWidget() == ingest && nav->isVisible();
            for (int i = 0; i < workspaces.size(); ++i) {
                group->button(i)->click();
                okay = okay && pages->currentWidget() == workspaces[i] && group->button(i)->isChecked();
            }
            group->button(2)->click();
            okay = okay && group->button(2)->isChecked();
            if (!okay || !window.close()) app.exit(1);
        });
    } else if (arguments.contains("--media-drop-test")) {
        const auto& fixture = *testDirectory;
        const auto path = fixture.filePath("Active project.openroll");
        const auto unrelated = fixture.filePath("Unrelated project.openroll");
        const auto folder = fixture.filePath("Camera card");
        if (!QDir().mkdir(folder)) return 1;
        const QStringList originals{folder + "/camera.mov", fixture.filePath("sound.wav"),
                                    fixture.filePath("second.mp4")};
        for (const auto& original : originals) {
            QFile input(original);
            if (!input.open(QIODevice::WriteOnly) || input.write("fixture") != 7) return 1;
        }
        Project other(unrelated, true);
        media->openProjectFile(path, true);
        auto sendDrop = [&](QWidget* target, const QList<QUrl>& urls) {
            QMimeData mime;
            mime.setUrls(urls);
            QDragEnterEvent enter(QPoint(5, 5), Qt::LinkAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(target, &enter);
            QDropEvent drop(QPointF(5, 5), Qt::LinkAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(target, &drop);
            return enter.isAccepted() && drop.isAccepted();
        };
        QElapsedTimer deadline;
        deadline.start();
        QTimer poll;
        int phase = 0;
        leaveWelcome(0);
        if (!sendDrop(ingest, {QUrl::fromLocalFile(folder)})) return 1;
        QObject::connect(&poll, &QTimer::timeout, &window, [&] {
            if (deadline.elapsed() > 10000) {
                qCritical("Timed out waiting for project media to be linked (phase %d).", phase);
                app.exit(1);
                return;
            }
            Project reopened(path, false);
            const auto linked = reopened.clipPaths();
            if (phase == 0 && linked.contains(originals[0])) {
                group->button(1)->click();
                auto* target = media->findChild<QListWidget*>("clipStrip");
                if (!target || !sendDrop(target->viewport(), {QUrl::fromLocalFile(originals[1]),
                                                               QUrl::fromLocalFile(originals[2])})) {
                    app.exit(1);
                    return;
                }
                ++phase;
            } else if (phase == 1 && linked.size() == 3) {
                auto* list = media->findChild<QListWidget*>("clipStrip");
                if (!list || list->count() != 3) return; // Wait for the scan completion UI.
                media->closeProject();
                media->openProjectFile(path);
                bool okay = list->count() == 3 && other.clipPaths().isEmpty();
                for (const auto& original : originals) {
                    QFile input(original);
                    okay = okay && linked.contains(original) && input.open(QIODevice::ReadOnly)
                                  && input.readAll() == "fixture";
                }
                app.exit(okay ? 0 : 1);
            }
        });
        poll.start(30);
        return app.exec();
    } else if (arguments.size() == 2) {
        leaveWelcome(2);
        media->openClip(QFileInfo(arguments.at(1)).absoluteFilePath());
    }
    return app.exec();
}
