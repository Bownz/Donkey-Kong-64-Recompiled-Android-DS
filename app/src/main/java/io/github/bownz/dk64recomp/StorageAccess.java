package io.github.bownz.dk64recomp;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.util.Log;
import android.widget.Toast;

import java.io.File;
import java.util.concurrent.CountDownLatch;

/**
 * Decides where DK64 keeps saves, config, mods and the imported ROM, and asks for the
 * storage permission that the shared-folder option needs.
 *
 * Two options, chosen once on first launch and remembered:
 *  - Shared folder: "DK64Recompiled" at the top of the device's storage. Easy to reach
 *    from a PC or file manager to drop in mods, and it survives uninstalling the app.
 *    Needs "All files access" on Android 11+ (a Settings screen, not a dialog), or the
 *    classic storage permission on Android 9-10.
 *  - App storage: Android/data/<package>/files. Needs no permission; removed on uninstall.
 *
 * {@link #resolveDataDirectory()} is called from the game's native thread before it
 * creates its window, and blocks until the player has answered. The activity must
 * forward onResume and onRequestPermissionsResult so the answer is noticed.
 */
final class StorageAccess {
    private static final String TAG = "DK64Recomp";
    private static final String PREFS = "storage";
    private static final String KEY_MODE = "mode";
    private static final String MODE_SHARED = "shared";
    private static final String MODE_APP = "app";
    static final String SHARED_FOLDER_NAME = "DK64Recompiled";
    static final int REQUEST_LEGACY_STORAGE = 0x5354;

    private final Activity activity;
    private final SharedPreferences prefs;

    private CountDownLatch answer;
    private String resolvedPath;
    private boolean waitingForSettings;
    private boolean waitingForPermissionDialog;

    StorageAccess(Activity activity) {
        this.activity = activity;
        this.prefs = activity.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    // ------------------------------------------------------------------------------
    // Queries
    // ------------------------------------------------------------------------------

    static boolean hasSharedStorageAccess(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return Environment.isExternalStorageManager();
        }
        return context.checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED
            && context.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    boolean usesSharedFolder() {
        return MODE_SHARED.equals(prefs.getString(KEY_MODE, null)) && hasSharedStorageAccess(activity);
    }

    private File sharedFolder() {
        return new File(Environment.getExternalStorageDirectory(), SHARED_FOLDER_NAME);
    }

    private File appFolder() {
        File external = activity.getExternalFilesDir(null);
        return external != null ? external : new File(activity.getFilesDir(), "data");
    }

    /**
     * For a document picked with the system file picker, returns the real file when the
     * app can read it directly (shared-folder mode), so a ROM needn't be copied. Returns
     * null when the document has to be copied in through its content stream instead.
     */
    File directFileFor(Uri uri) {
        if (!usesSharedFolder() || uri == null || !DocumentsContract.isDocumentUri(activity, uri)) {
            return null;
        }
        String id = DocumentsContract.getDocumentId(uri);
        File file = null;
        if ("com.android.externalstorage.documents".equals(uri.getAuthority()) && id.startsWith("primary:")) {
            file = new File(Environment.getExternalStorageDirectory(), id.substring("primary:".length()));
        } else if (id.startsWith("raw:")) {
            // The Downloads provider exposes some documents by their real path.
            file = new File(id.substring("raw:".length()));
        }
        return (file != null && file.isFile() && file.canRead()) ? file : null;
    }

    // ------------------------------------------------------------------------------
    // Resolution (called from the native thread)
    // ------------------------------------------------------------------------------

    /** Returns the user-data directory, asking the player first if they haven't chosen. */
    String resolveDataDirectory() {
        String mode = prefs.getString(KEY_MODE, null);

        if (MODE_APP.equals(mode)) {
            return ensureDirectory(appFolder());
        }
        if (MODE_SHARED.equals(mode)) {
            if (hasSharedStorageAccess(activity)) {
                String path = ensureDirectory(sharedFolder());
                if (path != null) {
                    return path;
                }
            }
            Log.w(TAG, "Shared folder chosen but not accessible (permission removed?); asking again");
        }

        answer = new CountDownLatch(1);
        activity.runOnUiThread(this::showChoice);
        try {
            answer.await();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return ensureDirectory(appFolder());
        }
        return resolvedPath;
    }

    private void showChoice() {
        new AlertDialog.Builder(activity)
            .setTitle(R.string.storage_title)
            .setMessage(activity.getString(R.string.storage_message, SHARED_FOLDER_NAME))
            .setCancelable(false)
            .setPositiveButton(R.string.storage_shared, (d, w) -> requestSharedAccess())
            .setNegativeButton(R.string.storage_app, (d, w) -> choose(MODE_APP))
            .show();
    }

    private void requestSharedAccess() {
        if (hasSharedStorageAccess(activity)) {
            finishSharedRequest();
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            // "All files access" is granted on a Settings screen; we hear back in onResume().
            Toast.makeText(activity, R.string.storage_settings_hint, Toast.LENGTH_LONG).show();
            waitingForSettings = true;
            Intent appScreen = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                Uri.parse("package:" + activity.getPackageName()));
            try {
                activity.startActivity(appScreen);
            } catch (ActivityNotFoundException e) {
                try {
                    activity.startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
                } catch (ActivityNotFoundException e2) {
                    waitingForSettings = false;
                    Log.w(TAG, "No settings screen for all-files access", e2);
                    finishSharedRequest();
                }
            }
        } else {
            waitingForPermissionDialog = true;
            activity.requestPermissions(new String[] {
                Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
            }, REQUEST_LEGACY_STORAGE);
        }
    }

    /** Forward from Activity.onResume(): picks up the result of the Settings screen. */
    void onResume() {
        if (waitingForSettings) {
            waitingForSettings = false;
            finishSharedRequest();
        }
    }

    /** Forward from Activity.onRequestPermissionsResult(). Returns true if it was ours. */
    boolean onRequestPermissionsResult(int requestCode) {
        if (requestCode != REQUEST_LEGACY_STORAGE || !waitingForPermissionDialog) {
            return false;
        }
        waitingForPermissionDialog = false;
        finishSharedRequest();
        return true;
    }

    private void finishSharedRequest() {
        if (hasSharedStorageAccess(activity) && ensureDirectory(sharedFolder()) != null) {
            choose(MODE_SHARED);
        } else {
            Toast.makeText(activity, R.string.storage_denied, Toast.LENGTH_LONG).show();
            choose(MODE_APP);
        }
    }

    private void choose(String mode) {
        prefs.edit().putString(KEY_MODE, mode).apply();
        String path = MODE_SHARED.equals(mode) ? ensureDirectory(sharedFolder()) : null;
        if (path == null) {
            path = ensureDirectory(appFolder());
        }
        Log.i(TAG, "User data folder: " + path);
        resolvedPath = path;
        if (answer != null) {
            answer.countDown();
        }
    }

    private static String ensureDirectory(File dir) {
        if (dir.isDirectory() || dir.mkdirs()) {
            return dir.getAbsolutePath();
        }
        Log.w(TAG, "Could not create " + dir);
        return null;
    }
}
