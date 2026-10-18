package org.moonengine.android;

import android.os.Bundle;
import android.content.pm.ActivityInfo;
import android.view.WindowManager;
import org.libsdl.app.SDLActivity;

/** SDL owns native input, audio and the Vulkan surface lifecycle. */
public final class GameActivity extends SDLActivity {
    @Override protected String[] getLibraries() { return new String[] { "SDL2", "main" }; }
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
