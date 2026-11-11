# Custom Video Virtual Camera

This build variant starts OBS in portable mode, loads a local looping video source, and starts OBS Virtual Camera automatically.

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

## Notes

The OBS app itself is green/portable. The Windows DirectShow virtual camera filter still requires one-time system registration on each machine because camera devices are discovered through system-wide COM registration.
