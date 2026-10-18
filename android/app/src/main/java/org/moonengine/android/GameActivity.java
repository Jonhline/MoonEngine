package org.moonengine.android;

import android.os.Bundle;
import android.content.pm.ActivityInfo;
import android.view.KeyEvent;
import android.view.WindowManager;
import org.libsdl.app.SDLActivity;

/** SDL owns native input, audio and the Vulkan surface lifecycle. */
public final class GameActivity extends SDLActivity {
    @Override protected String[] getLibraries() { return new String[] { "SDL2", "main" }; }

    /* G6.40 debug overlay: VOLUME_UP + VOLUME_DOWN together toggle it. SDL
       swallows the volume keys on purpose, so the chord is detected here; a
       single press still falls through to Android and keeps working. */
    private static native void nativeToggleOverlay();
    private static final long CHORD_NS = 250_000_000L;
    private long lastVolumeUpNs, lastVolumeDownNs;
    private boolean volumeUpHeld, volumeDownHeld, overlayChord;

    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        int code = event.getKeyCode();
        boolean up = code == KeyEvent.KEYCODE_VOLUME_UP;
        boolean down = code == KeyEvent.KEYCODE_VOLUME_DOWN;
        if (!up && !down)
            return super.dispatchKeyEvent(event);
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            boolean otherHeld = up ? volumeDownHeld : volumeUpHeld;
            if (up) volumeUpHeld = true; else volumeDownHeld = true;
            if (event.getRepeatCount() == 0) {
                long now = System.nanoTime();
                long since = up ? now - lastVolumeDownNs : now - lastVolumeUpNs;
                if (otherHeld || since <= CHORD_NS) {
                    overlayChord = true;
                    nativeToggleOverlay();
                }
                if (up) lastVolumeUpNs = now; else lastVolumeDownNs = now;
            }
            if (overlayChord)
                return true; /* the chord is ours; no volume change */
        } else if (event.getAction() == KeyEvent.ACTION_UP) {
            if (up) volumeUpHeld = false; else volumeDownHeld = false;
            if (!volumeUpHeld && !volumeDownHeld)
                overlayChord = false;
        }
        return false; /* a single volume press is Android's, as before */
    }
    @Override protected String[] getArguments() {
        // No disc yet: start the engine with no arguments. Its own launcher asks for
        // the disc (DiscActivity, driven from native) before the studio room opens.
        if (DiscActivity.disc == null) return new String[0];
        return new String[] { "/proc/self/fd/" + DiscActivity.disc.getFd(),
            "--save-dir", getFilesDir().getAbsolutePath(),
            "--config-file", getFilesDir().getAbsolutePath() + "/options.ini",
            "--renderer", "vulkan" };
    }
    /** Native (android_host.c) starts the document picker in this same process. */
    public void startDiscPicker() { DiscActivity.startPicker(this); }
    /** null while the picker is open, "" when cancelled, else /proc/self/fd/N. */
    public String discPickerResult() { return DiscActivity.pickerResult(); }
    // SDL can request orientation again when its native window is created/resized.
    // Keep both landscape rotations available even with auto-rotate disabled.
    @Override public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }
    @Override protected void onResume() {
        super.onResume();
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }
    @Override public void onCreate(Bundle state) {
        if (DiscActivity.disc == null) {
            String uri=getSharedPreferences("startup",MODE_PRIVATE).getString("discUri",null);
            if(uri!=null) try { DiscActivity.disc=getContentResolver().openFileDescriptor(android.net.Uri.parse(uri),"r"); }
            catch(Exception ignored) { /* Re-select revoked documents in the bootstrap screen. */ }
        }
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        super.onCreate(state);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }
    @Override public void onBackPressed() {
        // Send the same Back event as an external keyboard; do not finish the activity.
        dispatchKeyEvent(new android.view.KeyEvent(android.view.KeyEvent.ACTION_DOWN, android.view.KeyEvent.KEYCODE_ESCAPE));
        dispatchKeyEvent(new android.view.KeyEvent(android.view.KeyEvent.ACTION_UP, android.view.KeyEvent.KEYCODE_ESCAPE));
    }
}
