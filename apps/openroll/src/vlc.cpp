#include "vlc.hpp"

#include <QFile>
#include <QUrl>
#include <vlc/vlc.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#ifdef _MSC_VER
#include <malloc.h>
#endif
#include <mutex>
#include <stdexcept>
#include <utility>

struct VlcPlayback::State {
    libvlc_instance_t* instance = nullptr;
    libvlc_media_t* media = nullptr;
    libvlc_media_player_t* player = nullptr;
    std::mutex pixels;
    QImage buffer;
    QImage latest;
    bool frameRateKnown = false;
    double fps = 0;

    ~State()
    {
        if (player) {
            libvlc_media_player_stop(player);
            libvlc_media_player_release(player);
        }
        if (media) libvlc_media_release(media);
        if (instance) libvlc_release(instance);
    }

    static unsigned format(void** opaque, char* chroma, unsigned* width, unsigned* height,
                           unsigned* pitch, unsigned* lines)
    {
        auto& self = *static_cast<State*>(*opaque);
        if (!*width || !*height || *width > 8192 || *height > 8192 ||
            quint64(*width) * *height > 40000000) return 0;
        // libVLC requires pitches and plane heights aligned to 32.
        const auto alignedWidth = (*width + 7u) & ~7u;
        const auto alignedHeight = (*height + 31u) & ~31u;
        std::lock_guard<std::mutex> lock(self.pixels);
        const auto bytes = size_t(alignedWidth) * alignedHeight * 4;
#ifdef _MSC_VER
        auto* allocation = static_cast<uchar*>(_aligned_malloc(bytes, 32));
        const auto release = _aligned_free;
#else
        auto* allocation = static_cast<uchar*>(std::aligned_alloc(32, bytes));
        const auto release = std::free;
#endif
        if (!allocation) return 0;
        self.buffer = QImage(allocation, int(alignedWidth), int(alignedHeight), alignedWidth * 4,
                             QImage::Format_RGB32, release, allocation);
        if (self.buffer.isNull()) return 0;
        self.visible = QRect(0, 0, int(*width), int(*height));
        std::memcpy(chroma, "RV32", 4);
        *pitch = unsigned(self.buffer.bytesPerLine());
        *lines = alignedHeight;
        return 1;
    }

    static void* lock(void* opaque, void** planes)
    {
        auto& self = *static_cast<State*>(opaque);
        self.pixels.lock();
        *planes = self.buffer.bits();
        return nullptr;
    }

    static void unlock(void* opaque, void*, void* const*)
    {
        static_cast<State*>(opaque)->pixels.unlock();
    }

    static void display(void* opaque, void*)
    {
        auto& self = *static_cast<State*>(opaque);
        std::lock_guard<std::mutex> lock(self.pixels);
        self.latest = self.buffer.copy(self.visible);
    }

    QRect visible;
};

VlcPlayback::VlcPlayback(const QString& file, int initialVolume) : state(std::make_unique<State>())
{
    const char* options[] = {"--no-video-title-show", "--avcodec-hw=none"};
    state->instance = libvlc_new(2, options);
    if (!state->instance) throw std::runtime_error("Could not initialize libVLC.");
#ifdef Q_OS_WIN
    state->media = libvlc_media_new_location(state->instance, QUrl::fromLocalFile(file).toEncoded().constData());
#else
    state->media = libvlc_media_new_path(state->instance, QFile::encodeName(file).constData());
#endif
    if (!state->media) throw std::runtime_error("Could not open the media in libVLC.");
    state->player = libvlc_media_player_new_from_media(state->media);
    if (!state->player) throw std::runtime_error("Could not create the VLC player.");
    libvlc_video_set_callbacks(state->player, State::lock, State::unlock, State::display, state.get());
    libvlc_video_set_format_callbacks(state->player, State::format, nullptr);
    if (qEnvironmentVariableIsSet("OPENROLL_TEST_DUMMY_AUDIO"))
        libvlc_audio_output_set(state->player, "dummy");
    volume(initialVolume);
    if (libvlc_media_player_play(state->player) != 0)
        throw std::runtime_error("Could not start VLC playback.");
}

VlcPlayback::~VlcPlayback() = default;

void VlcPlayback::playPause()
{
    if (playing()) libvlc_media_player_set_pause(state->player, 1);
    else libvlc_media_player_play(state->player);
}

void VlcPlayback::pause() { libvlc_media_player_set_pause(state->player, 1); }

void VlcPlayback::seek(qint64 ms)
{
    if (seekable()) libvlc_media_player_set_time(state->player, std::clamp(ms, qint64(0), duration()));
}

void VlcPlayback::stop() { libvlc_media_player_stop(state->player); }

void VlcPlayback::step(int direction)
{
    if (playing()) libvlc_media_player_set_pause(state->player, 1);
    if (direction > 0) {
        libvlc_media_player_next_frame(state->player);
    } else {
        const auto fps = frameRate();
        if (fps > 0) seek(position() - qint64(std::llround(1000.0 / fps)));
    }
}

void VlcPlayback::volume(int value) { libvlc_audio_set_volume(state->player, value); }
qint64 VlcPlayback::position() const { return qMax(qint64(0), qint64(libvlc_media_player_get_time(state->player))); }
qint64 VlcPlayback::duration() const { return qMax(qint64(0), qint64(libvlc_media_player_get_length(state->player))); }
double VlcPlayback::frameRate() const
{
    if (state->frameRateKnown || duration() == 0) return state->fps;
    libvlc_media_track_t** tracks = nullptr;
    const auto count = libvlc_media_tracks_get(state->media, &tracks);
    if (!count) return 0;
    const auto selected = libvlc_video_get_track(state->player);
    for (unsigned i = 0; i < count; ++i) {
        const auto* track = tracks[i];
        if (track->i_type != libvlc_track_video || !track->video ||
            !track->video->i_frame_rate_num || !track->video->i_frame_rate_den) continue;
        if (state->fps == 0 || track->i_id == selected)
            state->fps = double(track->video->i_frame_rate_num) / track->video->i_frame_rate_den;
        if (track->i_id == selected) break;
    }
    libvlc_media_tracks_release(tracks, count);
    state->frameRateKnown = true;
    return state->fps;
}
bool VlcPlayback::playing() const { return libvlc_media_player_is_playing(state->player) != 0; }
bool VlcPlayback::seekable() const { return libvlc_media_player_is_seekable(state->player) != 0; }
bool VlcPlayback::failed() const { return libvlc_media_player_get_state(state->player) == libvlc_Error; }

unsigned VlcPlayback::decodedAudio() const
{
    libvlc_media_stats_t stats{};
    return libvlc_media_get_stats(state->media, &stats) ? unsigned(std::max(0, stats.i_decoded_audio)) : 0;
}

unsigned VlcPlayback::decodedVideo() const
{
    libvlc_media_stats_t stats{};
    return libvlc_media_get_stats(state->media, &stats) ? unsigned(std::max(0, stats.i_decoded_video)) : 0;
}

QImage VlcPlayback::takeFrame()
{
    std::unique_lock<std::mutex> lock(state->pixels, std::try_to_lock);
    return lock ? std::exchange(state->latest, {}) : QImage{};
}
