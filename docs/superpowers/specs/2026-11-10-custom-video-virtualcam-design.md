# Custom Video Virtual Camera Design

## Goal

Build a Windows x64 portable OBS variant that only plays one local video file in a loop and publishes it through OBS Virtual Camera.

The first milestone favors a conservative OBS customization over a deep rewrite. The product should reuse OBS' existing Qt frontend, `obs-ffmpeg` media source, and `win-dshow` virtual camera output, then remove unrelated plugins and startup friction.

## User-Facing Behavior

- The executable runs from a green/portable folder; no OBS installer is required.
- Launching the app starts in portable mode.
- The app loads a default scene containing one local video media source.
- The media source uses OBS' `ffmpeg_source` with:
  - `is_local_file = true`
  - `looping = true`
  - `restart_on_activate = true`
  - `clear_on_media_end = false`
- The virtual camera starts automatically after the scene collection is loaded.
- Other applications can select `OBS Virtual Camera` and receive the video output.

## Scope

Included:

- Windows x64 build.
- Local video file playback through `obs-ffmpeg`.
- Virtual camera output through `win-dshow`.
- Portable runtime folder with bundled config and a launcher.
- A first-run/default-video fallback so the app has a predictable startup state.

Excluded:

- Streaming, recording, replay buffer, browser source, WebSocket, DeckLink, AJA, VST, WebRTC, capture plugins, text plugins, live services, automatic updater, scripting, and platform-specific plugins unrelated to Windows x64 local file playback.
- A full custom UI in the first milestone. UI hiding can be a second milestone after the portable playback path is verified.

## Architecture

### Build Selection

Add a custom CMake preset and a project-level option:

- `ENABLE_CUSTOM_VIDEO_VCAM`
- `windows-x64-video-vcam`

The CMake option must also be forwarded to the `obs-studio` frontend target as a compile definition so C++ startup code can guard product-specific behavior with `#ifdef ENABLE_CUSTOM_VIDEO_VCAM`.

When enabled, `plugins/CMakeLists.txt` only adds the plugin directories needed for this product:

- `obs-ffmpeg` for local video playback.
- `win-dshow` for virtual camera output and registration scripts.

`ENABLE_SCRIPTING`, `ENABLE_BROWSER`, `ENABLE_WEBSOCKET`, `ENABLE_VST`, `ENABLE_WEBRTC`, `ENABLE_AJA`, `ENABLE_DECKLINK`, `ENABLE_VLC`, `ENABLE_NVENC`, `ENABLE_QSV11`, `ENABLE_SERVICE_UPDATES`, and compatibility/update features are disabled in the preset.

### Startup

Reuse existing OBS startup arguments where possible:

- `--portable`
- `--startvirtualcam`
- `--disable-updater`
- `--disable-missing-files-check`
- `--only-bundled-plugins`

Add a product-specific startup helper only if config templates are not enough. The preferred path is a small launcher script in the portable package:

```bat
bin\64bit\obs64.exe --portable --only-bundled-plugins --disable-updater --disable-missing-files-check --startvirtualcam
```

### Default Scene

Use OBS' existing first-run scene path:

- `OBSBasic::CreateDefaultScene`
- `OBSBasic::CreateFirstRunSources`

Extend first-run source creation under `ENABLE_CUSTOM_VIDEO_VCAM` so it creates a single `ffmpeg_source` item in the default scene when a configured local video exists.

The video path should be read from a portable package config file first, then fall back to `video\default.mp4` under the application root. This avoids hardcoding a developer-local path in source code.

### Portable Package

Create a package script under `build-aux/windows/` that copies the installed runtime into a clean portable folder and then prunes/debug-strips it to the minimal product set.

The package should include:

- `bin/64bit/obs64.exe`
- OBS Qt/runtime DLLs needed by the executable.
- `obs.dll`, `obs-frontend-api.dll`, graphics backends, FFmpeg DLLs, and dependencies used by `obs-ffmpeg`.
- `obs-plugins/64bit/obs-ffmpeg.dll`
- `obs-plugins/64bit/win-dshow.dll`
- `data/libobs`
- `data/obs-studio`
- `data/obs-plugins/obs-ffmpeg`
- `data/obs-plugins/win-dshow`
- `portable_mode.txt`
- `disable_updater.txt`
- `launch-video-virtualcam.bat`
- `video/default.mp4` placeholder location documented, not committed with a large media file.

The script should fail if the virtual camera module or install script is missing.

## Error Handling

- If the configured/default video file does not exist, start the app with an empty default scene and log a clear warning. Do not crash.
- If virtual camera startup fails, keep the app open and rely on OBS' existing failure dialog/logging.
- If the Windows virtual camera is not registered, include `virtualcam-install.bat` in the package and document that it may require administrator privileges once.

## Verification

Minimum verification for the first milestone:

1. Configure `windows-x64-video-vcam`.
2. Build `obs-studio`, `obs-ffmpeg`, `win-dshow`, and `obs-virtualcam-module`.
3. Build/install into `build_x64/install`.
4. Run the portable packaging script.
5. Launch the packaged app with `launch-video-virtualcam.bat`.
6. Confirm logs show the media source loaded and virtual camera started.
7. Confirm another app can select `OBS Virtual Camera` and receive frames.

## Follow-Up Milestone

After the conservative build works, add UI hiding:

- Hide stream/record/replay controls.
- Hide source type menus except local media selection.
- Add a single "Choose Video" entry point.

This is intentionally excluded from milestone 1 to keep the core media-to-virtual-camera path stable.
