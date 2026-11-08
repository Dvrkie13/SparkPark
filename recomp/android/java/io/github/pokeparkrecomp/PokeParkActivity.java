package io.github.pokeparkrecomp;

import android.app.AlertDialog;
import android.app.NativeActivity;
import android.app.Presentation;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import android.view.Display;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * The app's activity: NativeActivity (window_android.c runs the game), and
 * the one thing it can't do, asking for the player's disc image. The file
 * picker's answer comes back to an activity, so this asks, keeps a lasting
 * permission to read the file, remembers it, and hands the native side an
 * open file descriptor (disc_image_android.c reads the game from it).
 * And for the update check (update.c): fetching the latest release's
 * details, and opening its page in the browser. And on a device with a
 * second screen (the AYN Thor's lower one), a window there whose surface
 * and touches go to the native side (second_screen_android.c).
 */
public class PokeParkActivity extends NativeActivity {
    private static final int PICK = 1;
    private static final String DISC = "disc";
    private volatile int pickedFd = -1;
    private String asking;

    private SharedPreferences prefs() {
        return getSharedPreferences("pokepark", MODE_PRIVATE);
    }

    private int open(Uri uri) {
        try {
            ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r");
            return pfd == null ? -1 : pfd.detachFd();
        } catch (Exception e) {
            Log.w("PokePark", "can't open " + uri, e);
            return -1;
        }
    }

    /** The disc image chosen before, opened, or -1 (from native code). */
    public int savedGameFd() {
        String uri = prefs().getString(DISC, null);
        return uri == null ? -1 : open(Uri.parse(uri));
    }

    /** It wasn't the game after all (from native code). */
    public void forgetGame() {
        prefs().edit().remove(DISC).apply();
    }

    /** The disc image just chosen, once, or -1 (polled from native code). */
    public int takePickedFd() {
        int fd = pickedFd;
        pickedFd = -1;
        return fd;
    }

    /** An HTTPS GET's body, or null (from native code, off the main thread). */
    public String httpGet(String url, String userAgent) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(url).openConnection();
            c.setConnectTimeout(10000);
            c.setReadTimeout(10000);
            c.setRequestProperty("Accept", "application/vnd.github+json");
            c.setRequestProperty("User-Agent", userAgent);
            if (c.getResponseCode() != 200) return null;
            InputStream in = c.getInputStream();
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            for (int n; out.size() < 256 * 1024 && (n = in.read(buf)) > 0;) out.write(buf, 0, n);
            in.close();
            return out.toString("UTF-8");
        } catch (Exception e) {
            Log.w("PokePark", "update check: " + e);
            return null;
        } finally {
            if (c != null) c.disconnect();
        }
    }

    /** The second screen's surface, or null when it goes, and its size. */
    private static native void secondSurface(Surface surface, int width, int height);
    /** A touch on it: 0 down, 1 up, 2 moved, at x, y in its pixels. */
    private static native void secondTouch(int action, float x, float y);

    private Presentation second;

    /** Opens a window on a second screen, if there is one (from native
     * code, once its methods above are registered). */
    public void showSecondScreen() {
        runOnUiThread(() -> {
            if (second != null) return;
            DisplayManager dm = (DisplayManager) getSystemService(Context.DISPLAY_SERVICE);
            Display[] displays = dm.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION);
            if (displays.length == 0) return;
            second = new Presentation(this, displays[0]);
            SurfaceView view = new SurfaceView(second.getContext());
            view.getHolder().addCallback(new SurfaceHolder.Callback() {
                public void surfaceCreated(SurfaceHolder h) {}
                public void surfaceChanged(SurfaceHolder h, int format, int w, int hgt) {
                    secondSurface(h.getSurface(), w, hgt);
                }
                public void surfaceDestroyed(SurfaceHolder h) {
                    secondSurface(null, 0, 0);
                }
            });
            view.setOnTouchListener((v, e) -> {
                int a = e.getActionMasked();
                int kind = a == MotionEvent.ACTION_DOWN ? 0 : a == MotionEvent.ACTION_UP || a == MotionEvent.ACTION_CANCEL ? 1
                         : a == MotionEvent.ACTION_MOVE ? 2 : -1;
                if (kind >= 0) secondTouch(kind, e.getX(), e.getY());
                return true;
            });
            second.setContentView(view);
            try {
                second.show();
            } catch (Exception e) {
                Log.w("PokePark", "no second screen: " + e);
                second = null;
            }
        });
    }

    @Override
    protected void onDestroy() {
        if (second != null) second.dismiss();
        second = null;
        super.onDestroy();
    }

    /** Opens url in the browser (from native code). */
    public void openPage(final String url) {
        runOnUiThread(() -> {
            try {
                startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
            } catch (Exception e) {
                Log.w("PokePark", "can't open " + url, e);
            }
        });
    }

    /** Asks for the disc image, saying message (from native code). */
    public void chooseGame(final String message) {
        runOnUiThread(new Runnable() {
            public void run() {
                ask(message);
            }
        });
    }

    private void ask(String message) {
        asking = message;
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
            .setTitle("PokePark")
            .setMessage(message)
            .setCancelable(false)
            .setPositiveButton("Choose disc image", (d, w) -> {
                Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                pick.addCategory(Intent.CATEGORY_OPENABLE);
                pick.setType("*/*");
                startActivityForResult(pick, PICK);
            })
            .setNegativeButton("Quit", (d, w) -> finish())
            .show();
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        if (request != PICK) {
            super.onActivityResult(request, result, data);
            return;
        }
        Uri uri = result == RESULT_OK && data != null ? data.getData() : null;
        if (uri == null) {
            ask(asking); // backed out of the picker: there's nothing to play yet
            return;
        }
        try {
            getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (Exception e) {
            Log.w("PokePark", "no lasting permission for " + uri, e);
        }
        int fd = open(uri);
        if (fd < 0) {
            ask("That file couldn't be opened. Choose your disc image of PokePark Wii again.");
            return;
        }
        prefs().edit().putString(DISC, uri.toString()).apply();
        pickedFd = fd;
    }
}
