#include "playback.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QRegularExpression>
#include <QThread>
#include <iostream>
#include <stdexcept>

template<class Predicate>
void waitFor(Playback& playback, Predicate ready, const char* operation)
{
    QElapsedTimer time;
    time.start();
    while (!ready() && playback.error().isEmpty() && time.elapsed() < 6000) {
        QApplication::processEvents();
        QThread::msleep(5);
    }
    if (!playback.error().isEmpty())
        throw std::runtime_error(playback.error().toStdString());
    if (!ready()) throw std::runtime_error(operation);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() < 5) return 2;
    qputenv("OPENROLL_BRAW_RUNTIME", args[1].toUtf8());
    qputenv("OPENROLL_TEST_DUMMY_AUDIO", "1");
    try {
        Playback player;
        player.resize(800, 450);
        player.show();
        for (int i = 3; i < args.size(); ++i) {
            player.open(args[i]);
            const bool audioOnly = args[i].endsWith(".wav");
            if (!audioOnly) {
                waitFor(player, [&] { return player.conventionalFrameVisible() && !player.isPlaying(); },
                        "Video did not open paused.");
                player.playPause();
            }
            waitFor(player, [&] {
                return player.conventionalDuration() > 0 && player.isPlaying() &&
                    (audioOnly ? player.conventionalAudioDecoded() > 0 : player.conventionalFrameVisible() && player.conventionalVideoDecoded() >= 5);
            }, "VLC did not decode the expected stream.");
            const auto* counter = player.findChild<QLabel*>("quiet");
            const QRegularExpression display(audioOnly
                ? R"(\d{2,}:\d{2}:\d{2}:-- / \d{2,}:\d{2}:\d{2}:--)"
                : R"(\d{2,}:\d{2}:\d{2}:\d{2,} / \d{2,}:\d{2}:\d{2}:\d{2,})");
            waitFor(player, [&] { return counter && display.match(counter->text()).hasMatch(); },
                    "VLC time counter did not show the expected format.");
            if (i == 3) {
                waitFor(player, [&] { return player.conventionalAudioDecoded() > 0; }, "MP4 audio did not decode.");
                const auto target = player.conventionalDuration() / 2;
                player.seekConventional(target);
                waitFor(player, [&] { return player.conventionalPosition() >= target - 150; }, "VLC seek failed.");
                player.playPause();
                waitFor(player, [&] { return !player.isPlaying(); }, "VLC pause failed.");
                player.playPause();
                waitFor(player, [&] { return player.isPlaying(); }, "VLC resume failed.");
                player.open(args[2]);
                waitFor(player, [&] { return player.presentedFrame() == 0; }, "Switch from VLC to BRAW failed.");
            }
            std::cout << "Passed " << args[i].toStdString() << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
