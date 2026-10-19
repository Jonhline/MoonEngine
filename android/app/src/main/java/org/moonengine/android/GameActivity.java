package org.moonengine.android;

import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.pm.ActivityInfo;
import android.os.Environment;
import android.provider.MediaStore;
import android.view.KeyEvent;
import android.view.WindowManager;
import java.io.File;
import java.io.FileInputStream;
import java.io.OutputStream;
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

    /* ONE user root, reachable over MTP/SAF, in the same MoonEngine/MoonEngine
       layout desktop uses -- native (android_host.c) computes the same path. */
    private String userRoot() {
        File external = getExternalFilesDir(null);
        File base = external != null ? external : getFilesDir();
        return new File(base, "MoonEngine/MoonEngine").getAbsolutePath();
    }

    @Override protected String[] getArguments() {
        // No disc on the command line: a bare descriptor would select the game
        // path (launcher.c argv[1]) and skip the room. The engine always starts
        // its own launcher; native asks startupDiscPath() to pre-fill the disc.
        return new String[0];
    }
    /** Native (android_host.c) reads the remembered document for the disc step. */
    public String startupDiscPath() {
        if (DiscActivity.disc == null) {
            String uri = getSharedPreferences("startup", MODE_PRIVATE).getString("discUri", null);
            if (uri != null) try {
                DiscActivity.disc = getContentResolver().openFileDescriptor(Uri.parse(uri), "r");
            } catch (Exception ignored) { /* revoked or gone: the room shows no disc */ }
        }
        return DiscActivity.disc == null ? "" : "/proc/self/fd/" + DiscActivity.disc.getFd();
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
            if(uri!=null) try { DiscActivity.disc=getContentResolver().openFileDescriptor(Uri.parse(uri),"r"); }
            catch(Exception ignored) { /* Re-select revoked documents in the bootstrap screen. */ }
        }
        startLogMirror();
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        super.onCreate(state);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }
    @Override public void onBackPressed() {
        // Send the same Back event as an external keyboard; do not finish the activity.
        dispatchKeyEvent(new android.view.KeyEvent(android.view.KeyEvent.ACTION_DOWN, android.view.KeyEvent.KEYCODE_ESCAPE));
        dispatchKeyEvent(new android.view.KeyEvent(android.view.KeyEvent.ACTION_UP, android.view.KeyEvent.KEYCODE_ESCAPE));
    }

    /* The log the captain can attach: native appends <user root>/moonengine.log
       (never truncated, so a crash line survives the next launch). This small
       thread mirrors the whole file into the user-visible Download collection,
       Download/MoonEngine/moonengine.log, through MediaStore -- Android/data is
       still not reachable by a mail or chat "attach" picker, Download is. The
       backup runs every few seconds, so after a crash and relaunch the copy
       already holds the line the fatal handler wrote. */
    private void startLogMirror() {
        if (Build.VERSION.SDK_INT < 29) return; /* MediaStore.Downloads is API 29+ */
        final File log = new File(userRoot(), "moonengine.log");
        Thread mirror = new Thread(() -> {
            Uri uri = null;
            byte[] buffer = new byte[1 << 16];
            for (;;) {
                try { Thread.sleep(3000); } catch (InterruptedException stop) { return; }
                if (!log.isFile()) continue;
                if (uri == null) uri = downloadEntry();
                if (uri == null) continue;
                try (FileInputStream in = new FileInputStream(log);
                     OutputStream out = getContentResolver().openOutputStream(uri, "wt")) {
                    if (out == null) continue;
                    int read;
                    while ((read = in.read(buffer)) > 0) out.write(buffer, 0, read);
                    out.flush();
                } catch (Exception ignored) { /* a mirror that misses a beat is not fatal */ }
            }
        }, "moonengine-log-mirror");
        mirror.setDaemon(true);
        mirror.start();
    }
    private Uri downloadEntry() {
        try {
            Uri collection = MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
            String relative = Environment.DIRECTORY_DOWNLOADS + "/MoonEngine/";
            Cursor found = getContentResolver().query(collection,
                new String[] { MediaStore.Downloads._ID },
                MediaStore.Downloads.DISPLAY_NAME + "=? AND " + MediaStore.Downloads.RELATIVE_PATH + "=?",
                new String[] { "moonengine.log", relative }, null);
            if (found != null) {
                try {
                    if (found.moveToFirst()) return ContentUris.withAppendedId(collection, found.getLong(0));
                } finally { found.close(); }
            }
            ContentValues values = new ContentValues();
            values.put(MediaStore.Downloads.DISPLAY_NAME, "moonengine.log");
            values.put(MediaStore.Downloads.MIME_TYPE, "text/plain");
            values.put(MediaStore.Downloads.RELATIVE_PATH, relative);
            return getContentResolver().insert(collection, values);
        } catch (Exception ignored) {
            return null;
        }
    }
}
