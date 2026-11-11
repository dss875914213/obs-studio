param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [string] $Target = 'x64'
)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Resolve-Path "$PSScriptRoot\..\.."
$InstallRoot = Join-Path $ProjectRoot "build_$Target\install"
$OutputRoot = Join-Path $ProjectRoot "build_$Target\video-virtualcam-portable"

if (!(Test-Path $InstallRoot)) {
    throw "Install root not found: $InstallRoot. Run cmake --install first."
}

if (Test-Path $OutputRoot) {
    Remove-Item -Recurse -Force $OutputRoot
}

New-Item -ItemType Directory -Force $OutputRoot | Out-Null
Copy-Item -Recurse -Force "$InstallRoot\*" $OutputRoot

$RunDir = Join-Path $ProjectRoot "build_$Target\rundir\$Configuration"
$FallbackPlugin = Join-Path $RunDir 'obs-plugins\64bit\rtmp-services.dll'
$FallbackPluginData = Join-Path $RunDir 'data\obs-plugins\rtmp-services'
if (!(Test-Path (Join-Path $OutputRoot 'obs-plugins\64bit\rtmp-services.dll')) -and (Test-Path $FallbackPlugin)) {
    New-Item -ItemType Directory -Force (Join-Path $OutputRoot 'obs-plugins\64bit') | Out-Null
    Copy-Item -Force $FallbackPlugin (Join-Path $OutputRoot 'obs-plugins\64bit\rtmp-services.dll')
}
if (!(Test-Path (Join-Path $OutputRoot 'data\obs-plugins\rtmp-services')) -and (Test-Path $FallbackPluginData)) {
    New-Item -ItemType Directory -Force (Join-Path $OutputRoot 'data\obs-plugins') | Out-Null
    Copy-Item -Recurse -Force $FallbackPluginData (Join-Path $OutputRoot 'data\obs-plugins\rtmp-services')
}

$PluginDir = Join-Path $OutputRoot 'obs-plugins\64bit'
if (Test-Path $PluginDir) {
    Get-ChildItem $PluginDir -File | Where-Object {
        $_.Name -notin @('obs-ffmpeg.dll', 'obs-outputs.dll', 'obs-transitions.dll', 'obs-x264.dll', 'rtmp-services.dll', 'win-dshow.dll')
    } | Remove-Item -Force
}

$PluginDataDir = Join-Path $OutputRoot 'data\obs-plugins'
if (Test-Path $PluginDataDir) {
    Get-ChildItem $PluginDataDir -Directory | Where-Object {
        $_.Name -notin @('obs-ffmpeg', 'obs-outputs', 'obs-transitions', 'obs-x264', 'rtmp-services', 'win-dshow')
    } | Remove-Item -Recurse -Force
}

Get-ChildItem $OutputRoot -Recurse -Force -File -Include *.pdb,*.ilk,*.exp,*.lib -ErrorAction SilentlyContinue |
    Remove-Item -Force
foreach ($DevTool in @('bin\64bit\obs-amf-test.exe', 'bin\64bit\obs-ffmpeg-mux.exe')) {
    $Path = Join-Path $OutputRoot $DevTool
    if (Test-Path $Path) {
        Remove-Item -Force $Path
    }
}

New-Item -ItemType File -Force (Join-Path $OutputRoot 'portable_mode.txt') | Out-Null
New-Item -ItemType File -Force (Join-Path $OutputRoot 'disable_updater.txt') | Out-Null
New-Item -ItemType File -Force (Join-Path $OutputRoot 'disable_missing_files_check.txt') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $OutputRoot 'video') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $OutputRoot 'config\obs-studio\.sentinel') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $OutputRoot 'config\obs-studio\basic\scenes') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $OutputRoot 'config\obs-studio\basic\profiles') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $OutputRoot 'config\obs-studio\logs') | Out-Null

Copy-Item -Force "$ProjectRoot\build-aux\windows\launch-video-virtualcam.bat.in" `
    (Join-Path $OutputRoot 'launch-video-virtualcam.bat')

$DependencyBin = Join-Path $ProjectRoot '.deps\obs-deps-2025-08-23-x64\bin'
$RuntimeBin = Join-Path $OutputRoot 'bin\64bit'
$ExtraRuntimeDlls = @(
    'datachannel.dll',
    'librist.dll',
    'libx264-164.dll',
    'srt.dll'
)

foreach ($Dll in $ExtraRuntimeDlls) {
    $Source = Join-Path $DependencyBin $Dll
    if (Test-Path $Source) {
        Copy-Item -Force $Source $RuntimeBin
    }
}

$Required = @(
    'bin\64bit\obs64.exe',
    'bin\64bit\librist.dll',
    'bin\64bit\libx264-164.dll',
    'obs-plugins\64bit\obs-ffmpeg.dll',
    'obs-plugins\64bit\obs-outputs.dll',
    'obs-plugins\64bit\obs-transitions.dll',
    'obs-plugins\64bit\obs-x264.dll',
    'obs-plugins\64bit\rtmp-services.dll',
    'obs-plugins\64bit\win-dshow.dll',
    'data\obs-plugins\obs-transitions\fade_transition.effect',
    'data\obs-plugins\win-dshow\obs-virtualcam-module64.dll',
    'data\obs-plugins\win-dshow\virtualcam-install.bat',
    'data\obs-plugins\win-dshow\virtualcam-uninstall.bat'
)

foreach ($Path in $Required) {
    $FullPath = Join-Path $OutputRoot $Path
    if (!(Test-Path $FullPath)) {
        throw "Missing required package file: $Path"
    }
}

Write-Host "Portable package ready: $OutputRoot"
Write-Host "Put a test video at: $(Join-Path $OutputRoot 'video\default.mp4')"
