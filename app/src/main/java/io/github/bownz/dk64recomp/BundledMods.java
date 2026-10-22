package io.github.bownz.dk64recomp;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Mods that come with the app. They're copied into the game's mods folder once per bundled
 * version, so they show up in the Mods menu (disabled until the player enables them), and a
 * player who removes one doesn't get it back until a newer version ships.
 *
 *  - Tag Anywhere 1.0.3 (Killklli/DK64TagAnywhereRecomp, CC0): swap Kongs with the D-pad.
 *    The bottom screen's "Touch Kong Swap" setting uses it.
 */
final class BundledMods {
    private static final String TAG = "DK64Recomp";
    private static final String PREFS = "bundled_mods";
    private static final String[][] MODS = {
        // file name, bundled version
        { "dk64_tag_anywhere.nrm", "1.0.3" },
    };

    private BundledMods() {}

    /** {@code dataDirectory}: the game's data folder (the mods folder is inside it). */
    static void install(Context c, String dataDirectory) {
        if (dataDirectory == null) {
            return;
        }
        SharedPreferences prefs = c.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        File source = new File(new File(c.getFilesDir(), "game"), "bundled-mods");
        File mods = new File(dataDirectory, "mods");
        for (String[] mod : MODS) {
            String name = mod[0], version = mod[1];
            if (version.equals(prefs.getString(name, null))) {
                continue;
            }
            File from = new File(source, name);
            if (!from.isFile()) {
                continue;
            }
            try {
                if (!mods.isDirectory() && !mods.mkdirs()) {
                    throw new IOException("Could not create " + mods);
                }
                File to = new File(mods, name);
                File part = new File(mods, name + ".part");
                try (InputStream in = new FileInputStream(from); OutputStream out = new FileOutputStream(part)) {
                    byte[] buffer = new byte[64 * 1024];
                    int n;
                    while ((n = in.read(buffer)) > 0) {
                        out.write(buffer, 0, n);
                    }
                }
                if (!part.renameTo(to)) {
                    to.delete();
                    if (!part.renameTo(to)) {
                        throw new IOException("Could not write " + to);
                    }
                }
                prefs.edit().putString(name, version).apply();
                Log.i(TAG, "Installed bundled mod " + name + " " + version);
            } catch (IOException e) {
                Log.w(TAG, "Couldn't install bundled mod " + name, e);
            }
        }
    }
}
