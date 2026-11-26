#include "proxy_widget.hpp"
#include "theme.hpp"
#include "proxy.hpp"
#include "project.hpp"
#include <QSet>

#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <cmath>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QFrame>
#include <QLineEdit>
#include <QPainter>
#include <QMouseEvent>
#include <QStyledItemDelegate>
#include <QFontDatabase>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtConcurrentRun>

namespace {
class ClipDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return {180, 40}; }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const auto color = [](const char* name) { return QColor(OpenRollTheme::tokens.value(name)); };
        const bool enabled = index.flags().testFlag(Qt::ItemIsEnabled);
        const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        const QRect row = option.rect.adjusted(0, 1, 0, -1);
        painter->setPen(Qt::NoPen);
        painter->setBrush(option.state & QStyle::State_Selected ? color("@accentSoft")
                          : option.state & QStyle::State_MouseOver ? color("@raised") : Qt::transparent);
        painter->drawRoundedRect(row, 10, 10);
        const QRect box(row.left() + 8, row.center().y() - 8, 16, 16);
        painter->setPen(QPen(color(checked ? "@accent" : "@line")));
        painter->setBrush(checked ? color("@accent") : Qt::transparent);
        painter->drawRoundedRect(box, 4, 4);
        if (checked) {
            painter->setPen(QPen(color("@white"), 2));
            painter->drawPolyline(QPolygon{box.topLeft() + QPoint(3, 8), box.topLeft() + QPoint(7, 12),
                                           box.topLeft() + QPoint(13, 4)});
        }
        const auto detail = enabled ? index.data(Qt::UserRole + 2).toString() : QString("Offline");
        auto small = option.font;
        small.setPixelSize(11);
        painter->setFont(small);
        const int detailWidth = painter->fontMetrics().horizontalAdvance(detail);
        painter->setPen(color(enabled ? "@muted" : "@disabled"));
        painter->drawText(row.adjusted(0, 0, -8, 0), Qt::AlignRight | Qt::AlignVCenter, detail);
        painter->setFont(option.font);
        painter->setPen(color(enabled ? "@text" : "@disabled"));
        const auto title = row.adjusted(32, 0, -detailWidth - 16, 0);
        painter->drawText(title, Qt::AlignVCenter, option.fontMetrics.elidedText(index.data().toString(),
                          Qt::ElideMiddle, title.width()));
        painter->restore();
    }
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override
    {
        if (event->type() == QEvent::MouseButtonRelease &&
            index.flags().testFlag(Qt::ItemIsEnabled) && index.flags().testFlag(Qt::ItemIsUserCheckable)) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton &&
                QRect(option.rect.left() + 8, option.rect.center().y() - 8, 16, 16).contains(mouse->pos()))
                return model->setData(index, index.data(Qt::CheckStateRole).toInt() == Qt::Checked
                    ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
            return false;
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
};
}

ProxyWidget::ProxyWidget(std::function<QString()> projectPath, QWidget* parent)
    : QWidget(parent), currentProject(std::move(projectPath))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    auto makeCard = [this] {
        auto* card = new QFrame(this);
        card->setObjectName("card");
        auto* contents = new QVBoxLayout(card);
        contents->setContentsMargins(16, 16, 16, 16);
        contents->setSpacing(8);
        return contents;
    };
    auto* panes = new QHBoxLayout;
    panes->setSpacing(8);
    layout->addLayout(panes, 1);
    auto* clips = makeCard();
    auto* settings = makeCard();
    settings->parentWidget()->setFixedWidth(300);
    panes->addWidget(clips->parentWidget(), 1);
    panes->addWidget(settings->parentWidget());
    auto* heading = new QHBoxLayout;
    heading->addWidget(sectionLabel("Clips", this));
    heading->addStretch();
    auto* count = new QLabel(this);
    count->setObjectName("quiet");
    heading->addWidget(count);
    auto* selectAll = new QPushButton("Select all", this);
    setAction(selectAll, "link");
    heading->addWidget(selectAll);
    clips->addLayout(heading);
    sourceStatus = new QLabel(this);
    sourceStatus->setObjectName("quiet");
    sourceStatus->setTextFormat(Qt::PlainText);
    sourceStatus->setWordWrap(true);
    clips->addWidget(sourceStatus);
    queue = new QListWidget(this);
    queue->setAccessibleName("Media to export");
    queue->setItemDelegate(new ClipDelegate(queue));
    queue->setMouseTracking(true);
    clips->addWidget(queue, 1);
    auto* controls = new QHBoxLayout;
    auto* add = new QPushButton("Add files…", this);
    auto* remove = new QPushButton("Remove added file", this);
    setAction(remove, "ghost");
    controls->addWidget(add);
    controls->addWidget(remove);
    controls->addStretch();
    clips->addLayout(controls);

    settings->setSpacing(12);
    settings->addWidget(sectionLabel("Export settings", this));
    auto field = [this, settings](const QString& text, QWidget* widget) {
        auto* group = new QVBoxLayout;
        group->setSpacing(4);
        auto* caption = new QLabel(text, this);
        caption->setObjectName("quiet");
        caption->setBuddy(widget);
        group->addWidget(caption);
        group->addWidget(widget);
        settings->addLayout(group);
    };
    auto* preset = new QComboBox(this);
    preset->setAccessibleName("Export preset");
    preset->addItems({"ProRes Proxy · MOV", "ProRes 422 LT · MOV", "ProRes 422 · MOV",
                      "ProRes 422 HQ · MOV", "ProRes 4444 · MOV", "ProRes 4444 XQ · MOV",
                      "Avid DNxHR LB · MOV", "Avid DNxHR SQ · MOV", "Avid DNxHR HQ · MOV",
                      "Avid DNxHR HQX · MOV", "H.264 · MKV", "H.264 · MP4", "PCM audio · WAV"});
    field("Preset", preset);
    auto* resolution = new QComboBox(this);
    for (const auto height : {720, 1080, 2160})
        resolution->addItem(QString("Up to %1 px").arg(height), height);
    resolution->setCurrentIndex(1);
    field("Max height", resolution);
    auto* outputRow = new QWidget(this);
    auto* outputLayout = new QHBoxLayout(outputRow);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    auto* destination = new QLineEdit(outputRow);
    destination->setReadOnly(true);
    destination->setPlaceholderText("Not set");
    auto* output = new QPushButton("Choose…", outputRow);
    outputLayout->addWidget(destination, 1);
    outputLayout->addWidget(output);
    field("Output folder", outputRow);
    auto* summaryLabel = new QLabel(this);
    summaryLabel->setObjectName("quiet");
    summaryLabel->setWordWrap(true);
    settings->addWidget(summaryLabel);
    settings->addStretch();
    auto* reason = new QLabel(this);
    reason->setObjectName("quiet");
    settings->addWidget(reason);
    auto* run = new QPushButton("Export", this);
    run->setFixedHeight(40);
    setAction(run, "primary");
    settings->addWidget(run);
    auto* cancel = new QPushButton("Cancel", this);
    cancel->setFixedHeight(40);
    settings->addWidget(cancel);

    auto* jobs = makeCard();
    layout->addWidget(jobs->parentWidget());
    jobs->addWidget(sectionLabel("Queue", this));
    auto* progressLabel = new QLabel("Exports appear here.", this);
    progressLabel->setObjectName("quiet");
    progressLabel->setTextFormat(Qt::PlainText);
    progressLabel->setWordWrap(true);
    jobs->addWidget(progressLabel);
    auto* bar = new QProgressBar(this);
    bar->setTextVisible(false);
    bar->hide();
    jobs->addWidget(bar);
    auto* results = new QTextEdit(this);
    results->setObjectName("exportResults");
    results->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    results->setReadOnly(true);
    results->setMaximumHeight(160);
    results->hide();
    jobs->addWidget(results);

    auto refresh = [=] {
        int checked = 0;
        for (int i = 0; i < queue->count(); ++i)
            if (queue->item(i)->flags().testFlag(Qt::ItemIsEnabled) &&
                queue->item(i)->checkState() == Qt::Checked) ++checked;
        count->setText(QString("%1 clips · %2 selected").arg(queue->count()).arg(checked));
        const auto selected = QString("%1 %2").arg(checked).arg(checked == 1 ? "clip" : "clips");
        const auto problem = !checked ? QString("Select at least one clip")
                           : !QFileInfo(destination->text()).isDir() ? QString("Choose an output folder") : QString{};
        summaryLabel->setText(!checked ? "No clips selected" : selected + " · " + preset->currentText() +
            (resolution->isEnabled() ? " · " + resolution->currentText().toLower() : QString{}));
        reason->setText(problem);
        reason->setVisible(!exporting && !problem.isEmpty());
        run->setText(problem.isEmpty() ? "Export " + selected : "Export");
        run->setEnabled(!exporting && problem.isEmpty());
        selectAll->setEnabled(!exporting && checked < queue->count());
    };
    connect(queue, &QListWidget::itemChanged, this, refresh);
    connect(queue->model(), &QAbstractItemModel::rowsInserted, this, refresh);
    connect(queue->model(), &QAbstractItemModel::rowsRemoved, this, refresh);
    connect(destination, &QLineEdit::textChanged, this, refresh);
    connect(resolution, &QComboBox::currentIndexChanged, this, refresh);
    connect(selectAll, &QPushButton::clicked, this, [=] {
        for (int i = 0; i < queue->count(); ++i)
            if (queue->item(i)->flags().testFlag(Qt::ItemIsEnabled)) queue->item(i)->setCheckState(Qt::Checked);
    });

    connect(add, &QPushButton::clicked, this, [=] {
        const auto files = QFileDialog::getOpenFileNames(this, "Choose media files", {}, "Media (*.braw *.BRAW *.mp4 *.mov *.mkv *.mxf *.avi *.webm *.wav *.mp3 *.flac *.m4a *.aiff *.ogg);;All files (*)");
        for (const auto& file : files) {
            QListWidgetItem* existing = nullptr;
            for (int i = 0; i < queue->count(); ++i)
                if (queue->item(i)->data(Qt::UserRole).toString() == file) existing = queue->item(i);
            if (existing) {
                if (existing->flags().testFlag(Qt::ItemIsEnabled)) existing->setCheckState(Qt::Checked);
                continue;
            }
            auto* entry = new QListWidgetItem(QFileInfo(file).fileName(), queue);
            entry->setData(Qt::UserRole, file);
            entry->setToolTip(file);
            entry->setCheckState(Qt::Checked);
        }
    });
    auto updateRemove = [=] {
        const auto* item = queue->currentItem();
        remove->setEnabled(!exporting && item && !item->data(Qt::UserRole + 1).toBool());
    };
    connect(queue, &QListWidget::currentItemChanged, this, updateRemove);
    connect(remove, &QPushButton::clicked, this, [=] {
        auto* item = queue->currentItem();
        if (item && !item->data(Qt::UserRole + 1).toBool()) delete item;
    });
    connect(output, &QPushButton::clicked, this, [=] {
        const auto path = QFileDialog::getExistingDirectory(this, "Choose export folder");
        if (!path.isEmpty()) destination->setText(path);
    });
    connect(cancel, &QPushButton::clicked, this, [this, cancel] {
        cancelled.store(true);
        cancel->setEnabled(false);
    });
    auto active = [=](bool busy) {
        exporting = busy;
        for (auto* button : {add, output, run}) button->setEnabled(!busy);
        queue->setEnabled(!busy);
        updateRemove();
        preset->setEnabled(!busy);
        resolution->setEnabled(!busy && preset->currentIndex() != int(ExportPreset::Wav));
        cancel->setVisible(busy);
        cancel->setEnabled(busy);
        run->setVisible(!busy);
        bar->setVisible(busy);
        refresh();
    };
    active(false);
    connect(preset, &QComboBox::currentIndexChanged, this, [=] { active(false); });
    auto* watch = new QFutureWatcher<QStringList>(this);
    connect(watch, &QFutureWatcher<QStringList>::finished, this, [=] {
        const auto summary = watch->result();
        int succeeded = 0;
        for (const auto& line : summary) if (line.startsWith("Exported ")) ++succeeded;
        progressLabel->setText(cancelled.load() ? "Export cancelled"
            : QString("%1 exported · %2 failed").arg(succeeded).arg(summary.size() - succeeded));
        results->clear();
        for (const auto& line : summary) {
            results->setTextColor(QColor(OpenRollTheme::tokens.value(line.startsWith("Exported ") ? "@success" : "@danger")));
            results->append(line.toHtmlEscaped());
        }
        results->setVisible(!summary.isEmpty());
        if (!bar->maximum()) { bar->setRange(0, 100); bar->setValue(0); }
        active(false);
        refreshProjectClips();
    });
    connect(run, &QPushButton::clicked, this, [=] {
        if (!queue->count() || !QFileInfo(destination->text()).isDir()) {
            results->setPlainText("Choose media and an output folder.");
            return;
        }
        const auto folder = QDir(destination->text());
        const auto selectedPreset = ExportPreset(preset->currentIndex());
        const auto height = resolution->currentData().toInt();
        QStringList files;
        for (int i = 0; i < queue->count(); ++i) {
            const auto* item = queue->item(i);
            if (item->flags().testFlag(Qt::ItemIsEnabled) && item->checkState() == Qt::Checked)
                files << item->data(Qt::UserRole).toString();
        }
        if (files.isEmpty()) {
            results->setPlainText("Check at least one available clip or add a file.");
            return;
        }
        cancelled.store(false);
        bar->setRange(0, 100);
        bar->setValue(0);
        progressLabel->setText("Preparing export…");
        bar->setFormat("%p%");
        results->hide();
        active(true);
        watch->setFuture(QtConcurrent::run(&worker, [=] {
            QStringList summary;
            for (int i = 0; i < files.size(); ++i) {
                const auto& file = files.at(i);
                if (cancelled.load()) break;
                const auto name = QFileInfo(file).completeBaseName() + exportSuffix(selectedPreset);
                const auto path = folder.filePath(name);
                QElapsedTimer clock;
                clock.start();
                qint64 updated = -150;
                QString previousPhase;
                try {
                    transcodeMedia(file, path, height, selectedPreset, cancelled, [&](qint64 done, qint64 total, const QString& phase) {
                        const auto ms = clock.elapsed();
                        if (ms - updated < 150 && phase == previousPhase) return;
                        updated = ms;
                        previousPhase = phase;
                        QString eta;
                        if (total > 0 && done > 0 && done < total && ms >= 1000) {
                            const auto sec = qint64(std::ceil((ms / 1000.0) * (double(total - done) / done)));
                            eta = sec >= 60 ? QString(" · about %1 min %2 s left").arg(sec / 60).arg(sec % 60)
                                            : QString(" · about %1 s left").arg(sec);
                        }
                        QMetaObject::invokeMethod(this, [=] {
                            if (cancelled.load()) return;
                            bar->setRange(0, total > 0 ? 100 : 0);
                            if (total > 0) bar->setValue(qMin(99, int(100.0 * done / total)));
                            progressLabel->setText(QString("%1 of %2 · %3 · %4")
                                .arg(i + 1).arg(files.size()).arg(name, phase) + eta);
                        }, Qt::QueuedConnection);
                    });
                    summary << "Exported " + path;
                    QMetaObject::invokeMethod(this, [=] {
                        bar->setRange(0, 100);
                        bar->setValue(100);
                    }, Qt::QueuedConnection);
                } catch (const std::exception& error) {
                    summary << name + " · " + QString::fromUtf8(error.what());
                }
            }
            if (cancelled.load()) summary << "Queue cancelled.";
            return summary;
        }));
    });
}

ProxyWidget::~ProxyWidget()
{
    cancelled.store(true);
    worker.waitForDone();
}


void ProxyWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshProjectClips();
}

void ProxyWidget::refreshProjectClips()
{
    if (exporting) return;
    const auto path = currentProject();
    QSet<QString> checked;
    for (int i = queue->count() - 1; i >= 0; --i) {
        auto* item = queue->item(i);
        if (!item->data(Qt::UserRole + 1).toBool()) continue;
        if (path == displayedProject && item->checkState() == Qt::Checked)
            checked.insert(item->data(Qt::UserRole).toString());
        delete item;
    }
    displayedProject = path;
    if (path.isEmpty()) {
        sourceStatus->setText("Open a project to see its clips, or add files here.");
        return;
    }
    try {
        Project project(path, false);
        const auto clips = project.clips();
        sourceStatus->setText(clips.isEmpty() ? "Add a folder in Media to link project clips."
                                            : "Check the project clips you want to export.");
        int row = 0;
        for (const auto& clip : clips) {
            const auto& clipPath = clip.path;
            const QFileInfo file(clipPath);
            const bool available = file.isFile() && file.isReadable();
            for (int i = queue->count() - 1; i >= row; --i) {
                auto* external = queue->item(i);
                if (external->data(Qt::UserRole).toString() != clipPath) continue;
                if (external->checkState() == Qt::Checked) checked.insert(clipPath);
                delete external;
            }
            auto* item = new QListWidgetItem(file.fileName());
            item->setData(Qt::UserRole, clipPath);
            item->setData(Qt::UserRole + 1, true);
            item->setToolTip(clipPath);
            const auto detail = clip.duration < 0 ? clip.codec :
                clip.codec + QString(" · %1 s").arg(clip.duration, 0, 'f', 1);
            item->setData(Qt::UserRole + 2, detail);
            item->setCheckState(available && checked.contains(clipPath) ? Qt::Checked : Qt::Unchecked);
            if (!available) item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            queue->insertItem(row++, item);
        }
    } catch (const std::exception& error) {
        sourceStatus->setText("Could not load project clips. " + QString::fromUtf8(error.what()));
    }
}
