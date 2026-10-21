package io.github.bownz.dk64recomp;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Process;
import android.widget.Toast;

import java.util.List;

/**
 * "Graphics driver" screen, opened from the app icon's long-press shortcut. Lists the system
 * driver and any installed custom drivers, installs new ones from a .zip, and restarts the
 * game so the choice takes effect.
 *
 * Runs in its own process (":settings") so it can restart the game's process.
 */
public final class GpuDriverActivity extends Activity {
    private static final int PICK_ZIP = 0x6770;

    private boolean changed;

    /** Opens straight to the driver list (from the in-game settings). */
    static final String EXTRA_DRIVERS = "drivers";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // (Static shortcuts pass extras as strings.)
        if (getIntent().getBooleanExtra(EXTRA_DRIVERS, false) || "true".equals(getIntent().getStringExtra(EXTRA_DRIVERS))) {
            showChooser();
        } else {
            showTools();
        }
    }

    /** "DK64 Tools": graphics driver, share log, version. */
    private void showTools() {
        GpuDrivers.Driver current = GpuDrivers.selected(this);
        String[] items = {
            "Graphics driver: " + (current == null ? "System driver" : current.label()),
            "Share log",
        };
        new AlertDialog.Builder(this)
            .setTitle("DK64 Tools · " + Diagnostics.version(this))
            .setItems(items, (dialog, which) -> {
                if (which == 0) {
                    showChooser();
                } else {
                    Diagnostics.share(this, null);
                    finish();
                }
            })
            .setOnCancelListener(dialog -> finish())
            .show();
    }

    private void showChooser() {
        List<GpuDrivers.Driver> drivers = GpuDrivers.installed(this);
        GpuDrivers.Driver current = GpuDrivers.selected(this);
        String[] labels = new String[drivers.size() + 1];
        labels[0] = "System driver (default)";
        int checked = 0;
        for (int i = 0; i < drivers.size(); i++) {
            labels[i + 1] = drivers.get(i).label();
            if (current != null && current.id.equals(drivers.get(i).id)) {
                checked = i + 1;
            }
        }
        final int[] choice = { checked };
        new AlertDialog.Builder(this)
            .setTitle("Graphics driver")
            .setSingleChoiceItems(labels, checked, (dialog, which) -> choice[0] = which)
            .setPositiveButton("Use", (dialog, which) -> {
                GpuDrivers.Driver picked = choice[0] == 0 ? null : drivers.get(choice[0] - 1);
                GpuDrivers.Driver before = GpuDrivers.selected(this);
                GpuDrivers.select(this, picked);
                changed = (before == null) != (picked == null) || (before != null && !before.id.equals(picked.id));
                finishWithRestart();
            })
            .setNeutralButton("Install .zip…", (dialog, which) -> pickZip())
            .setNegativeButton(drivers.isEmpty() ? "Close" : "Remove…", (dialog, which) -> {
                if (drivers.isEmpty()) {
                    finishWithRestart();
                } else {
                    showRemove(drivers);
                }
            })
            .setOnCancelListener(dialog -> finishWithRestart())
            .show();
    }

    private void showRemove(List<GpuDrivers.Driver> drivers) {
        String[] labels = new String[drivers.size()];
        for (int i = 0; i < drivers.size(); i++) {
            labels[i] = drivers.get(i).label();
        }
        new AlertDialog.Builder(this)
            .setTitle("Remove a driver")
            .setItems(labels, (dialog, which) -> {
                GpuDrivers.Driver d = drivers.get(which);
                boolean wasSelected = GpuDrivers.selected(this) != null && GpuDrivers.selected(this).id.equals(d.id);
                GpuDrivers.remove(this, d);
                changed |= wasSelected;
                showChooser();
            })
            .setOnCancelListener(dialog -> showChooser())
            .show();
    }

    private void pickZip() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] { "application/zip", "application/x-zip-compressed", "application/octet-stream" });
        startActivityForResult(intent, PICK_ZIP);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_ZIP) {
            return;
        }
        Uri uri = resultCode == RESULT_OK && data != null ? data.getData() : null;
        if (uri != null) {
            try {
                GpuDrivers.Driver d = GpuDrivers.install(this, uri);
                Toast.makeText(this, "Installed " + d.label(), Toast.LENGTH_SHORT).show();
            } catch (Exception e) {
                new AlertDialog.Builder(this)
                    .setTitle("Couldn't install the driver")
                    .setMessage(e.getMessage())
                    .setPositiveButton("OK", (dialog, which) -> showChooser())
                    .setOnCancelListener(dialog -> showChooser())
                    .show();
                return;
            }
        }
        showChooser();
    }

    /** Applies a change by restarting the game (if it's running), then closes. */
    private void finishWithRestart() {
        if (changed && gameRunning()) {
            new AlertDialog.Builder(this)
                .setTitle("Restart the game?")
                .setMessage("The new graphics driver is used from the next start. Unsaved progress will be lost if you restart now.")
                .setPositiveButton("Restart now", (dialog, which) -> {
                    killGame();
                    startActivity(new Intent(this, DK64Activity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
                    finish();
                })
                .setNegativeButton("Later", (dialog, which) -> finish())
                .setOnCancelListener(dialog -> finish())
                .show();
            return;
        }
        finish();
    }

    private ActivityManager.RunningAppProcessInfo gameProcess() {
        ActivityManager am = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.RunningAppProcessInfo> processes = am.getRunningAppProcesses();
        if (processes == null) {
            return null;
        }
        for (ActivityManager.RunningAppProcessInfo info : processes) {
            if (info.processName.equals(getPackageName())) {
                return info;
            }
        }
        return null;
    }

    private boolean gameRunning() {
        return gameProcess() != null;
    }

    private void killGame() {
        ActivityManager.RunningAppProcessInfo game = gameProcess();
        if (game != null) {
            Process.killProcess(game.pid);
        }
    }
}
