package io.github.bownz.dk64recomp;

import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.hardware.display.DisplayManager;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.provider.OpenableColumns;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

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
 * On top of SDLActivity this adds:
 *  - Asset extraction: the UI files that sit next to the desktop executable are packaged in
 *    the APK and extracted to internal storage, where the native code can open them.
 *  - Storage: where saves, mods and the ROM live, with the permission flow
 *    ({@link StorageAccess}); the native side asks via {@link #getDataDirectory()}.
 *  - A blocking document picker that the native nfd replacement calls over JNI.
 *  - Touch controls ({@link TouchControllerView}) as an overlay when there's no physical
 *    gamepad, and the DK64 bottom screen ({@link BottomScreenView}): health, collectibles and action buttons on a second screen if there is
 *    one ({@link SecondScreenPresentation}).
 *  - Game-friendly window behaviour: immersive fullscreen, screen kept on, drawing into
 *    the display cutout, sustained-performance mode.
 */
public class DK64Activity extends SDLActivity
        implements InputManager.InputDeviceListener, DisplayManager.DisplayListener {
    private static final String TAG = "DK64Recomp";
    private static final String ASSET_ROOT = "game";
    private static final int PICK_REQUEST_CODE = 0x6E6664; // "nfd"

    // Must match RequestMode in android/jni/nfd_android.cpp.
    private static final int MODE_OPEN_FILE = 0;
    private static final int MODE_OPEN_MULTIPLE = 1;

    private StorageAccess storage;

    private TouchControllerView overlay;
    private SecondScreenPresentation presentation;
    private InputManager inputManager;
    private DisplayManager displayManager;
    private boolean started;

    private final Object pickLock = new Object();
    private CountDownLatch pickLatch;
    private String pickResult;

    @Override
    protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL2", "main" };
    }

    // ------------------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------------------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Must finish before SDL starts the native thread, which happens in super.onCreate
        // and later lifecycle callbacks.
        extractAssetsIfNeeded();
        storage = new StorageAccess(this);
        Diagnostics.rotateLogs(this);

        super.onCreate(savedInstanceState);

        configureWindow();
        offerCrashReport();

        inputManager = (InputManager) getSystemService(Context.INPUT_SERVICE);
        displayManager = (DisplayManager) getSystemService(Context.DISPLAY_SERVICE);

        // SDLActivity builds its layout (with the game surface) in onCreate. If the native
        // libraries failed to load it shows an error instead and mLayout stays null.
        if (mLayout != null) {
            overlay = new TouchControllerView(this);
            mLayout.addView(overlay, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        started = true;
        updateTouchControls();
    }

    @Override
    protected void onResume() {
        super.onResume();
        storage.onResume();
        inputManager.registerInputDeviceListener(this, null);
        displayManager.registerDisplayListener(this, null);
        hideSystemBars();
        // Controllers or displays may have changed while we were away.
        updateTouchControls();
    }

    @Override
    protected void onPause() {
        // Nothing may stay held while the game can't see the player's fingers.
        if (overlay != null) {
            overlay.releaseAll();
        }
        if (presentation != null && presentation.getBottomScreen() != null) {
            presentation.getBottomScreen().cancel();
        }
        inputManager.unregisterInputDeviceListener(this);
        displayManager.unregisterDisplayListener(this);
        super.onPause();
    }

    @Override
    protected void onStop() {
        // A Presentation is a window of this activity: it must go when the activity stops.
        started = false;
        dismissPresentation();
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        dismissPresentation();
        TouchInput.shutdown();
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            // System bars come back after dialogs, the notification shade, etc.
            hideSystemBars();
            // By now the game has usually started SDL: create the shared virtual gamepad if
            // the overlay or action pad wanted it before SDL was ready.
            TouchInput.ensure();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (!storage.onRequestPermissionsResult(requestCode)) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }

    private void configureWindow() {
        Window window = getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // Use the whole panel, including the area beside a notch or camera cutout.
        WindowManager.LayoutParams attributes = window.getAttributes();
        attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        window.setAttributes(attributes);

        // Steadier clocks over a long session instead of bursts followed by throttling.
        PowerManager power = (PowerManager) getSystemService(Context.POWER_SERVICE);
        if (power != null && power.isSustainedPerformanceModeSupported()) {
            window.setSustainedPerformanceMode(true);
        }
    }

    @SuppressWarnings("deprecation")
    private void hideSystemBars() {
        Window window = getWindow();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false);
            WindowInsetsController controller = window.getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            window.getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        }
    }

    // ------------------------------------------------------------------------------
    // Storage (called from android/jni/android_main.cpp before the game starts)
    // ------------------------------------------------------------------------------

    /**
     * Where saves, config, mods and the imported ROM go. On first launch this asks the
     * player (and requests the storage permission if needed), blocking the native thread
     * until they answer. That happens before the game creates its window.
     */
    @SuppressWarnings("unused")
    /**
     * Called from the native side (android/jni/gpu_driver.cpp) as the renderer starts: the
     * custom Vulkan driver to load, or null for the system driver. Chosen from the app icon's
     * "Graphics driver" shortcut (GpuDriverActivity).
     */
    public String getGpuDriver() {
        String driver = GpuDrivers.forRenderer(this);
        if (GpuDrivers.takeRevertedNotice(this)) {
            runOnUiThread(() -> android.widget.Toast.makeText(this,
                "The custom graphics driver didn't start last time, so the system driver is being used.",
                android.widget.Toast.LENGTH_LONG).show());
        }
        if (driver != null) {
            // Still running after a while: the driver works, keep it.
            new android.os.Handler(getMainLooper()).postDelayed(() -> GpuDrivers.confirmWorking(this), GpuDrivers.TRIAL_MS);
        }
        return driver;
    }

    /** For the in-game Graphics settings: the selected driver id, then "id\tlabel" lines. */
    public String getGpuDriverList() {
        GpuDrivers.Driver selected = GpuDrivers.selected(this);
        StringBuilder sb = new StringBuilder(selected == null ? "" : selected.id);
        for (GpuDrivers.Driver d : GpuDrivers.installed(this)) {
            sb.append('\n').append(d.id).append('\t').append(d.label().replace('\t', ' ').replace('\n', ' '));
        }
        return sb.toString();
    }

    /** From the in-game Graphics settings: use this driver ("" = system) from the next start. */
    public String selectGpuDriver(String id) {
        GpuDrivers.Driver pick = null;
        for (GpuDrivers.Driver d : GpuDrivers.installed(this)) {
            if (d.id.equals(id)) {
                pick = d;
            }
        }
        GpuDrivers.select(this, pick);
        return null;
    }

    /** From the in-game Graphics settings: install or remove driver packages. */
    public String openGpuDriverManager() {
        runOnUiThread(() -> startActivity(new Intent(this, GpuDriverActivity.class)
            .putExtra(GpuDriverActivity.EXTRA_DRIVERS, true).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)));
        return null;
    }

    /** If the game crashed or froze last time, offer to share the log. */
    private void offerCrashReport() {
        final String crash = Diagnostics.takeUnreportedCrash(this);
        if (crash == null) {
            return;
        }
        new android.app.AlertDialog.Builder(this)
            .setTitle("DK64 closed unexpectedly")
            .setMessage("Last time the game had a " + crash + ".\n\nShare the log so it can be fixed? "
                + "It contains the game's technical output and your device model, nothing personal.")
            .setPositiveButton("Share log", (dialog, which) -> Diagnostics.share(this, crash))
            .setNegativeButton("Not now", null)
            .show();
    }

    /** From the in-game settings (Technical > Share Log). */
    public String shareLog() {
        runOnUiThread(() -> Diagnostics.share(this, null));
        return null;
    }

    /** For the in-game settings: e.g. "0.2.0-beta (76e2c32)". */
    public String getAppVersion() {
        return Diagnostics.version(this);
    }

    public String getDataDirectory() {
        String dir = storage.resolveDataDirectory();
        RomIcons.dataDirectory = dir;
        BundledMods.install(this, dir);
        return dir;
    }

    // ------------------------------------------------------------------------------
    // Touch controls: overlay, second screen, or none
    // ------------------------------------------------------------------------------

    /**
     * Main screen: the touch controller overlay, only when there's no physical gamepad
     * (a handheld's built-in controls count as one).
     * Second display, if any (e.g. the AYN Thor's lower screen): DK64 action buttons, always,
     * since they add one-tap Z+C moves on top of whatever controls are in use.
     */
    private void updateTouchControls() {
        if (overlay == null) {
            return;
        }

        overlay.setActive(!hasPhysicalGamepad());

        Display second = started ? findSecondDisplay() : null;
        if (second == null || (presentation != null && presentation.getDisplay().getDisplayId() != second.getDisplayId())) {
            dismissPresentation();
        }
        if (second != null && presentation == null) {
            showPresentation(second);
        }
    }

    private boolean hasPhysicalGamepad() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice device = InputDevice.getDevice(id);
            if (device == null || device.isVirtual()) {
                continue;
            }
            int sources = device.getSources();
            if ((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                    || (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) {
                return true;
            }
        }
        return false;
    }

    /** A display other than ours that can host a presentation, if any. */
    private Display findSecondDisplay() {
        Display ours = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
            ? getDisplay() : getDefaultDisplayLegacy();
        for (Display display : displayManager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)) {
            if (display.getDisplayId() != ours.getDisplayId() && display.isValid()) {
                return display;
            }
        }
        return null;
    }

    @SuppressWarnings("deprecation")
    private Display getDefaultDisplayLegacy() {
        return getWindowManager().getDefaultDisplay();
    }

    private void showPresentation(Display display) {
        try {
            presentation = new SecondScreenPresentation(this, display);
            presentation.setOnDismissListener(d -> {
                if (presentation == d) {
                    presentation = null;
                }
            });
            presentation.show();
            Log.i(TAG, "Action buttons on display " + display.getDisplayId() + " (" + display.getName() + ")");
        } catch (WindowManager.InvalidDisplayException e) {
            Log.w(TAG, "Second display went away", e);
            presentation = null;
        }
    }

    private void dismissPresentation() {
        if (presentation != null) {
            SecondScreenPresentation old = presentation;
            presentation = null;
            old.dismiss();
        }
    }

    @Override public void onInputDeviceAdded(int deviceId) { updateTouchControls(); }
    @Override public void onInputDeviceRemoved(int deviceId) { updateTouchControls(); }
    @Override public void onInputDeviceChanged(int deviceId) { updateTouchControls(); }
    @Override public void onDisplayAdded(int displayId) { updateTouchControls(); }
    @Override public void onDisplayRemoved(int displayId) { updateTouchControls(); }
    @Override public void onDisplayChanged(int displayId) { }

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
     * and returns real paths for the chosen documents, separated by '\n', or null if the
     * user cancelled. Documents the app can read directly (shared-folder mode) are used in
     * place; anything else is copied into app storage first.
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
                    File file = storage.directFileFor(uri);
                    if (file == null) {
                        file = importDocument(uri);
                    }
                    if (paths.length() > 0) {
                        paths.append('\n');
                    }
                    paths.append(file.getAbsolutePath());
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

    /** Copies a picked document into <app external files>/imports, keeping its display name. */
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
