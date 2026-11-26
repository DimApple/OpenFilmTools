#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>
#include <memory>

struct RawInfo {
    QSize size;
    double fps = 0;
    qint64 frames = 0;
    QString camera;
    QString timecode;
    int audioRate = 0;
    int audioChannels = 0;
    quint64 audioSamples = 0;
};

struct RawFrame {
    QImage image;
    QByteArray pcm;
};

class RawDecoder {
public:
    explicit RawDecoder(const QString& runtimeDirectory);
    ~RawDecoder();
    RawInfo open(const QString& path);
    RawFrame read(qint64 index, bool audio, bool fullResolution = false);

private:
    struct State;
    std::unique_ptr<State> state;
};

QString brawRuntimeDirectory();
