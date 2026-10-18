package org.moonengine.android;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.system.Os;
import android.system.OsConstants;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.IOException;

/** Host bootstrap only: no copied game data or broad storage permission. */
public final class DiscActivity extends Activity {
    private static final int DISC = 1;
    private TextView message;
    static ParcelFileDescriptor disc;

    @Override protected void onResume() {
        super.onResume();
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(40, 24, 40, 24);
        layout.setBackgroundColor(0xfffefefd);
        TextView title = new TextView(this);
        title.setText("MOON ENGINE"); title.setTextSize(32);
        layout.addView(title);
        message = new TextView(this);
        message.setGravity(Gravity.CENTER);
        message.setText("Choose your own Gran Turismo 4 USA v2.00 ISO.\nSaves stay in this app's private storage.");
        message.setTextSize(18); layout.addView(message);
        Button choose = new Button(this); choose.setText("CHOOSE DISC");
        choose.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            startActivityForResult(intent, DISC);
        });
        layout.addView(choose); setContentView(layout);
        String saved = getSharedPreferences("startup", MODE_PRIVATE).getString("discUri", null);
        if (saved != null) open(Uri.parse(saved));
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != DISC || result != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        try {
            getContentResolver().takePersistableUriPermission(uri, data.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException ignored) { /* Session access may still be available. */ }
        open(uri);
    }
    private void open(Uri uri) {
        ParcelFileDescriptor selected = null;
        try {
            selected = getContentResolver().openFileDescriptor(uri, "r");
            if (selected == null) throw new IOException("File provider returned no file.");
            Os.lseek(selected.getFileDescriptor(), 0, OsConstants.SEEK_SET);
            long size = Os.fstat(selected.getFileDescriptor()).st_size;
            if (size < 2048) throw new IOException("Choose a local, seekable ISO, not a cloud stream.");
            if (disc != null) disc.close();
            disc = selected;
            getSharedPreferences("startup", MODE_PRIVATE).edit().putString("discUri", uri.toString()).apply();
            startActivity(new Intent(this, GameActivity.class));
            finish();
        } catch (Exception error) {
            if (selected != null && selected != disc) try { selected.close(); } catch (IOException ignored) {}
            message.setText("Could not open this disc: " + error.getMessage() + "\nChoose a local ISO.");
        }
    }
}
