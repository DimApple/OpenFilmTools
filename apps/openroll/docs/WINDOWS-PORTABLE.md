# OpenRoll Windows x64 portable beta

Extract the entire ZIP and double-click Launch OpenRoll.cmd. Keep openroll.exe beside its DLLs and plugin folders.
Install Microsoft's current Visual C++ x64 Redistributable before launch.
For BRAW, install Blackmagic RAW 6.0 with its Windows SDK component, or set
OPENROLL_BRAW_RUNTIME to the extracted SDK's Win\Libraries directory.
For metadata, thumbnails and export, put Windows ffmpeg.exe and ffprobe.exe
on PATH before launching. The portable ZIP leaves both external dependencies
to the recipient.

The packaging script stages your compiled OpenRoll executable and uses
windeployqt to deploy Qt. It also copies VLC 3 runtime files and plugins from
your VLC installation. Use the resulting archive for local validation first.
Test on a Windows x64 machine without Qt or the developer terminal. Verify
startup, BRAW and conventional playback, verified backup and export. Review
Qt and VLC license obligations before public redistribution. See THIRD-PARTY.md.
