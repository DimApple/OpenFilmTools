#pragma once

#include "raw.hpp"
#include <QElapsedTimer>
#include <QThreadPool>
#include <QTimer>
#include <QWidget>
#include <deque>

class QAudioSink;
class QIODevice;
class QLabel;
class QSlider;
class QPushButton;
class VideoSurface;
class VlcPlayback;

class Playback final : public QWidget {
public:
    explicit Playback(QWidget* parent = nullptr);
    ~Playback() override;
    void open(const QString& path);
    void playPause();
    void seek(qint64 frame);
    void seekConventional(qint64 ms);
    void step(int frames);
    qint64 presentedFrame() const { return shown; }
    qint64 frameCount() const { return info.frames; }
    bool isPlaying() const;
    qint64 conventionalPosition() const;
    qint64 conventionalDuration() const;
    bool conventionalFrameVisible() const;
    unsigned conventionalAudioDecoded() const;
    unsigned conventionalVideoDecoded() const;
    qint64 audioProcessedUs() const { return audioProcessed; }
    QString activePath() const { return path; }
    QString error() const { return lastError; }

private:
    struct BufferedFrame {
        qint64 index;
        RawFrame frame;
        qsizetype written = 0;
    };
    void tick();
    void requestFrame();
    void startAudio();
    void stopAudio();
    void present(const BufferedFrame& frame);
    void updateControls();
    void failed(const QString& message);

    std::unique_ptr<VlcPlayback> vlc;
    QThreadPool worker;
    QTimer timer;
    QElapsedTimer clock;
    std::shared_ptr<RawDecoder> decoder;
    std::deque<BufferedFrame> buffered;
    RawInfo info;
    std::unique_ptr<QAudioSink> audio;
    QIODevice* audioInput = nullptr;
    QLabel* status;
    QLabel* position;
    QSlider* timeline;
    QSlider* volume;
    QPushButton* play;
    VideoSurface* surface;
    qint64 requested = 0;
    qint64 start = 0;
    qint64 shown = -1;
    qint64 serial = 0;
    qint64 clockOriginUs = 0;
    bool pauseOnFirstVideoFrame = false;
    bool busy = false;
    bool playing = false;
    qint64 audioProcessed = 0;
    QString path;
    QString lastError;
    QString audioError;
};
