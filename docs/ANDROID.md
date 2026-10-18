# Android foundation

This is a private ARM64 development APK, not a validated Android release.
It shares native physics, Adhoc, career, disc reading and the desktop Vulkan
renderer. There is no PS2 executable in the APK and no deko3d dependency.
The proposed first hardware target is the user's Redmi 11; its exact model,
Android version, GPU, graphics correctness and performance remain unverified.

## Build on Windows

Install Android Studio/SDK, platform 36, build tools, official NDK
27.2.12479018 (r27c), SDK CMake 3.22.1, Java 17 or newer, Gradle 8.14.3,
Git and Python with the project's existing build dependencies. The host script
uses an existing Gradle cache or an explicit `--gradle` path. AGP may install
missing SDK CMake packages under the configured SDK after accepted licenses.
No SDK or signing key is stored in the repository.

```
python tools/build_android.py --sdk C:/path/to/Android/Sdk --gradle C:/path/to/gradle.bat
```

The helper uses the official SDL2 release-2.30.9 source already in
`build/_deps/sdl2-src`, or fetches that tag into `build/android-deps`.
CMake fetches the same pinned dependencies as the PC. Output:
`android/app/build/outputs/apk/debug/app-debug.apk`. Java is a debug APK;
the native game builds Release with the shared arithmetic lowering. APK
signing uses the local Android debug key, not a distributable release key.
Minimum Android API 26, ARM64, Vulkan; no compatibility/performance promise
is made for every device meeting those declarations.

## Bootstrap and storage

The first screen opens the system document picker for the player's own GT4
USA v2.00 ISO. It requests only that document's persistent read grant, without
broad storage permissions or copying the ISO. A retained file descriptor is
passed as `/proc/self/fd/N`, then duplicated by the Android file adapter without
reopening a permission-protected filesystem path. Reads use 64-bit `pread`
so duplicated descriptors cannot interfere through their shared offset. Local
providers must supply a seekable regular
file. Native disc loading performs the existing disc-format checks. A
revoked/unavailable document returns to selection rather than deleting saves.
Process recreation reopens the saved URI. No game contents are packaged.

Saves and the shared `options.ini` are in the app's private files directory.
Uninstalling the app can delete those files. Import/export/backup UI is not
implemented by this first foundation. Avoid treating it as a permanent save
location until hardware validation and export support are complete.

## Input and lifecycle

Touch menus reuse the shared preview/second-tap confirmation, list/map drag,
parameter controls and original Back artwork. Android touch positions are
mapped through the actual renderer viewport before shared hit testing; touch
mouse duplicates stay suppressed. Controls are in Options > Screen Settings >
Controls, with Classic retaining physical-controller behaviour. See Options
for stable IDs, defaults and persistence.

The optional race overlay captures simultaneous fingers independently, offers
analog/digital steering, pedals, handbrake, reverse, shifts, camera and Pause. Small,
Medium and Large sizes, five opacity values and mirrored layout are supported;
free per-button placement is not. Physical SDL controllers take priority.
Input capture clears on cancellation, disabling, pause and controller takeover.
Touch pause cards preview on the first tap and confirm on the second using
the authored actor identity; cancellation, dragging and foreign contacts do
not activate a card. The pad stream uses the existing bindings and recording path, never a physics
fork. Replay, demo and pause screens do not receive active driving contacts.

SDL owns audio/device events and the Vulkan surface. Background/foreground
events cancel touches, pause an active race and reset frame timing; returning
does not automatically resume the race. This does not serialize an active race:
if Android kills the process, unsaved race progress cannot be restored. Android Back routes to the normal
Escape action. Landscape is fixed, with cutout avoidance requested from the
host window. Surface replacement, interruptions and insets need real-device
validation, not just an APK compilation check.

## Validation and remaining work

`mobile_options` checks both presets, shared-store persistence and retained
unknown keys. `desktop_navigation` exercises touch pause cards and a deliberate
one-tap-activation negative. `mobile_controls` and its deliberate negative cover Classic/Enhanced,
simultaneous steering/pedals, outside releases, unknown/duplicate contacts,
controller takeover, cancellation and layout/size bounds on the host. Existing
desktop and Switch touch tests remain separate. These checks do not prove
an entire Android race or a screenshot matches the desktop.

No Android device was connected during initial setup. Hardware gates: install,
choose a local ISO, reach the menus, exercise touch and controller navigation,
start a race, steer/brake/shift, background/resume, verify saves across restart,
and inspect graphics/latency/memory/temperature on the actual phone. Use ADB
logcat and the native log for diagnosis. Neither 60 FPS nor complete lifecycle
handling has been certified.

Movie support currently follows the unavailable non-Windows external-decoder
path. In-process Android FFmpeg and the dependent IPU image decoding are not
yet linked. This can affect videos and IPU-backed UI imagery; do not describe
the build as feature-complete. Additional work: free-position controls, save import/export, gyro/haptics and phone performance
profiles. PC and Switch implementations remain behind their existing guards.

Initial host validation: 15 focused CTests passed (mobile controls/options,
desktop pointer/navigation/events/store, Switch contacts and intentional
negatives), including the touch pause-card negative. Windows Release and
Android ARM64 native Release both build. APK v2 signing and the three ARM64
native libraries were checked; the APK contains no disc, saves or console keys.
Existing upstream/legacy compiler warnings remain; this is not a zero-warning
or hardware-validated release.

### Landscape orientation (17/10/2026)

Both app activities retain sensorLandscape in the manifest and explicitly
request SENSOR_LANDSCAPE after creation and resume. GameActivity also overrides
SDLActivity.setOrientationBis: SDL window creation/resizing cannot replace the
app's landscape policy with portrait or FULL_USER. Both landscape rotations
remain allowed. The external system document picker belongs to its provider
and may choose its own orientation. This sets orientation, not a forced 16:9
stretch on phones with other display ratios. APK compilation/signing are
host checks; physical orientation still needs confirmation on the phone.

### Vulkan content rotation (17/10/2026)

The app can be landscape while the presented game image is rotated. The
final Vulkan presentation is an unrotated blit, so it now requests IDENTITY
when supported instead of copying currentTransform into preTransform. This
leaves surface rotation to Android's compositor, as documented by Khronos:
https://docs.vulkan.org/samples/latest/samples/performance/surface_rotation/README.html
Android advertises all transforms; unsupported identity on other platforms
retains the previous currentTransform selection. Native pre-rotation is a
future optimization; compositor rotation can add bandwidth/processing cost.
Host policy tests cover eight rotations/mirrors and a deliberate old-policy
negative. Vulkan/OpenGL frame comparison and its negative also passed.
APK builds and v2 signature verifies; image orientation on the phone remains
pending physical validation. This supersedes treating Java orientation as
the complete correction for the user's report.
