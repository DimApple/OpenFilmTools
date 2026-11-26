#pragma once

#include <QImage>
#include <QString>
#include <memory>

class VlcPlayback {
public:
    VlcPlayback(const QString& file, int volume);
    ~VlcPlayback();

    void playPause();
    void pause();
    void seek(qint64 ms);
    void stop();
    void step(int direction);
    void volume(int value);
    qint64 position() const;
    qint64 duration() const;
    double frameRate() const;
    bool playing() const;
    bool seekable() const;
    bool failed() const;
    unsigned decodedAudio() const;
    unsigned decodedVideo() const;
    QImage takeFrame();

private:
    struct State;
    std::unique_ptr<State> state;
};
