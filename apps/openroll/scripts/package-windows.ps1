[CmdletBinding()]
param(
    [string]$BuildDir = "$env:LOCALAPPDATA\OpenRoll\build-0.17.2-x64",
    [string]$QtRoot = 'C:\Qt\6.12.1\msvc2022_64',
    [string]$VlcRoot = 'C:\Dev\vlc-3.0.24',
    [string]$Output = "$HOME\Desktop\OpenRoll-0.17.2-Windows-x64.zip"
)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $BuildDir 'openroll.exe'
$deploy = Join-Path $QtRoot 'bin\windeployqt.exe'
foreach ($required in @($exe, $deploy, "$VlcRoot\libvlc.dll", "$VlcRoot\libvlccore.dll", "$VlcRoot\plugins")) {
    if (!(Test-Path -LiteralPath $required)) { throw "Missing: $required" }
}
if (Test-Path -LiteralPath $Output) { throw "Output already exists: $Output" }
$work = Join-Path ([IO.Path]::GetTempPath()) ('openroll-package-' + [guid]::NewGuid())
$stage = Join-Path $work 'OpenRoll'
$previousPath = $env:Path
try {
    New-Item -ItemType Directory -Path $stage | Out-Null
    Copy-Item -LiteralPath $exe -Destination $stage
    $env:Path = "$QtRoot\bin;$previousPath"
    & $deploy --release --no-compiler-runtime --no-translations --dir $stage (Join-Path $stage 'openroll.exe')
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE." }
    Copy-Item -LiteralPath "$VlcRoot\libvlc.dll","$VlcRoot\libvlccore.dll" -Destination $stage
    Copy-Item -LiteralPath "$VlcRoot\plugins" -Destination (Join-Path $stage 'vlc-plugins') -Recurse
    foreach ($required in @('Qt6Core.dll', 'Qt6Multimedia.dll', 'platforms\qwindows.dll', 'sqldrivers\qsqlite.dll')) {
        if (!(Test-Path -LiteralPath (Join-Path $stage $required))) { throw "Qt deployment omitted $required" }
    }
    foreach ($notice in @('COPYING', 'COPYING.txt', 'THANKS.txt')) {
        $path = Join-Path $VlcRoot $notice
        if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination (Join-Path $stage "VLC-$notice") }
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\docs\THIRD-PARTY.md') -Destination $stage
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\docs\WINDOWS-PORTABLE.md') -Destination $stage
    @'
@echo off
setlocal
set "VLC_PLUGIN_PATH=%~dp0vlc-plugins"
start "" "%~dp0openroll.exe"
'@ | Set-Content -LiteralPath (Join-Path $stage 'Launch OpenRoll.cmd') -Encoding Ascii
    Compress-Archive -LiteralPath $stage -DestinationPath $Output
    Get-FileHash -LiteralPath $Output -Algorithm SHA256
} finally {
    $env:Path = $previousPath
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
}
