# OpenRoll 0.17.2 development preview

See [Windows x64 port and build instructions](docs/WINDOWS.md) for this source checkpoint.

This checkpoint covers the dependency audit, the native Qt foundation, Blackmagic's SDK verification, BRAW playback, and conventional playback via libVLC 3, verified media offloading, and a SQLite-backed media workspace. See [milestone status](docs/STATUS.md) and [third-party notices](docs/THIRD-PARTY.md).

## Workspace navigation

Use the top workspace switcher or Ctrl+1 for Ingest, Ctrl+2 for Media, Ctrl+3 for Monitor, and Ctrl+4 for Transcode. In Media, choose a folder on the left and select a thumbnail to load the central viewer. Press I in Media to show or hide metadata for the selected clip. The controls above the clip strip switch to a list or filter by filename, codec, or camera. Monitor expands the same viewer without reopening the clip.

Drop footage files or folders into Ingest or Media to link them to the open
project. OpenRoll indexes the originals in place. A single source dropped into
Ingest also becomes the source for a verified copy. If you drop several sources
into Ingest, OpenRoll switches to Media to link them as a batch. To make a
verified backup, import one source and select **Back up…** in the bottom-right
corner of Ingest. Set destinations and start the verified copy in the dialog.

The playback counters show elapsed time and duration as HH:MM:SS:FF. Audio-only clips show a dash for frames. The metadata inspector continues to show source timecode when available.

In Media or Monitor, press Space to play or pause, comma and period to step, and F for video-only fullscreen. Press Escape or F to return to the workspace. The Export queue shows clip filenames, with full source paths in the tooltips and the current clip’s queue position and percentage beside the progress bar.

## Launch on CachyOS

With the dependencies below installed, run this from the extracted project directory in Fish or Bash. The script extracts the local SDK, builds, runs the bundled tests, and opens the window. Subsequent launches reuse the same extraction and build directory.

```bash
bash scripts/run-preview.sh "$HOME/Downloads/Blackmagic_RAW_Linux_6.0(2).tar"
```

This source includes the rounded BRAW buffer check, quarter/half/full preview handling, the frame-read retry, a full-clip-duration test timeout, and audio diagnostics. The Qt executables compile with PIC on CachyOS.

## Build on CachyOS x86-64

Run in Bash from this source directory with the supplied Blackmagic RAW 6.0 outer tar available on disk. Review the SDK agreement supplied in its RPM before use. The Arch dependency commands are for a CachyOS desktop. The included build and test logs are from Debian 13.

```bash
sudo pacman -Syu --needed base-devel meson ninja pkgconf qt6-base qt6-multimedia \
    qt6-multimedia-ffmpeg libarchive libglvnd util-linux-libs \
    libvlc vlc-plugins-base vlc-plugins-all ffmpeg
```

The Media workspace uses the Qt SQLite driver and the FFmpeg and FFprobe commands. The VLC plugin set supplies demuxers, codecs, and native audio output.

```bash
pacman -Si libvlc vlc-plugins-base vlc-plugin-ffmpeg sqlite ffmpeg
```

Extract to a new directory. Change only the path to the supplied tar if you use the example.

```bash
sdk_parent="$PWD/../vendor-braw6"
sdk="$(./scripts/extract-sdk.sh "$HOME/Downloads/Blackmagic_RAW_Linux_6.0(2).tar" "$sdk_parent")"
profile="$sdk_parent/BlackmagicRAW/BlackmagicRAWSpeedTest/profile.braw"
export OPENROLL_BRAW_RUNTIME="$sdk/Linux/Libraries"

c++ --version
pkg-config --modversion Qt6Widgets Qt6Concurrent Qt6Multimedia
ldd "$OPENROLL_BRAW_RUNTIME/libBlackmagicRawAPI.so"
meson setup build --buildtype=release -Dsdk_root="$sdk" -Dbraw_test_clip="$profile"
meson compile -C build
meson test -C build --print-errorlogs
./build/openroll "$profile"
```

To verify the vendor examples directly, use these commands. Each example starts in its own executable directory so the vendor runtime path resolves.

```bash
for name in ExtractMetadata ExtractFrame ExtractAudio; do
    make -C "$sdk/Samples/$name" config=release verbose=1
done
export TMPDIR="$(mktemp -d)"
(cd "$sdk/Samples/ExtractMetadata/bin/Release" && ./ExtractMetadata "$sdk/Media/sample.braw")
(cd "$sdk/Samples/ExtractFrame/bin/Release" && ./ExtractFrame "$sdk/Media/sample.braw")
(cd "$sdk/Samples/ExtractAudio/bin/Release" && ./ExtractAudio --sync "$sdk/Media/sample.braw")
file "$TMPDIR/outputRGBU8.bmp" "$TMPDIR/syncAudio.wav"
```

## Playback and Milestone 3 desktop acceptance

Open a clip with **Open media…**. Conventional video opens paused after VLC decodes the first available frame. Audio-only playback starts automatically. BRAW clips open paused in the SDK viewer. Use Play/Pause, Stop, the timeline, or the two frame-step buttons. Fullscreen displays the video by itself. The status line displays clip properties and reports unavailable audio output. Qt uses the desktop's default audio device.

Use a real BRAW clip with audio for the audio clock test. A short clip of about 5–10 seconds with a visible, audible slate clap is suitable. The test opens, renders, seeks, steps and plays through the clip five times. The output clock must reach the end of playback within 200 ms. Use footage with audio throughout.

```bash
clip="/absolute/path/to/your-clip.braw"
./build/braw-test "$clip" "$OPENROLL_BRAW_RUNTIME" --require-audio
./build/openroll "$clip"
```

On the desktop, compare the slate clap against picture, play through the clip after a mid-clip seek, and repeat from the beginning after pausing. Check for sync drift, continued playback and responsive controls. The automated clock check complements this manual test.

## Install or remove the locally built preview

The local install copies one executable. Keep the SDK runtime path exported when launching it.

```bash
install -Dm755 build/openroll "$HOME/.local/bin/openroll-preview"
"$HOME/.local/bin/openroll-preview" "$clip"
```

Remove the preview executable with `rm -- "$HOME/.local/bin/openroll-preview"`.

Build on the target desktop for CachyOS validation. The Arch package follows integration and regression testing.

## Verified media offloading

1. Open **Ingest** and choose the camera card or source folder. OpenRoll uses your active project when one is open. Otherwise, the job runs as standalone ingest.
2. Add one or more destinations. **Final copy path** shows each full output path. The copy-folder name defaults to the source name. Change it to identify a card or resolve an existing-folder conflict.
3. Click **Start verified copy**. Wait for **All destinations verified**, then use **Open report folder** to inspect the TXT and CSV reports.

OpenRoll copies regular files, hidden files and empty folders. It refuses source symlinks and special files. Every destination is closed, reopened, read and checked against SHA-256 of the source data before publication. Job progress and transfer rate include writes and verification reads.

Cancellation and failed copies can leave hidden `.openroll-incomplete-<job-id>` staging folders. The reports give their paths. Start a new job to retry after resolving the failure. The publish phase renames verified folders on each destination separately. If one rename fails after another succeeded, the overall job remains incomplete and the report identifies any destinations already published. The engine requires filesystem support for sync and the Linux no-replace rename operation. Removable-drive filesystems and physical unplugging need validation on the target machine.

Reports live under the OpenRoll application data directory, usually `~/.local/share/OpenRoll/transfers`, with one folder per job.


## Media and projects

1. In **Media** or the top **Project** menu, click **New project…**. Enter a name, choose a save folder, and confirm the previewed `.openroll` path. OpenRoll remembers the last project folder.
2. In **Media**, click **Add media folder…** and choose a folder. OpenRoll scans recursively, saves metadata and thumbnails where available, and displays unavailable fields explicitly. Click a clip to load it in the viewer.
3. Run an ingest while a project is open to attach its transfer result and report copies to that project. Use **Open report folder** in Ingest to read the generated reports. Standalone ingest can run with no project open.
4. Use **Open project…** in the Project menu to reopen the same database. It contains references to the original media paths. Keep footage alongside your project backups separately.

Scans preserve clip IDs when the same path is scanned again. The data model separates logical clips from their constituent files. This release imports single-file recordings. Format-specific spanned clips and image sequence grouping remain later work. Stopped or failed jobs are recorded when the worker returns. A force-terminated app can leave an incomplete standalone report without a project history row.


## Transcode

Transcode lists media from the active project. Check the clips you want to export, or use **Add files…** for additional media. Offline project clips cannot be selected for export. Choose an output folder and a preset, then select **Start export**. Supported files are those FFmpeg can decode, plus BRAW through the Blackmagic SDK. The preset applies to the whole queue and the results show individual failures. Video presets require a video stream. WAV extracts the first audio stream from conventional media.

- MOV with ProRes Proxy, 422 LT, 422, 422 HQ, 4444, or 4444 XQ.
- MOV with Avid DNxHR LB, SQ, HQ, or HQX.
- H.264 in MKV with PCM or MP4 with stereo AAC.
- WAV with 24-bit PCM.

Video is scaled to the selected maximum height without upscaling. BRAW uses the SDK's 8-bit RGB decode and clip look. ProRes 4444 presets encode 4:4:4 without preserving alpha. BRAW uses clip frame rate and pads audio shorter than the picture in video exports. Source timecode is passed to BRAW MOV exports when available. Outputs publish only after FFmpeg succeeds and existing files are protected. Cancellation removes temporary output for the active clip.


### Ingest drop area and export progress

Drop one local file or folder into **Ingest** to index it in the background. The drop area shows its file count and total size. Once indexing finishes, add backup destinations and review where each copy will land. The Verified copy section shows overall progress and the final result. Dropping a single file copies that file alone. Dropping a folder preserves its directory structure. With a project open, dropping a folder also links supported clips to Media without copying the files. If no project is open, choose New project, Open project, or Ingest only in the prompt.

**Transcode** shows the active filename, clip number, phase, and estimated time left for that clip once enough progress is available. For conventional media, FFprobe supplies the duration and FFmpeg reports encoded time. BRAW uses frame counts. Sources without a known duration show activity until the export finishes.

At startup, choose **Create project** or **Open project**. **Continue without a project** opens standalone Ingest. Drop a local file or folder to link supported clips to the active project. If needed, Ingest asks for a project after the drop completes. A successful link appears in Media even when the source cannot be used for verified backup.
