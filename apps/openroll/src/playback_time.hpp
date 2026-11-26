#pragma once

#include <QString>
#include <cmath>
#include <limits>

inline QString formatPlaybackTime(qint64 seconds, const QString& frames)
{
    return QString("%1:%2:%3:%4")
        .arg(seconds / 3600, 2, 10, QChar('0'))
        .arg((seconds / 60) % 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'))
        .arg(frames);
}

inline bool usableFrameRate(double fps)
{
    return std::isfinite(fps) && fps >= 1 && fps <= 1000;
}

// The transport counter is elapsed, zero-based, and non-drop-frame.
inline QString formatPlaybackFrames(qint64 frames, double fps)
{
    if (!usableFrameRate(fps)) return "--:--:--:--";
    const auto nominal = qint64(std::llround(fps));
    frames = qMax(qint64(0), frames);
    return formatPlaybackTime(frames / nominal,
                              QString("%1").arg(frames % nominal, 2, 10, QChar('0')));
}

inline QString formatPlaybackMilliseconds(qint64 ms, double fps, bool duration = false)
{
    ms = qMax(qint64(0), ms);
    if (!usableFrameRate(fps)) return formatPlaybackTime(ms / 1000, "--");
    const auto fractionalFrames = static_cast<long double>(ms) * fps / 1000;
    // VLC exposes milliseconds. Allow for its integer timestamp precision at frame boundaries.
    const auto quantized = duration ? std::round(fractionalFrames)
                                    : std::floor(fractionalFrames + fps / 2000);
    if (quantized >= std::numeric_limits<qint64>::max()) return "--:--:--:--";
    return formatPlaybackFrames(qint64(quantized), fps);
}
