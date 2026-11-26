# Run in the Visual Studio x64 Native Tools command prompt with PowerShell.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BrawSdk,
    [Parameter(Mandatory)][string]$QtRoot,
    [Parameter(Mandatory)][string]$VlcRoot,
    [Parameter(Mandatory)][string]$FfmpegBin,
    [string]$BuildDir,
    [switch]$Launch
)
$ErrorActionPreference = 'Stop'

function Invoke-Checked([string]$Tool, [string[]]$Arguments) {
    & $Tool @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Tool failed with exit code $LASTEXITCODE." }
}

$Source = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BrawSdk = (Resolve-Path $BrawSdk).Path
$QtRoot = (Resolve-Path $QtRoot).Path
$VlcRoot = (Resolve-Path $VlcRoot).Path
$FfmpegBin = (Resolve-Path $FfmpegBin).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) { $BuildDir = Join-Path $env:LOCALAPPDATA 'OpenRoll\build-0.17.2-x64' }
$BuildDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildDir)
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') { throw 'Open the Visual Studio x64 Native Tools command prompt first.' }
$VlcSdk = Join-Path $VlcRoot 'sdk'
$QtBin = Join-Path $QtRoot 'bin'
$SdkRuntime = Join-Path $BrawSdk 'Win\Libraries'
$Fixture = Join-Path $BrawSdk 'Media\sample.braw'
$Idl = Join-Path $BrawSdk 'Win\Include\BlackmagicRawAPI.idl'
foreach ($File in @($Idl, $Fixture, (Join-Path $SdkRuntime 'BlackmagicRawAPI.dll'),
    (Join-Path $QtBin 'qmake.exe'), (Join-Path $VlcSdk 'include\vlc\vlc.h'),
    (Join-Path $VlcSdk 'lib\libvlc.lib'), (Join-Path $VlcRoot 'libvlc.dll'),
    (Join-Path $VlcRoot 'libvlccore.dll'), (Join-Path $VlcRoot 'plugins'),
    (Join-Path $FfmpegBin 'ffmpeg.exe'), (Join-Path $FfmpegBin 'ffprobe.exe'))) {
    if (!(Test-Path -LiteralPath $File)) { throw "Required file or folder missing: $File" }
}

$PreviousPath = $env:Path
$PreviousBraw = $env:OPENROLL_BRAW_RUNTIME
$PreviousVlc = $env:VLC_PLUGIN_PATH
try {
    $MesonBin = Join-Path $HOME 'openroll-build-env\Scripts'
    $env:Path = "$QtBin;$VlcRoot;$FfmpegBin;$PreviousPath"
    if (Test-Path -LiteralPath (Join-Path $MesonBin 'meson.exe')) {
        $env:Path = "$MesonBin;$env:Path"
    }
    $env:OPENROLL_BRAW_RUNTIME = $SdkRuntime
    $env:VLC_PLUGIN_PATH = Join-Path $VlcRoot 'plugins'
    foreach ($Tool in @('meson', 'ninja', 'midl', 'cl')) {
        Get-Command $Tool -CommandType Application | Out-Null
    }
    $Headers = "$BuildDir-braw-headers"
    New-Item -ItemType Directory -Path $Headers -Force | Out-Null
    Invoke-Checked midl @('/nologo', '/env', 'x64', '/out', $Headers, '/h', 'BlackmagicRawAPI.h', $Idl)
    $Setup = @('setup', $BuildDir, $Source, '--buildtype=release',
        "-Dsdk_root=$BrawSdk", "-Dwindows_braw_headers=$Headers", "-Dwindows_vlc_sdk=$VlcSdk",
        "-Dbraw_test_clip=$Fixture")
    if (Test-Path (Join-Path $BuildDir 'build.ninja')) { $Setup += '--reconfigure' }
    Invoke-Checked meson $Setup
    Invoke-Checked meson @('compile', '-C', $BuildDir)
    Invoke-Checked meson @('test', '-C', $BuildDir, '--print-errorlogs')
    Write-Host "Built and tested: $BuildDir\openroll.exe"
    if ($Launch) { Start-Process -FilePath (Join-Path $BuildDir 'openroll.exe') }
} finally {
    $env:Path = $PreviousPath
    $env:OPENROLL_BRAW_RUNTIME = $PreviousBraw
    $env:VLC_PLUGIN_PATH = $PreviousVlc
}
