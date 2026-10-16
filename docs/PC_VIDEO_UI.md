# PC display settings

`src/app/video_settings.c` extends the original ScreenRoot with host display
choices. The settings cover aspect ratio, window mode, resolution, UI filter,
text clarity, vertical synchronization and upscaling. Video preferences are
stored in `video.ini`, independently of career data.

The PC launcher's Video page also stores the graphics backend choice in that
file. OpenGL is the default; Vulkan is an experimental native renderer for
menus and 3D scenes. See [PC backends](VULKAN.md) for tests and limitations.

The controller tracks committed and draft settings: Cancel restores the
committed choices; OK applies and persists them. Window resolution and the
selected 4:3 or 16:9 presentation ratio are separate values. UI clipping and
scene projection need to follow the selected transform consistently.

The [development record](notes/PC_VIDEO_UI.md) retains settings tests and
presentation findings. Earlier notes include superseded limitations; see
[Fidelity](FIDELITY.md) for the scope of current verification. PC display
choices do not certify every original menu's widescreen placement.
