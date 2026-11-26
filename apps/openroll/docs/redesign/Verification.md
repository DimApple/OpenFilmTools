# OpenRoll 0.17.0 redesign verification

The five patches apply in order to the Step 2 snapshot. Each build log records the
11-test Meson suite for that step. The release source is the result of applying
all five patches. Existing BRAW, VLC, export, transfer, and project database
engines are reused.

| Step | Screen | Changes |
| ---- | ---- | ------- |
| 3 | Welcome | Centered create/open actions, recent projects, and project-file drop |
| 4 | Transcode | Project clip selection, adjacent export settings, and separate queue progress |
| 5 | Ingest | Drop and index, destination paths and final copy paths, verified transfer action |
| 6 | Media and Monitor | Shared viewer, file heading, transport, optional metadata, and clip strip |
| 7 | Polish | Keyboard focus, copy/link project drops, filename elision, stacked destination rows |

The Qt offscreen check rendered Welcome, Ingest, Media, Monitor, and Transcode at
1280×790 and 960×790. It also rendered Media and Monitor with a decoded BRAW
fixture under a long filename. Screenshots show empty project states or that
standalone BRAW fixture. Separately, VLC fixture tests passed with H.264,
HEVC, ProRes, DNxHD, DNxHR, and WAV on Debian. Headless VLC emitted decoder
diagnostics during the run. The bundled verified ingest and BRAW-to-ProRes
tests passed.

CachyOS desktop rendering, native drag-and-drop and real audio output need
desktop verification. The metadata inspector retains the existing tree
widget. The Transcode queue shows the clip and phase together. Ingest reports
one overall progress bar because the transfer engine exposes overall progress.
The source archive excludes the Blackmagic SDK, an executable distribution,
and an Arch install package. See the root README for local build instructions.
