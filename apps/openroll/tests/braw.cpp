#include "playback.hpp"
#include "raw.hpp"

#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFile>
#include <cmath>
#include <QImage>
#include <QThread>
#include <QtEndian>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool pass, const char* message)
{
    if (!pass) throw std::runtime_error(message);
}

template<class Condition>
void await(Playback& playback, Condition ready, qint64 timeoutMs = 10000,
           const char* operation = "Playback operation")
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (!ready() && playback.error().isEmpty() && elapsed.elapsed() < timeoutMs) {
        QApplication::processEvents();
        QThread::msleep(2);
    }
    if (!playback.error().isEmpty()) throw std::runtime_error(playback.error().toStdString());
    if (!ready())
        throw std::runtime_error(QString("%1 timed out at displayed frame %2 after %3 ms.")
                                     .arg(QString::fromLatin1(operation))
                                     .arg(playback.presentedFrame() + 1).arg(elapsed.elapsed()).toStdString());
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() < 3 || args.size() > 4) {
        std::cerr << "Usage: braw-test /path/to/clip.braw /path/to/sdk/Linux/Libraries [--require-audio]\n";
        return 2;
    }
    const auto clip = QFileInfo(args[1]).absoluteFilePath();
    qputenv("OPENROLL_BRAW_RUNTIME", QFile::encodeName(args[2]));
    try {
        RawDecoder raw(args[2]);
        const auto info = raw.open(clip);
        std::cout << "Source: " << info.size.width() << 'x' << info.size.height() << ", "
                  << info.frames << " frames @ " << info.fps << " fps, "
                  << info.audioChannels << " audio channels @ " << info.audioRate << " Hz\n";
        if (args.contains("--require-audio"))
            require(info.audioChannels > 0 && info.frames / info.fps >= 2.0,
                    "Audio testing requires at least two seconds of BRAW with an audio track.");
        const auto first = raw.read(0, true);
        require(!first.image.isNull(), "Decoder did not produce an image.");
        if (info.audioChannels) {
            const auto expected = qint64(std::llround(info.audioRate / info.fps))
                * qMin(2, info.audioChannels) * 4;
            require(first.pcm.size() == expected, "PCM length did not match the video frame interval.");
            std::cout << "Decoded PCM frame interval: " << first.pcm.size() << " bytes\n"
                      << "SDK audio duration: " << double(info.audioSamples) / info.audioRate << " s\n";
            if (info.frames / info.fps >= 2.0) {
                for (const int percent : {10, 40, 55, 75, 90}) {
                    const auto frame = (info.frames - 1) * percent / 100;
                    const auto pcm = raw.read(frame, true).pcm;
                    qint64 peak = 0;
                    for (qsizetype i = 0; i + 4 <= pcm.size(); i += 4)
                        peak = qMax(peak, std::abs(qint64(qFromLittleEndian<qint32>(pcm.constData() + i))));
                    std::cout << "Source PCM at " << frame / info.fps << " s: peak "
                              << 100.0 * peak / 2147483648.0 << "% of full scale\n";
                }
            }
        }
        const auto last = raw.read(info.frames - 1, true);
        require(last.image.size() == first.image.size(), "Decoder changed dimensions between frames.");
        Playback playback;
        playback.resize(900, 540);
        playback.show();
        for (int repeat = 0; repeat < 5; ++repeat) {
            playback.open(clip);
            await(playback, [&] { return playback.presentedFrame() == 0; });
            QApplication::processEvents();
            const auto image = playback.grab().toImage();
            const auto center = image.pixelColor(image.width() / 2, image.height() / 2);
            const auto decodedCenter = first.image.pixelColor(first.image.width() / 2, first.image.height() / 2);
            if (qMax(decodedCenter.red(), qMax(decodedCenter.green(), decodedCenter.blue())) > 8)
                require(qMax(center.red(), qMax(center.green(), center.blue())) > 0,
                        "Video surface rendered a black center.");
            require(playback.frameCount() == info.frames, "Playback reported a different frame count.");
            if (repeat == 0) {
                const auto buttons = playback.findChildren<QPushButton*>("transportButton");
                require(buttons.size() == 5, "Expected five transport buttons.");
                QPushButton* fullscreen = nullptr;
                for (auto* button : buttons) {
                    require(button->size() == (button->accessibleName() == "Fullscreen" ? QSize(28, 28) :
                                button->accessibleName() == "Play or pause" ? QSize(36, 36) : QSize(32, 32)) && button->iconSize() == QSize(20, 20),
                            "Transport button dimensions do not match its role.");
                    if (button->accessibleName() == "Fullscreen") fullscreen = button;
                }
                require(fullscreen != nullptr, "Fullscreen button is missing.");
                fullscreen->click();
                auto* display = playback.findChild<QDialog*>("fullscreenVideo");
                auto* picture = playback.findChild<QWidget*>("videoSurface");
                require(display && display->isFullScreen() && !playback.isFullScreen() &&
                        picture && picture->window() == display, "Fullscreen includes more than the video.");
                display->reject();
                require(picture->window() == &playback && playback.presentedFrame() == 0,
                        "Exiting fullscreen lost the video or position.");
            }
            if (info.frames < 2) continue;
            playback.seek(info.frames - 1);
            await(playback, [&] { return playback.presentedFrame() == info.frames - 1; });
            playback.step(-1);
            await(playback, [&] { return playback.presentedFrame() == info.frames - 2; });
            playback.step(1);
            await(playback, [&] { return playback.presentedFrame() == info.frames - 1; });
            playback.seek(0);
            await(playback, [&] { return playback.presentedFrame() == 0; });
            QElapsedTimer clock;
            clock.start();
            playback.playPause();
            const auto expectedMs = info.frames * 1000.0 / info.fps;
            await(playback, [&] { return !playback.isPlaying(); },
                  qint64(std::ceil(2 * expectedMs)) + 15000, "Full clip playback");
            require(playback.presentedFrame() == info.frames - 1, "Playback ended before the last frame.");
            require(clock.elapsed() + 12 >= expectedMs, "Playback ended faster than the clip duration.");
            if (args.contains("--require-audio"))
                require(playback.audioProcessedUs() + 200000 >= expectedMs * 1000.0,
                        "The audio output clock stopped before the end of playback.");
            std::cout << "Playback run " << repeat + 1 << ": elapsed " << clock.elapsed()
                      << " ms, media " << expectedMs << " ms, audio output "
                      << playback.audioProcessedUs() / 1000.0 << " ms processed" << '\n';
        }
        playback.open(clip + ".missing");
        QElapsedTimer clock;
        clock.start();
        while (playback.error().isEmpty() && clock.elapsed() < 10000) {
            QApplication::processEvents();
            QThread::msleep(2);
        }
        require(!playback.error().isEmpty(), "Missing clip did not report an error.");
        playback.open(clip);
        await(playback, [&] { return playback.presentedFrame() == 0; });
        require(playback.close(), "Window failed to close.");
        std::cout << "Rendering, repeated opening, missing-file recovery and shutdown passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
