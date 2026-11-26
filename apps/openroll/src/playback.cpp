#include "playback.hpp"
#include "playback_time.hpp"
#include "theme.hpp"
#include "vlc.hpp"

#include <QAudioDevice>
#include <QAudioSink>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QFontDatabase>
#include <QToolButton>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QIcon>
#include <QIODevice>
#include <QLabel>
#include <QMediaDevices>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QShortcut>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>
#include <cmath>

class VideoSurface final : public QWidget {
public:
    using QWidget::QWidget;
    QImage image;
    QString empty = "Select a clip";
    QSize displaySize;
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
        if (image.isNull()) {
            if (!empty.isEmpty()) {
                painter.setPen(QColor(OpenRollTheme::tokens.value("@muted")));
                painter.drawText(rect(), Qt::AlignCenter, empty);
            }
            return;
        }
        const auto size = displaySize.scaled(this->size(), Qt::KeepAspectRatio);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(QRect(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2),
                                size), image);
    }
};

namespace {
constexpr size_t bufferLimit = 5;

class ClipHeading final : public QLabel {
public:
    using QLabel::QLabel;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        const auto bounds = contentsRect();
        style()->drawItemText(&painter, bounds, Qt::AlignVCenter, palette(), isEnabled(),
                              fontMetrics().elidedText(text(), Qt::ElideMiddle, bounds.width()),
                              QPalette::WindowText);
    }
};

enum class TransportIcon { Previous, Play, Pause, Stop, Next, Fullscreen, Speaker, Metadata };

QIcon transportIcon(TransportIcon symbol, const char* tone = "@text")
{
    QPixmap bitmap(40, 40);
    bitmap.fill(Qt::transparent);
    bitmap.setDevicePixelRatio(2);
    QPainter p(&bitmap);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor ink(OpenRollTheme::tokens.value(tone));
    p.setBrush(ink);
    p.setPen(Qt::NoPen);
    switch (symbol) {
    case TransportIcon::Previous:
        p.translate(20, 0);
        p.scale(-1, 1);
        [[fallthrough]];
    case TransportIcon::Next:
        p.drawPolygon(QPolygonF{{5, 4}, {13, 10}, {5, 16}});
        p.drawRect(QRectF(14, 4, 2, 12));
        break;
    case TransportIcon::Play:
        p.drawPolygon(QPolygonF{{5, 3}, {16, 10}, {5, 17}});
        break;
    case TransportIcon::Pause:
        p.drawRect(QRectF(5, 4, 3.5, 12));
        p.drawRect(QRectF(11.5, 4, 3.5, 12));
        break;
    case TransportIcon::Stop:
        p.drawRect(QRectF(4.5, 4.5, 11, 11));
        break;
    case TransportIcon::Metadata:
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(ink, 1.5));
        p.drawRoundedRect(QRectF(3, 4, 14, 12), 2, 2);
        p.drawLine(12, 4, 12, 16);
        break;
    case TransportIcon::Speaker:
        p.drawPolygon(QPolygonF{{3, 8}, {6, 8}, {10, 4}, {10, 16}, {6, 12}, {3, 12}});
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(ink, 1.5));
        p.drawArc(QRectF(7, 4, 10, 12), -55 * 16, 110 * 16);
        break;
    case TransportIcon::Fullscreen:
        p.setPen(QPen(ink, 1.5, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(QPolygonF{{4, 8}, {4, 4}, {8, 4}});
        p.drawPolyline(QPolygonF{{12, 4}, {16, 4}, {16, 8}});
        p.drawPolyline(QPolygonF{{16, 12}, {16, 16}, {12, 16}});
        p.drawPolyline(QPolygonF{{8, 16}, {4, 16}, {4, 12}});
        break;
    }
    return QIcon(bitmap);
}

void updatePlayIcon(QPushButton* button, bool playing)
{
    static const auto playIcon = transportIcon(TransportIcon::Play, "@onAccent");
    static const auto pauseIcon = transportIcon(TransportIcon::Pause, "@onAccent");
    const auto& icon = playing ? pauseIcon : playIcon;
    if (button->icon().cacheKey() != icon.cacheKey()) button->setIcon(icon);
}

struct Decoded {
    std::shared_ptr<RawDecoder> decoder;
    RawInfo info;
    RawFrame frame;
    QString error;
};
}

Playback::Playback(QWidget* parent) : QWidget(parent)
{
    worker.setMaxThreadCount(1);
    worker.setExpiryTimeout(-1);
    timer.setTimerType(Qt::PreciseTimer);
    timer.setInterval(8);
    connect(&timer, &QTimer::timeout, this, [this] { tick(); });
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);
    auto* toolbar = new QWidget(this);
    toolbar->setFixedHeight(32);
    auto* heading = new QHBoxLayout(toolbar);
    heading->setContentsMargins(0, 0, 0, 0);
    auto* metadata = new QToolButton(toolbar);
    metadata->setObjectName("metadataToggle");
    metadata->setIcon(transportIcon(TransportIcon::Metadata));
    metadata->setIconSize(QSize(16, 16));
    metadata->setFixedSize(28, 28);
    metadata->setCheckable(true);
    metadata->setAccessibleName("Toggle metadata");
    metadata->setToolTip("Metadata (I)");
    status = new ClipHeading(toolbar);
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(false);
    status->setMinimumWidth(0);
    status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    heading->addWidget(status, 1);
    heading->addWidget(metadata);
    layout->addWidget(toolbar);
    surface = new VideoSurface(this);
    surface->setObjectName("videoSurface");
    surface->setMinimumSize(260, 148);
    layout->addWidget(surface, 1);
    timeline = new QSlider(Qt::Horizontal, this);
    timeline->setAccessibleName("Timeline");
    timeline->setRange(0, 10000);
    timeline->setEnabled(false);
    auto seekTimeline = [this] {
        const auto fraction = timeline->sliderPosition() / 10000.0;
        if (vlc) seekConventional(qint64(vlc->duration() * fraction));
        else if (info.frames) seek(qint64(std::llround((info.frames - 1) * fraction)));
    };
    connect(timeline, &QSlider::sliderReleased, this, seekTimeline);
    connect(timeline, &QSlider::actionTriggered, this, [this, seekTimeline](int action) {
        if (!timeline->isSliderDown() && action != QAbstractSlider::SliderMove) seekTimeline();
    });
    timeline->setFixedHeight(20);
    layout->addWidget(timeline);
    auto* controlRow = new QWidget(this);
    controlRow->setFixedHeight(44);
    auto* zones = new QGridLayout(controlRow);
    zones->setContentsMargins(0, 0, 0, 0);
    zones->setHorizontalSpacing(8);
    zones->setColumnStretch(0, 1);
    zones->setColumnStretch(2, 1);
    position = new QLabel(this);
    position->setObjectName("quiet");
    position->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    position->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    position->setAccessibleName("Elapsed time and duration, hours minutes seconds frames");
    zones->addWidget(position, 0, 0);
    auto* bar = new QHBoxLayout;
    bar->setSpacing(8);
    zones->addLayout(bar, 0, 1, Qt::AlignCenter);
    auto* sound = new QHBoxLayout;
    sound->setSpacing(8);
    zones->addLayout(sound, 0, 2, Qt::AlignRight);
    layout->addWidget(controlRow);
    auto addButton = [this, bar](TransportIcon icon, auto action) {
        auto* button = new QPushButton(this);
        button->setIcon(transportIcon(icon));
        button->setIconSize(QSize(20, 20));
        button->setFixedSize(32, 32);
        button->setObjectName("transportButton");
        setAction(button, "ghost");
        bar->addWidget(button);
        connect(button, &QPushButton::clicked, this, action);
        return button;
    };
    auto addShortcut = [this](const QString& key, auto action) {
        auto* shortcut = new QShortcut(QKeySequence(key), this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, action);
    };
    auto* previous = addButton(TransportIcon::Previous, [this] { step(-1); });
    previous->setAccessibleName("Previous frame");
    previous->setToolTip("Previous frame (,)");
    addShortcut(",", [this] { step(-1); });
    play = addButton(TransportIcon::Play, [this] { playPause(); });
    play->setFixedSize(36, 36);
    setAction(play, "primary");
    play->setAccessibleName("Play or pause");
    play->setToolTip("Play or pause (Space)");
    addShortcut("Space", [this] { playPause(); });
    auto* stop = addButton(TransportIcon::Stop, [this] { if (vlc) vlc->stop(); else seek(0); });
    stop->setToolTip("Stop");
    stop->setAccessibleName("Stop");
    auto* next = addButton(TransportIcon::Next, [this] { step(1); });
    next->setAccessibleName("Next frame");
    next->setToolTip("Next frame (.)");
    addShortcut(".", [this] { step(1); });
    auto fullscreen = [this, layout] {
        if (surface->window() != window()) return;
        const int index = layout->indexOf(surface);
        auto* display = new QDialog(this, Qt::Window | Qt::FramelessWindowHint);
        display->setObjectName("fullscreenVideo");
        display->setWindowTitle("OpenRoll video");
        display->setScreen(window()->screen());
        auto* displayLayout = new QVBoxLayout(display);
        displayLayout->setContentsMargins(0, 0, 0, 0);
        displayLayout->addWidget(surface);
        auto* exit = new QShortcut(QKeySequence("F"), display);
        connect(exit, &QShortcut::activated, display, &QDialog::reject);
        auto* togglePlay = new QShortcut(QKeySequence("Space"), display);
        connect(togglePlay, &QShortcut::activated, this, [this] { playPause(); });
        connect(display, &QDialog::finished, this, [this, layout, index, display] {
            layout->insertWidget(index, surface, 1);
            surface->show();
            display->deleteLater();
        });
        display->showFullScreen();
        surface->show();
    };
    auto* full = addButton(TransportIcon::Fullscreen, fullscreen);
    full->setAccessibleName("Fullscreen");
    full->setToolTip("Fullscreen video (F) · Exit with Esc");
    addShortcut("F", fullscreen);
    bar->removeWidget(full);
    full->setFixedSize(28, 28);
    auto* speaker = new QLabel(this);
    speaker->setPixmap(transportIcon(TransportIcon::Speaker, "@muted").pixmap(16, 16));
    speaker->setToolTip("Volume");
    sound->addWidget(speaker);
    volume = new QSlider(Qt::Horizontal, this);
    volume->setAccessibleName("Volume");
    volume->setRange(0, 100);
    volume->setValue(80);
    volume->setFixedWidth(90);
    sound->addWidget(volume);
    sound->addWidget(full);
    connect(volume, &QSlider::valueChanged, this, [this](int value) {
        if (vlc) vlc->volume(value);
        if (audio) audio->setVolume(value / 100.0);
    });
    updateControls();
    timer.start();
}

Playback::~Playback()
{
    timer.stop();
    stopAudio();
    worker.waitForDone();
}

void Playback::open(const QString& file)
{
    ++serial;
    vlc.reset();
    pauseOnFirstVideoFrame = false;
    stopAudio();
    decoder.reset();
    info = {};
    path = file;
    lastError.clear();
    setAction(status, "");
    audioError.clear();
    audioProcessed = 0;
    buffered.clear();
    shown = -1;
    start = requested = 0;
    playing = false;
    clock.invalidate();
    surface->empty.clear();
    surface->image = {};
    surface->update();
    const QFileInfo mediaFile(file);
    if (!mediaFile.isFile() || !mediaFile.isReadable()) {
        failed("Could not read " + mediaFile.fileName());
        return;
    }
    if (mediaFile.suffix().compare("braw", Qt::CaseInsensitive) != 0) {
        try {
            vlc = std::make_unique<VlcPlayback>(mediaFile.absoluteFilePath(), volume->value());
            pauseOnFirstVideoFrame = true;
            status->setText(mediaFile.fileName() + " · VLC");
            updateControls();
        } catch (const std::exception& error) {
            failed(QString::fromUtf8(error.what()));
        }
        return;
    }
    status->setText("Opening " + mediaFile.fileName() + "…");
    status->setToolTip(mediaFile.fileName());
    updateControls();
    requestFrame();
}

void Playback::seek(qint64 frame)
{
    if (!decoder || !info.frames) return;
    ++serial;
    stopAudio();
    buffered.clear();
    playing = false;
    clock.invalidate();
    start = requested = std::clamp(frame, qint64(0), info.frames - 1);
    updateControls();
    requestFrame();
}

bool Playback::isPlaying() const { return vlc ? vlc->playing() : playing; }
qint64 Playback::conventionalPosition() const { return vlc ? vlc->position() : 0; }
qint64 Playback::conventionalDuration() const { return vlc ? vlc->duration() : 0; }
bool Playback::conventionalFrameVisible() const { return vlc && !surface->image.isNull(); }
unsigned Playback::conventionalAudioDecoded() const { return vlc ? vlc->decodedAudio() : 0; }
unsigned Playback::conventionalVideoDecoded() const { return vlc ? vlc->decodedVideo() : 0; }
void Playback::seekConventional(qint64 ms) { if (vlc) vlc->seek(ms); }

void Playback::step(int frames)
{
    if (vlc) { vlc->step(frames); return; }
    seek((!playing) ? start + frames : qMax(qint64(0), shown) + frames);
}

void Playback::playPause()
{
    if (vlc) { pauseOnFirstVideoFrame = false; vlc->playPause(); return; }
    if (!decoder || !info.frames) return;
    if (playing) {
        seek(qMax(qint64(0), shown));
        return;
    }
    ++serial;
    stopAudio();
    clock.invalidate();
    buffered.clear();
    start = requested = (start == info.frames - 1 ? 0 : start);
    playing = true;
    updateControls();
    requestFrame();
}

void Playback::requestFrame()
{
    if (vlc || busy || path.isEmpty()) return;
    if (decoder && (!info.frames || requested >= info.frames || buffered.size() >= bufferLimit ||
                    (!playing && requested != start))) return;

    const auto ticket = serial;
    const auto index = requested++;
    const auto clipPath = path;
    const auto session = decoder;
    const auto withAudio = playing;
    busy = true;
    auto* future = new QFutureWatcher<Decoded>(this);
    connect(future, &QFutureWatcher<Decoded>::finished, this, [this, future, ticket, index] {
        const auto decoded = future->result();
        future->deleteLater();
        busy = false;
        if (ticket == serial) {
            if (!decoded.error.isEmpty()) {
                failed(decoded.error);
                return;
            }
            if (!decoder) {
                decoder = decoded.decoder;
                info = decoded.info;
                status->setText(QFileInfo(path).fileName());
                status->setToolTip(QString("%1  ·  %2 × %3  ·  %4 fps  ·  TC %5")
                                    .arg(QFileInfo(path).fileName()).arg(info.size.width())
                                    .arg(info.size.height()).arg(info.fps, 0, 'f', 3)
                                    .arg(info.timecode.isEmpty() ? "Unavailable" : info.timecode)
                                    + (info.camera.isEmpty() ? QString{} : "  ·  " + info.camera));
            }
            BufferedFrame frame{index, decoded.frame};
            if (playing) buffered.push_back(std::move(frame));
            else present(frame);
            updateControls();
        }
        requestFrame();
    });
    future->setFuture(QtConcurrent::run(&worker, [session, clipPath, index, withAudio] {
        Decoded result;
        try {
            if (!session) {
                const auto directory = brawRuntimeDirectory();
                result.decoder = std::make_shared<RawDecoder>(directory);
                result.info = result.decoder->open(clipPath);
            } else {
                result.decoder = session;
            }
            result.frame = result.decoder->read(index, withAudio);
        } catch (const std::exception& exception) {
            result.error = QString::fromUtf8(exception.what());
        }
        return result;
    }));
}

void Playback::startAudio()
{
    if (buffered.empty() || buffered.front().frame.pcm.isEmpty()) return;
    const auto device = QMediaDevices::defaultAudioOutput();
    QAudioFormat format;
    format.setSampleRate(info.audioRate);
    format.setChannelCount(qMin(2, info.audioChannels));
    format.setSampleFormat(QAudioFormat::Int32);
    if (device.isNull() || !device.isFormatSupported(format)) {
        audioError = "  ·  Audio output unavailable";
        return;
    }
    audio = std::make_unique<QAudioSink>(device, format);
    audio->setBufferSize(format.bytesForDuration(180000));
    audio->setVolume(volume->value() / 100.0);
    audioInput = audio->start();
    if (!audioInput || audio->error() != QAudio::NoError) {
        stopAudio();
        audioError = "  ·  Audio output could not start";
    }
}

void Playback::stopAudio()
{
    audioInput = nullptr;
    if (audio) {
        audio->reset();
        audio.reset();
    }
}

void Playback::tick()
{
    if (vlc) {
        if (vlc->failed()) {
            vlc.reset();
            failed("VLC could not play this media.");
            return;
        }
        auto frame = vlc->takeFrame();
        if (!frame.isNull()) {
            if (pauseOnFirstVideoFrame) {
                pauseOnFirstVideoFrame = false;
                vlc->pause();
            }
            surface->displaySize = frame.size();
            surface->image = std::move(frame);
            surface->update();
        }
        updateControls();
        return;
    }
    if (!playing) return;
    if (!clock.isValid()) {
        if (buffered.size() < 3 && requested < info.frames) return;
        if (buffered.empty()) return;
        audioProcessed = 0;
        startAudio();
        clockOriginUs = 0;
        clock.start();
    }
    if (audioInput) {
        for (auto& item : buffered) {
            const auto& pcm = item.frame.pcm;
            while (item.written < pcm.size() && audio->bytesFree() > 0) {
                const auto bytes = qMin(audio->bytesFree(), pcm.size() - item.written);
                const auto wrote = audioInput->write(pcm.constData() + item.written, bytes);
                if (wrote <= 0) break;
                item.written += wrote;
            }
            if (item.written < pcm.size()) break;
        }
        const auto outputError = audio->error();
#if QT_VERSION < QT_VERSION_CHECK(6, 11, 0)
        const bool outputFailed = outputError != QAudio::NoError && outputError != QAudio::UnderrunError;
#else
        const bool outputFailed = outputError != QAudio::NoError;
#endif
        if (outputFailed) {
            qWarning() << "BRAW audio device error" << outputError << "near frame" << shown + 1
                       << "after processing" << audio->processedUSecs() << "microseconds";
            clockOriginUs = audio->processedUSecs();
            audioProcessed = qMax(audioProcessed, clockOriginUs);
            clock.restart();
            stopAudio();
            audioError = "  ·  Audio output stopped";
        }
    }
    const auto elapsed = audioInput ? audio->processedUSecs() : clockOriginUs + clock.nsecsElapsed() / 1000;
    if (audioInput) audioProcessed = qMax(audioProcessed, elapsed);
    const auto frame = start + qint64(std::floor(elapsed * info.fps / 1000000.0 + 0.00001));
    while (!buffered.empty() && buffered.front().index <= frame) {
        const auto& current = buffered.front();
        if (audioInput && current.written < current.frame.pcm.size()) break;
        present(current);
        buffered.pop_front();
    }
    // Some audio backends round their final playback position down to a device period.
    const auto durationUs = qint64(std::llround((info.frames - start) * 1000000.0 / info.fps));
    if (buffered.empty() && !busy && shown == info.frames - 1 &&
        (elapsed >= durationUs || (audio && audio->state() == QAudio::IdleState))) {
        playing = false;
        start = shown;
        stopAudio();
    }
    requestFrame();
    updateControls();
}

void Playback::present(const BufferedFrame& item)
{
    shown = item.index;
    surface->displaySize = info.size;
    surface->image = item.frame.image;
    surface->update();
}

void Playback::updateControls()
{
    if (vlc) {
        const auto length = vlc->duration();
        const auto time = vlc->position();
        play->setEnabled(true);
        updatePlayIcon(play, vlc->playing());
        timeline->setEnabled(length > 0 && vlc->seekable());
        if (length > 0 && !timeline->isSliderDown())
            timeline->setValue(int(time * 10000.0 / length));
        const auto fps = vlc->frameRate();
        position->setText(formatPlaybackMilliseconds(time, fps) + " / " +
                          (length > 0 ? formatPlaybackMilliseconds(length, fps, true) : "--:--:--:--"));
        return;
    }
    const auto enabled = info.frames > 0;
    timeline->setEnabled(enabled);
    play->setEnabled(enabled);
    updatePlayIcon(play, playing);
    if (enabled && !timeline->isSliderDown())
        timeline->setValue(int(qMax(qint64(0), shown) * 10000 / qMax(qint64(1), info.frames - 1)));
    if (!enabled) {
        position->clear();
    } else {
        position->setText(formatPlaybackFrames(shown, info.fps) + " / " +
                          formatPlaybackFrames(info.frames, info.fps) + audioError);
    }
}

void Playback::failed(const QString& message)
{
    playing = false;
    path.clear();
    lastError = message;
    buffered.clear();
    stopAudio();
    info = {};
    setAction(status, "warning");
    status->setText(message);
    status->setToolTip(message);
    updateControls();
}
