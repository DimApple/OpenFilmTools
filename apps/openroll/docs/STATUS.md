# OpenRoll checkpoint, November 25, 2026

| Milestone | Status | Evidence |
| --- | --- | --- |
| 0. Environment | Passed on Debian 13 x86-64 | GCC 14.2, Meson 1.7, Ninja 1.12, Qt 6.8.2, libVLC 3.0.24, SQLite 3.46.1, FFmpeg 7.1.5 and BRAW SDK 6.0. Qt SQL SQLite and VLC codec plugin located. CachyOS dependency and build instructions supplied. |
| 1. Foundation | Passed offscreen | Meson configured, Ninja linked an x86-64 ELF, Qt created and exposed a window, and the close/event loop test exited successfully. |
| 2. SDK | Passed on Debian | Unmodified vendor examples compiled. SDK factory and codec opened the fixture, read camera metadata, decoded a 4608×2592 frame and extracted stereo 48 kHz, 24-bit PCM. |
| 3. Playback | Partial | CPU quarter-resolution Qt rendering, bounded buffering, transport controls and an audio output path compiled. Offscreen render, seek, frame stepping, timing on the four-frame fixture, five reopen cycles, missing-file recovery and shutdown passed. A second, one-frame stereo fixture passed decode and rendering. Sustained playback and desktop A/V sync remain unverified. |
| 4–9 | Pending | The requested milestone gate requires a Milestone 3 pass before work on conventional playback, ingest, storage, export, integration or packaging. |

## Inputs and dependency findings

The FFmpeg archive identifies revision `359c82515228774eb683dffce9d623b6b4a2fa53` and libavcodec major 63. FFmpeg 7.1.5 supplied by Debian is usable independently. Its encoder list includes libx264, prores_ks and dnxhd. Encoder behavior and profile support still need export tests.

The VLC archive identifies revision `5c415c4ef816951c54ae09777469d3a2efd0aa46`, version 4.0.0-dev and libVLC ABI 12. The installed development package provides libVLC 3.0.24, SONAME 5. Future VLC integration should use the installed 3.x headers. Codec playback remains untested. The two source archives were used as references.

The outer BRAW tar has a distro RPM and a player/Speed Test tarball. The RPM has the SDK headers, example applications and Makefiles, license, x86-64 shared libraries, and `Media/sample.braw`. The other tarball has the Speed Test `profile.braw`. The CPU library needs glibc, libgcc_s, libuuid and libGL, plus its adjacent AVX/AVX2 helper files. The host exposes AVX and AVX2, and the CPU decoder ran successfully. GPU components were inventoried and deferred.

The supplied 125-page manual documents callback-based frame reading and decoding, CPU/GPU pipelines, metadata, audio access and timecode. The preview uses the native C++ headers from the SDK.

## Resolved issues and current block

- Debian's system development packages were absent. With no root access, the build used packages downloaded from Debian's signed repository and extracted under a scratch prefix. Qt and libVLC development headers were resolved with pkg-config.
- Blackmagic's examples use a relative library directory. Starting them from their own `bin/Release` directory resolved the initial runtime loader error. The audio example additionally requires `--sync` or `--async`.
- The SDK's Linux `REFIID` defines `operator==` in C++17. Using that overload corrected a compile failure in the playback callback.
- The execution host rejected Xvfb listener sockets and PulseAudio sockets, and has no ALSA hardware device. Qt's offscreen platform worked. Direct PCM reading and frame interval size were tested. The audio backend's real output clock, underrun handling and sync require a desktop audio test.
- The SDK sample has one frame at 24 fps. The Speed Test fixture has four frames at 60 fps and 16 audio channels. These fixtures exercise short transport operations but do not demonstrate sustained playback. Attempts to retrieve longer vendor samples from the build host returned HTML in place of ZIP files.

## Current behavior

The preview decodes to quarter-resolution RGB for the viewer and sizes it to fit the clip's dimensions. On supported PCM tracks, the preview sends mono or the first two channels to the Qt audio output. Native output requires a supported default device. Full-resolution display, multichannel output routing, wider aspect metadata coverage and behavior on more camera variants still need coverage. There is no production offload workflow in this checkpoint.

## 0.2.1 verification

CachyOS successfully built the earlier preview with `-fPIC` and passed both bundled tests. The user confirmed quarter-resolution playback on a 49-frame clip after accepting 4 KiB-rounded buffers. Frame 497 of a separate 520-frame clip failed in both OpenRoll and Blackmagic's standalone SDK example. Its audio track was reported as 4.6 seconds long.

Version 0.2.1 consolidates those fixes and diagnoses. The integrated source compiled and passed the two bundled tests under Qt 6.8.2 on Debian x86-64. First launch and cached relaunch worked offscreen. The checks under `--require-audio` now require the output clock to process within 200 ms of the full clip duration. A real audio device and a multi-second BRAW with audio throughout are still needed to verify this path and check sync on CachyOS.

## 0.3.0 conventional playback

The user confirmed BRAW is working and requested proceeding to conventional playback. OpenRoll selects the Blackmagic SDK for BRAW and libVLC 3 for other local media. Both share the Qt viewer and transport. Conventional backward stepping seeks by the reported frame interval, so accuracy depends on VLC seeking and the source codec. Testing used square-pixel clips.

The Debian offscreen integration run checks H.264/AAC MP4, H.265 MP4, ProRes Proxy MOV, DNxHD MOV, DNxHR LB MOV and PCM WAV. It checks visible video and decoded frames, decoded audio, play/pause/resume and seeking on MP4, and switching from VLC to BRAW and back. Audio decoding uses VLC's dummy audio output in this test. Physical audio output and playback on CachyOS need a desktop smoke test.

## 0.4.0 ingest

The user requested proceeding to Milestone 5 after reporting MP3 playback on CachyOS. Local regression tests cover playback with the bundled BRAW fixture and generated conventional fixtures. The ingest tests use local temporary folders and exercise two destinations, separately read SHA-256 hashes, reports, source integrity, original paths, missing destinations, insufficient space, collisions, deliberate corruption, cancellation and a forcibly ended worker process. Removable-drive and desktop validation on CachyOS remains pending. No Arch install package is part of this checkpoint.

## 0.5.0 media browser and projects

The Media workspace scans folders on a worker, opens clips in the existing player, and persists source paths, clip IDs, clip-to-file membership, metadata, thumbnails, the last scanned folder, transfer verification state and report copies. The user confirmed 0.4.0 ingest on CachyOS with synthetic files and two destination folders. Version 0.5.0 was tested on Debian using generated MP4 and the supplied BRAW fixture. The project test covers duplicate-free rescanning, SQLite reopen, stable IDs, metadata, thumbnails, settings, and reports after the original report folder is removed. CachyOS validation of the Media workspace remains pending.

## 0.6.0 BRAW to ProRes Proxy

The user requested BRAW-to-ProRes as the first export test. The Export workspace supports single and batch BRAW exports with resolution selection, progress, cancel and per-clip results. Tests use FFprobe to confirm ProRes Proxy profile, frame count, frame rate, track timing, audio channels and source timecode if present. FFmpeg independently decodes the MOV. The test also exercises output collisions and cancellation. The fixture runs are short and offscreen on Debian. Sustained export, visual/audio sync, and export on CachyOS need a real clip check.

## 0.7.0 workspace UI

The user confirmed BRAW-to-ProRes was functional and requested a UI/UX pass. The native Qt shell uses top workspace navigation, workspace shortcuts, an active-project header, and a consistent contrast and action hierarchy. Media adds local filtering and an explicit Open action. Playback adds transport shortcuts and timeline actions. Ingest differentiates verified and incomplete results, and Transcode shows queue position and source filenames.

All four pages were inspected at the default window size using Qt's offscreen renderer on Debian. The navigation smoke test exercises each workspace button and tracks the active page during programmatic navigation. The ingest, project, BRAW and ProRes test suites were rerun. Native rendering on the CachyOS desktop is pending.

## 0.8.0 reference layout

Media combines the source tree, shared playback monitor, metadata inspector, and horizontal thumbnail strip. The sides of the upper region can be resized. The list toggle and search act on the same project clips. Monitor expands the shared player. Reports were removed from workspace navigation and the Media layout. Ingest still produces TXT and CSV files and stores its project records. The inspector shows real clip metadata. The reference's LUT and Look controls remain deferred. This was compiled and smoke-tested offscreen on Debian. Desktop rendering on CachyOS remains unverified.

## 0.9.0 project creation and ingest

Project creation now uses separate name and save-folder fields, previews the full project path, and prevents creating over an existing file. Ingest shows its project context, previews the final copy paths, supports a custom copy-folder name, and validates the plan before starting. The worker validates again and retains the no-overwrite publish and checksum safeguards. Automated tests cover the new-project form, a named two-destination transfer, rejection of path traversal in a folder name, and earlier regressions. The revised dialogs and workflows still need a CachyOS desktop check.

## 0.9.1 transport and fullscreen

The five transport buttons share fixed dimensions and centered icons with a subtle outline. The fullscreen action moves the current video surface into a dedicated borderless fullscreen window and restores it on exit. Media playback remains in the existing session. The second F shortcut and the dialog's Escape handling close fullscreen. Offscreen tests verify button sizing, video-only fullscreen ownership, and frame position after exit. CachyOS compositor behavior remains to be checked on the desktop.

## 0.9.2 playback counters

Elapsed position and duration now use zero-based non-drop-frame HH:MM:SS:FF. BRAW uses decoded frame indices and the clip rate. For conventional media, libVLC reports milliseconds and a nominal video rate, so frame fields are estimates for variable-frame-rate clips. Audio without a video rate uses a dash for the frame field. Seven bundled tests and the six VLC fixture checks passed on Debian.

## 0.10.0 transcode and video opening

Conventional media and BRAW share a multi-preset export queue. FFprobe verified all 13 presets using H.264/AAC input. Independent FFmpeg decoding also passed on short BRAW outputs in ProRes Proxy, DNxHR LB, H.264 MKV/MP4, and WAV. The VLC test checks that conventional video renders an initial frame, pauses, and resumes on explicit Play. Audio-only input retains its previous behavior. The Debian tests use short fixtures. Desktop validation on CachyOS remains pending.

## 0.17.2 Import and verified backup

Ingest shows the footage drop area and file/folder pickers. The bottom-right
Back up button opens a separate verified-copy dialog for the indexed source.
Automated tests verified the dialog stays hidden during ordinary import,
opens on demand and produces a verified backup and project transfer record.
Native CachyOS desktop behavior has not been tested in this release.
