# OpenRoll Windows x64 port

This source checkpoint incorporates fixes tested in a Windows VM. Linux regression results are included. Windows backup, export and portable deployment still need verification.

## Build environment

- Visual Studio 2022 Build Tools with Desktop development with C++ and a
  Windows SDK that supplies MIDL. Use the x64 Native Tools command prompt.
- Qt 6.8 or newer built for MSVC x64 with Qt Multimedia.
- Python, Meson and Ninja available on PATH.
- VideoLAN VLC 3 x64 SDK distribution with headers, MSVC import library, DLLs,
  and plugins. Use the directory containing both the SDK and playback runtime.
- Windows x64 FFmpeg and FFprobe executables.
- Blackmagic RAW 6.0 for Windows. The supplied installer named
  `Install Blackmagic RAW 6.0 x86.msi` contains the x64 runtime and SDK.
  Install the SDK component, then locate the folder that contains `Win` and
  `Media`. The installation target is usually under
  `C:\Program Files\Blackmagic Design\Blackmagic RAW\Blackmagic RAW SDK`.

Start PowerShell from the x64 Native Tools prompt. From the extracted source
directory, run the following with your actual paths.

```powershell
.\scripts\build-windows.ps1 `
  -BrawSdk "C:\Program Files\Blackmagic Design\Blackmagic RAW\Blackmagic RAW SDK" `
  -QtRoot "C:\Qt\6.8.3\msvc2022_64" `
  -VlcRoot "C:\Dev\vlc-3.0.24" `
  -FfmpegBin "C:\Dev\ffmpeg\bin" -Launch
```

The script generates the Windows API header from the vendor IDL, builds the
application with Meson and Ninja, runs the tests, and optionally launches the
result with the configured runtime paths. Results go in
`build-windows\meson-logs\testlog.txt`. BRAW uses the local vendor SDK.
Windows file names are passed as UTF-16 to the filesystem and BRAW SDK.
Verified backups flush files, independently verify checksums, and publish
with a no-replacement Windows move. Windows directory crash durability needs
testing on the target internal and removable filesystems.

Before producing an installer, verify Windows playback for BRAW and standard
video with audio, ingest to two destinations, rejection of existing outputs,
BRAW-to-ProRes export, cancel and retry, and a project reopen. Also test the
installed executable on a clean Windows x64 system with the intended runtime
dependencies.

## VM validation and portable staging

The Windows VM build was confirmed to launch and play BRAW after updating
the Microsoft Visual C++ x64 Redistributable. The VM also confirmed the
metadata rendering fix. This archive consolidates the Qt module list,
MSVC allocator, external build directory and inspector fixes. The portable
packaging script itself still needs a Windows run.

With the known-good VM build already compiled and OpenRoll closed, run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
```

The ZIP is written to your desktop by default. See WINDOWS-PORTABLE.md for
recipient requirements and validation.
