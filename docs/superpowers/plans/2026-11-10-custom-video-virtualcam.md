# Custom Video Virtual Camera Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a portable Windows x64 OBS variant that loops one local video file into OBS Virtual Camera.

**Architecture:** Keep OBS' existing frontend and media/output lifecycle. Add a custom build preset and plugin gate, create a first-run local-media scene under a product flag, and package a portable runtime folder with only the needed plugins and resources.

**Tech Stack:** CMake, Visual Studio 2022, C++/Qt frontend, libobs, obs-ffmpeg, win-dshow virtual camera, PowerShell packaging script.

---

## File Structure

- Modify `CMakeLists.txt`: add top-level `ENABLE_CUSTOM_VIDEO_VCAM` option.
- Modify `CMakePresets.json`: add `windows-x64-video-vcam` configure and build presets.
- Modify `frontend/CMakeLists.txt`: forward `ENABLE_CUSTOM_VIDEO_VCAM` to C++ as a compile definition.
- Modify `plugins/CMakeLists.txt`: when the custom option is enabled, only add minimal plugins.
- Modify `frontend/widgets/OBSBasic.hpp`: declare a helper for the custom first-run source path.
- Modify `frontend/widgets/OBSBasic_SceneItems.cpp`: create the default local looping `ffmpeg_source`.
- Modify `frontend/widgets/OBSBasic_SceneCollections.cpp`: call the custom first-run helper after default scene creation.
- Create `build-aux/windows/Package-VideoVirtualCam.ps1`: produce the green portable folder.
- Create `build-aux/windows/launch-video-virtualcam.bat.in`: launcher template copied by the package script.
- Create `docs/custom-video-virtualcam.md`: user build/run notes.

---

## Task 1: Add the Custom Build Switch

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `CMakePresets.json`
- Modify: `frontend/CMakeLists.txt`

- [ ] **Step 1: Add the top-level option**

In `CMakeLists.txt`, near the existing top-level options:

```cmake
option(ENABLE_CUSTOM_VIDEO_VCAM "Build the lightweight local-video virtual camera variant" OFF)
```

- [ ] **Step 2: Add the configure preset**

In `CMakePresets.json`, add a configure preset after `windows-x64`:

```json
{
  "name": "windows-x64-video-vcam",
  "displayName": "Windows x64 Local Video Virtual Camera",
  "description": "Lightweight Windows x64 build for looping a local video into OBS Virtual Camera",
  "inherits": ["windows-x64"],
  "cacheVariables": {
    "ENABLE_CUSTOM_VIDEO_VCAM": true,
    "ENABLE_SCRIPTING": false,
    "ENABLE_BROWSER": false,
    "ENABLE_AJA": false,
    "ENABLE_DECKLINK": false,
    "ENABLE_WEBSOCKET": false,
    "ENABLE_WEBRTC": false,
    "ENABLE_VST": false,
    "ENABLE_VLC": false,
    "ENABLE_NVENC": false,
    "ENABLE_QSV11": false,
    "ENABLE_SERVICE_UPDATES": false,
    "ENABLE_COMPAT_UPDATES": false,
    "ENABLE_NEW_MPEGTS_OUTPUT": false,
    "ENABLE_HEVC": false
  }
}
```

- [ ] **Step 3: Add the build preset**

In `CMakePresets.json`, add:

```json
{
  "name": "windows-x64-video-vcam",
  "configurePreset": "windows-x64-video-vcam",
  "displayName": "Windows 64-bit Local Video Virtual Camera",
  "description": "Windows build for the lightweight local-video virtual camera variant",
  "configuration": "RelWithDebInfo"
}
```

- [ ] **Step 4: Forward the option to C++**

In `frontend/CMakeLists.txt`, after the existing `target_compile_definitions(obs-studio ...)` block that sets `SAFE_MODULES`, add:

```cmake
if(ENABLE_CUSTOM_VIDEO_VCAM)
  target_compile_definitions(obs-studio PRIVATE ENABLE_CUSTOM_VIDEO_VCAM)
endif()
```

- [ ] **Step 5: Verify configure**

Run:

```powershell
cmake --preset windows-x64-video-vcam
```

Expected: CMake configures `build_x64` without requiring CEF/browser dependencies.

---

## Task 2: Gate Plugins to the Minimal Set

**Files:**
- Modify: `plugins/CMakeLists.txt`

- [ ] **Step 1: Add early minimal plugin block**

After the `ENABLE_PLUGINS` check in `plugins/CMakeLists.txt`, add:

```cmake
if(ENABLE_CUSTOM_VIDEO_VCAM)
  add_obs_plugin(obs-ffmpeg)
  add_obs_plugin(win-dshow PLATFORMS WINDOWS)
  return()
endif()
```

- [ ] **Step 2: Verify generated targets**

Run:

```powershell
cmake --build --preset windows-x64-video-vcam --target obs-studio obs-ffmpeg win-dshow obs-virtualcam-module
```

Expected: the build includes `obs-ffmpeg`, `win-dshow`, and `obs-virtualcam-module`; it does not build browser, WebSocket, capture, streaming service, VST, DeckLink, AJA, or WebRTC plugins.

---

## Task 3: Create the Default Local Video Source

**Files:**
- Modify: `frontend/widgets/OBSBasic.hpp`
- Modify: `frontend/widgets/OBSBasic_SceneItems.cpp`
- Modify: `frontend/widgets/OBSBasic_SceneCollections.cpp`

- [ ] **Step 1: Declare helper**

In `frontend/widgets/OBSBasic.hpp`, near `CreateFirstRunSources()`:

```cpp
void CreateCustomVideoVirtualCamSource(obs_scene_t *scene);
std::string GetCustomVideoVirtualCamPath() const;
```

- [ ] **Step 2: Implement path lookup**

In `frontend/widgets/OBSBasic_SceneItems.cpp`, add:

```cpp
#include <util/platform.h>
```

Then implement:

```cpp
std::string OBSBasic::GetCustomVideoVirtualCamPath() const
{
	char *base = os_get_executable_path_ptr(nullptr);
	if (!base)
		return {};

	std::string path = base;
	bfree(base);

	const size_t slash = path.find_last_of("\\/");
	if (slash != std::string::npos)
		path.resize(slash);

	path += "/../../video/default.mp4";
	os_normalize_path(path.data());
	return path;
}
```

- [ ] **Step 3: Implement media source creation**

In `frontend/widgets/OBSBasic_SceneItems.cpp`, add:

```cpp
void OBSBasic::CreateCustomVideoVirtualCamSource(obs_scene_t *scene)
{
#ifdef ENABLE_CUSTOM_VIDEO_VCAM
	if (!scene)
		return;

	std::string videoPath = GetCustomVideoVirtualCamPath();
	if (videoPath.empty() || !os_file_exists(videoPath.c_str())) {
		blog(LOG_WARNING, "Custom video virtual camera: default video not found: %s", videoPath.c_str());
		return;
	}

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", true);
	obs_data_set_string(settings, "local_file", videoPath.c_str());
	obs_data_set_bool(settings, "looping", true);
	obs_data_set_bool(settings, "restart_on_activate", true);
	obs_data_set_bool(settings, "clear_on_media_end", false);

	OBSSourceAutoRelease source = obs_source_create("ffmpeg_source", "Local Loop Video", settings, nullptr);
	if (!source) {
		blog(LOG_ERROR, "Custom video virtual camera: failed to create ffmpeg_source");
		return;
	}

	obs_sceneitem_t *item = obs_scene_add(scene, source);
	if (item) {
		obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
		vec2 bounds = {1920.0f, 1080.0f};
		obs_sceneitem_set_bounds(item, &bounds);
	}
#endif
}
```

- [ ] **Step 4: Call helper during default scene creation**

In `frontend/widgets/OBSBasic_SceneCollections.cpp`, update `CreateDefaultScene`:

```cpp
OBSSceneAutoRelease scene = obs_scene_create(Str("Basic.Scene"));

if (firstStart) {
	CreateFirstRunSources();
	CreateCustomVideoVirtualCamSource(scene);
}
```

- [ ] **Step 5: Verify first-run source appears**

Run the built executable from a clean portable folder containing `video/default.mp4`.

Expected log lines:

```text
[Loaded global audio device]
source: 'Local Loop Video' (ffmpeg_source)
Starting virtual camera
```

---

## Task 4: Package the Portable Runtime

**Files:**
- Create: `build-aux/windows/launch-video-virtualcam.bat.in`
- Create: `build-aux/windows/Package-VideoVirtualCam.ps1`

- [ ] **Step 1: Add launcher template**

Create `build-aux/windows/launch-video-virtualcam.bat.in`:

```bat
@echo off
setlocal
set ROOT=%~dp0
"%ROOT%bin\64bit\obs64.exe" --portable --only-bundled-plugins --disable-updater --disable-missing-files-check --startvirtualcam
endlocal
```

- [ ] **Step 2: Add package script**

Create `build-aux/windows/Package-VideoVirtualCam.ps1`:

```powershell
param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [string] $Target = 'x64'
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Resolve-Path "$PSScriptRoot\..\.."
$InstallRoot = Join-Path $ProjectRoot "build_$Target\install"
$OutputRoot = Join-Path $ProjectRoot "build_$Target\video-virtualcam-portable"

if (Test-Path $OutputRoot) {
    Remove-Item -Recurse -Force $OutputRoot
}

New-Item -ItemType Directory -Force $OutputRoot | Out-Null
Copy-Item -Recurse -Force "$InstallRoot\*" $OutputRoot

New-Item -ItemType File -Force (Join-Path $OutputRoot 'portable_mode.txt') | Out-Null
New-Item -ItemType File -Force (Join-Path $OutputRoot 'disable_updater.txt') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $OutputRoot 'video') | Out-Null

Copy-Item -Force "$ProjectRoot\build-aux\windows\launch-video-virtualcam.bat.in" `
    (Join-Path $OutputRoot 'launch-video-virtualcam.bat')

$Required = @(
    'bin\64bit\obs64.exe',
    'obs-plugins\64bit\obs-ffmpeg.dll',
    'obs-plugins\64bit\win-dshow.dll',
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
```

- [ ] **Step 3: Build and install**

Run:

```powershell
cmake --build --preset windows-x64-video-vcam
cmake --install build_x64 --config RelWithDebInfo --prefix build_x64/install
```

Expected: `build_x64/install` contains OBS runtime files.

- [ ] **Step 4: Create portable package**

Run:

```powershell
.\build-aux\windows\Package-VideoVirtualCam.ps1 -Configuration RelWithDebInfo -Target x64
```

Expected: `build_x64/video-virtualcam-portable/launch-video-virtualcam.bat` exists.

---

## Task 5: Document Runbook and Validation

**Files:**
- Create: `docs/custom-video-virtualcam.md`

- [ ] **Step 1: Add user runbook**

Create `docs/custom-video-virtualcam.md`:

````markdown
# Custom Video Virtual Camera

## Build

```powershell
cmake --preset windows-x64-video-vcam
cmake --build --preset windows-x64-video-vcam
cmake --install build_x64 --config RelWithDebInfo --prefix build_x64/install
.\build-aux\windows\Package-VideoVirtualCam.ps1 -Configuration RelWithDebInfo -Target x64
```

## Run

1. Copy a local video to `build_x64/video-virtualcam-portable/video/default.mp4`.
2. If the OBS virtual camera has not been registered on this machine, run `data/obs-plugins/win-dshow/virtualcam-install.bat` once as administrator.
3. Double-click `launch-video-virtualcam.bat`.
4. Select `OBS Virtual Camera` in another application.

## Expected Behavior

- OBS starts in portable mode.
- The default scene contains `Local Loop Video`.
- The media source loops `video/default.mp4`.
- The virtual camera starts automatically.
```
````

- [ ] **Step 2: Validate runtime logs**

Run:

```powershell
.\build_x64\video-virtualcam-portable\launch-video-virtualcam.bat
```

Expected: OBS launches and its log contains `ffmpeg_source` and virtual camera start messages.

- [ ] **Step 3: Validate external camera visibility**

Open Windows Camera, Teams, Zoom, browser camera test page, or another DirectShow consumer.

Expected: `OBS Virtual Camera` appears and displays the looping video frames.

---

## Task 6: First Milestone Review

**Files:**
- Inspect: `build_x64/video-virtualcam-portable`
- Inspect: latest OBS log under the portable config directory

- [ ] **Step 1: Measure plugin footprint**

Run:

```powershell
Get-ChildItem .\build_x64\video-virtualcam-portable\obs-plugins\64bit
```

Expected: only the required plugin DLLs remain for milestone 1, primarily `obs-ffmpeg.dll` and `win-dshow.dll`.

- [ ] **Step 2: Confirm no installer dependency for OBS itself**

Move `build_x64/video-virtualcam-portable` to another local directory and double-click `launch-video-virtualcam.bat`.

Expected: OBS starts from that folder. Virtual camera registration may still require the provided admin registration script once per machine because Windows DirectShow filters must be registered system-wide.

- [ ] **Step 3: Decide whether to start UI slimming milestone**

Only after video playback and virtual camera output are verified, start a separate plan to hide stream/record controls and replace OBS' source workflow with a single video chooser.
