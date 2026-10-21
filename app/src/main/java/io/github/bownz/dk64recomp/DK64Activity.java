package io.github.bownz.dk64recomp;

import android.content.ClipData;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;

/**
 * Hosts DK64: Recompiled. SDLActivity (compiled from the SDL2 source tree Gradle
 * downloads) loads libSDL2.so and libmain.so and runs SDL_main on its own thread.
 *
 * This subclass adds two things the desktop build gets for free:
 *  1. The UI assets that sit next to the desktop executable are packaged in the APK
 *     and extracted to internal storage, where the native code can open them.
 *  2. A blocking file picker that the native nfd replacement calls over JNI.
 */
public class DK64Activity extends SDLActivity {
    private static final String TAG = "DK64Recomp";
    private static final String ASSET_ROOT = "game";
    private static final int PICK_REQUEST_CODE = 0x6E6664; // "nfd"

    // Must match RequestMode in android/jni/nfd_android.cpp.
    private static final int MODE_OPEN_FILE = 0;
    private static final int MODE_OPEN_MULTIPLE = 1;

    private final Object pickLock = new Object();
    private CountDownLatch pickLatch;
    private String pickResult;

    @Override
    protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL2", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Must finish before SDL starts the native thread, which happens in super.onCreate
        // and later lifecycle callbacks.
        extractAssetsIfNeeded();
        super.onCreate(savedInstanceState);
    }

    // ------------------------------------------------------------------------------
    // Asset extraction
    // ------------------------------------------------------------------------------

    private void extractAssetsIfNeeded() {
        File target = new File(getFilesDir(), ASSET_ROOT);
        File marker = new File(target, ".extracted-version");
        String version = currentVersionTag();

        if (marker.isFile() && version.equals(readSmallFile(marker))) {
            return;
        }

        Log.i(TAG, "Extracting game assets to " + target);
        deleteRecursively(target);
        try {
            copyAssetTree(getAssets(), ASSET_ROOT, target);
            writeSmallFile(marker, version);
        } catch (IOException e) {
            Log.e(TAG, "Asset extraction failed", e);
        }
    }

    private String currentVersionTag() {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
            // lastUpdateTime changes on every install, so a reinstalled debug build with the
            // same versionCode still refreshes its assets.
            return info.versionName + "/" + info.lastUpdateTime;
        } catch (Exception e) {
            return "unknown";
        }
    }

    private static void copyAssetTree(AssetManager assets, String assetPath, File destination) throws IOException {
        String[] children = assets.list(assetPath);
        if (children == null || children.length == 0) {
            // A file (AssetManager.list returns an empty array for files).
            File parent = destination.getParentFile();
            if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
                throw new IOException("Could not create " + parent);
            }
            try (InputStream in = assets.open(assetPath); OutputStream out = new FileOutputStream(destination)) {
                copyStream(in, out);
            }
            return;
        }
        if (!destination.isDirectory() && !destination.mkdirs()) {
            throw new IOException("Could not create " + destination);
        }
        for (String child : children) {
            copyAssetTree(assets, assetPath + "/" + child, new File(destination, child));
        }
    }

    // ------------------------------------------------------------------------------
    // File picker used by android/jni/nfd_android.cpp
    // ------------------------------------------------------------------------------

    /**
     * Called from a native (non-UI) thread. Shows the system picker, waits for the user,
     * copies the chosen documents into app storage and returns their paths separated by
     * '\n', or null if the user cancelled.
     */
    @SuppressWarnings("unused")
    public String nfdRequest(int mode) {
        synchronized (pickLock) {
            pickResult = null;
            pickLatch = new CountDownLatch(1);

            runOnUiThread(() -> {
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                // N64 ROMs and .nrm mods have no registered MIME type.
                intent.setType("*/*");
                if (mode == MODE_OPEN_MULTIPLE) {
                    intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                }
                try {
                    startActivityForResult(intent, PICK_REQUEST_CODE);
                } catch (Exception e) {
                    Log.e(TAG, "No document picker available", e);
                    pickLatch.countDown();
                }
            });

            try {
                pickLatch.await();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return null;
            }
            return pickResult;
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != PICK_REQUEST_CODE) {
            super.onActivityResult(requestCode, resultCode, data);
            return;
        }

        if (resultCode != RESULT_OK || data == null) {
            finishPick(null);
            return;
        }

        List<Uri> uris = new ArrayList<>();
        ClipData clip = data.getClipData();
        if (clip != null) {
            for (int i = 0; i < clip.getItemCount(); i++) {
                uris.add(clip.getItemAt(i).getUri());
            }
        } else if (data.getData() != null) {
            uris.add(data.getData());
        }

        // Copy off the UI thread: a ROM is tens of megabytes.
        new Thread(() -> {
            StringBuilder paths = new StringBuilder();
            for (Uri uri : uris) {
                try {
                    File copy = importDocument(uri);
                    if (paths.length() > 0) {
                        paths.append('\n');
                    }
                    paths.append(copy.getAbsolutePath());
                } catch (IOException e) {
                    Log.e(TAG, "Could not import " + uri, e);
                }
            }
            finishPick(paths.length() > 0 ? paths.toString() : null);
        }, "dk64-import").start();
    }

    private void finishPick(String result) {
        pickResult = result;
        if (pickLatch != null) {
            pickLatch.countDown();
        }
    }

    /** Copies a picked document into <external files>/imports, keeping its display name. */
    private File importDocument(Uri uri) throws IOException {
        File base = getExternalFilesDir(null);
        if (base == null) {
            base = getFilesDir();
        }
        File importDir = new File(base, "imports");
        if (!importDir.isDirectory() && !importDir.mkdirs()) {
            throw new IOException("Could not create " + importDir);
        }

        File destination = new File(importDir, sanitize(displayName(uri)));
        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(destination)) {
            if (in == null) {
                throw new IOException("Provider returned no stream");
            }
            copyStream(in, out);
        }
        return destination;
    }

    private String displayName(Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                String name = cursor.getString(0);
                if (name != null && !name.isEmpty()) {
                    return name;
                }
            }
        } catch (Exception ignored) {
        }
        String last = uri.getLastPathSegment();
        return last != null ? last : "imported_file";
    }

    private static String sanitize(String name) {
        return name.replaceAll("[\\\\/:*?\"<>|]", "_");
    }

    // ------------------------------------------------------------------------------
    // Small IO helpers
    // ------------------------------------------------------------------------------

    private static void copyStream(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1 << 16];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
    }

    private static String readSmallFile(File file) {
        try (InputStream in = new java.io.FileInputStream(file)) {
            byte[] bytes = new byte[(int) Math.min(file.length(), 4096)];
            int read = in.read(bytes);
            return read > 0 ? new String(bytes, 0, read, "UTF-8") : "";
        } catch (IOException e) {
            return "";
        }
    }

    private static void writeSmallFile(File file, String contents) throws IOException {
        try (OutputStream out = new FileOutputStream(file)) {
            out.write(contents.getBytes("UTF-8"));
        }
    }

    private static void deleteRecursively(File file) {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRecursively(child);
            }
        }
        //noinspection ResultOfMethodCallIgnored
        file.delete();
    }
}
