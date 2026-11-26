#include "playback_time.hpp"

int main()
{
    return formatPlaybackFrames(25 * 3723 + 4, 25) == "01:02:03:04" &&
           formatPlaybackFrames(24, 24000.0 / 1001) == "00:00:01:00" &&
           formatPlaybackFrames(23, 24000.0 / 1001) == "00:00:00:23" &&
           formatPlaybackMilliseconds(1151, 25) == "00:00:01:03" &&
           formatPlaybackMilliseconds(83, 24000.0 / 1001) == "00:00:00:02" &&
           formatPlaybackMilliseconds(10099, 25, true) == "00:00:10:02" &&
           formatPlaybackMilliseconds(3723004, 0) == "01:02:03:--" ? 0 : 1;
}
