package io.github.bownz.dk64recomp;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.Log;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.zip.GZIPInputStream;

/**
 * The game's own icons (melon, orange, film, golden banana, Kong weapons and instruments)
 * for the bottom screen, decoded from the player's ROM on the device. No game art ships in
 * the APK: everything here is read from the ROM the player picked (stored by the runtime as
 * DK64.z64 in the data folder). Anything that can't be found or doesn't decode as expected
 * stays null, and the caller draws its own vector icon instead.
 *
 * ROM layout (US 1.0, as documented by the DK64 decomp and randomizer projects): a pointer
 * table at 0x101C50 holds 32 tables; word t is table t's offset, word 32 + t its file count.
 * File entries are offsets from the same base (bit 31 marks a redirect to another file).
 * Table 7 is stored raw, the others gzip-compressed. Textures are RGBA5551, big-endian;
 * the sprite-style ones are stored bottom row first.
 */
final class RomIcons {
    private static final String TAG = "DK64Recomp";
    private static final long POINTER_TABLE = 0x101C50;
    private static final int TABLE_COUNT = 32;
    private static final int TEXTURES_UNCOMPRESSED = 7;
    private static final int TEXTURES_GEOMETRY = 25;

    /** Where each icon lives: table, file, width, height, stored upside down. */
    private static final class Spec {
        final int table, file, width, height;
        final boolean flipped;

        Spec(int table, int file, int width, int height, boolean flipped) {
            this.table = table;
            this.file = file;
            this.width = width;
            this.height = height;
            this.flipped = flipped;
        }
    }

    private static final Spec MELON = new Spec(TEXTURES_UNCOMPRESSED, 544, 48, 42, true);
    private static final Spec ORANGE = new Spec(TEXTURES_UNCOMPRESSED, 0x136, 32, 32, true);
    private static final Spec FILM = new Spec(TEXTURES_UNCOMPRESSED, 439, 48, 42, true);
    private static final Spec GOLDEN_BANANA = new Spec(TEXTURES_GEOMETRY, 5468, 44, 44, true);
    // Per Kong: DK, Diddy, Lanky, Tiny, Chunky.
    private static final Spec[] BANANA = {
        new Spec(TEXTURES_UNCOMPRESSED, 0xA8, 44, 44, true),
        new Spec(TEXTURES_UNCOMPRESSED, 0x98, 44, 44, true),
        new Spec(TEXTURES_UNCOMPRESSED, 0xE8, 44, 44, true),
        new Spec(TEXTURES_UNCOMPRESSED, 0xD0, 44, 44, true),
        new Spec(TEXTURES_UNCOMPRESSED, 0xF0, 44, 44, true),
    };
    private static final Spec[] WEAPON = {
        new Spec(TEXTURES_UNCOMPRESSED, 0x2A3, 40, 51, true),  // coconut
        new Spec(TEXTURES_GEOMETRY, 0x1474, 32, 32, true),     // peanut
        new Spec(TEXTURES_GEOMETRY, 0x1506, 32, 32, true),     // grape
        new Spec(TEXTURES_GEOMETRY, 0x15A3, 32, 32, true),     // feather
        new Spec(TEXTURES_GEOMETRY, 0x12E5, 32, 48, true),     // pineapple
    };
    private static final Spec[] INSTRUMENT = {
        new Spec(TEXTURES_GEOMETRY, 0x15AC, 40, 40, true),     // bongos
        new Spec(TEXTURES_GEOMETRY, 0x15AB, 40, 40, true),     // guitar
        new Spec(TEXTURES_GEOMETRY, 0x15AF, 40, 40, true),     // trombone
        new Spec(TEXTURES_GEOMETRY, 0x15AD, 40, 40, true),     // saxophone
        new Spec(TEXTURES_GEOMETRY, 0x15AE, 40, 40, true),     // triangle
    };

    static volatile Bitmap melon, orange, film, goldenBanana;
    static final Bitmap[] banana = new Bitmap[5];
    static final Bitmap[] weapon = new Bitmap[5];
    static final Bitmap[] instrument = new Bitmap[5];

    /** The game's data folder (holding DK64.z64), set by DK64Activity once it's known. */
    static volatile String dataDirectory;

    private static volatile boolean started;
    private static final Paint PAINT = new Paint(Paint.FILTER_BITMAP_FLAG);
    private static final RectF DST = new RectF();

    private RomIcons() {}

    /**
     * Starts decoding in the background once the ROM is in the data folder; calls
     * {@code onLoaded} (on that thread) when done. Safe to call repeatedly.
     */
    static void ensureLoaded(Runnable onLoaded) {
        String dataDir = dataDirectory;
        if (started || dataDir == null) {
            return;
        }
        File rom = new File(dataDir, "DK64.z64");
        if (!rom.isFile()) {
            return;
        }
        started = true;
        new Thread(() -> {
            try (RandomAccessFile f = new RandomAccessFile(rom, "r")) {
                if (!isUsRom(f)) {
                    Log.w(TAG, "Bottom screen: ROM isn't the US release, using built-in icons");
                    return;
                }
                melon = load(f, MELON);
                orange = load(f, ORANGE);
                film = load(f, FILM);
                goldenBanana = load(f, GOLDEN_BANANA);
                int found = count(melon, orange, film, goldenBanana);
                for (int k = 0; k < 5; k++) {
                    banana[k] = load(f, BANANA[k]);
                    weapon[k] = load(f, WEAPON[k]);
                    instrument[k] = load(f, INSTRUMENT[k]);
                    found += count(banana[k], weapon[k], instrument[k]);
                }
                Log.i(TAG, "Bottom screen: " + found + " of 19 icons loaded from the ROM");
            } catch (IOException | RuntimeException e) {
                Log.w(TAG, "Bottom screen: couldn't read icons from the ROM", e);
            }
            if (onLoaded != null) {
                onLoaded.run();
            }
        }, "DK64RomIcons").start();
    }

    /**
     * Draws {@code bitmap} fitted into a square of radius {@code r} around (cx, cy). Returns
     * false (drawing nothing) if it's null, so the caller can fall back to its own icon.
     */
    static boolean draw(Canvas c, Bitmap bitmap, float cx, float cy, float r) {
        if (bitmap == null) {
            return false;
        }
        float scale = 2 * r / Math.max(bitmap.getWidth(), bitmap.getHeight());
        float hw = bitmap.getWidth() * scale / 2, hh = bitmap.getHeight() * scale / 2;
        DST.set(cx - hw, cy - hh, cx + hw, cy + hh);
        c.drawBitmap(bitmap, null, DST, PAINT);
        return true;
    }

    static Bitmap of(Bitmap[] perKong, int kong) {
        return kong >= 0 && kong < perKong.length ? perKong[kong] : null;
    }

    private static int count(Bitmap... bitmaps) {
        int n = 0;
        for (Bitmap b : bitmaps) {
            if (b != null) {
                n++;
            }
        }
        return n;
    }

    private static boolean isUsRom(RandomAccessFile f) throws IOException {
        byte[] id = new byte[4];
        f.seek(0x3B);
        f.readFully(id);
        return id[0] == 'N' && id[1] == 'D' && id[2] == 'O' && id[3] == 'E';
    }

    private static int readU32(RandomAccessFile f, long at) throws IOException {
        f.seek(at);
        return f.readInt();
    }

    /** Returns the file's raw bytes from the ROM (still compressed if the table is). */
    private static byte[] readFile(RandomAccessFile f, int table, int file, int depth) throws IOException {
        if (table < 0 || table >= TABLE_COUNT || depth > 4) {
            return null;
        }
        long head = POINTER_TABLE + (readU32(f, POINTER_TABLE + table * 4L) & 0xFFFFFFFFL);
        int count = readU32(f, POINTER_TABLE + (TABLE_COUNT + table) * 4L);
        if (file < 0 || file >= count) {
            return null;
        }
        int entry = readU32(f, head + file * 4L);
        long start = entry & 0x7FFFFFFFL;
        if ((entry & 0x80000000) != 0) {
            // Redirect: the 8-byte header at the target names the real file.
            f.seek(POINTER_TABLE + start);
            int redirect = f.readUnsignedShort();
            return readFile(f, table, redirect, depth + 1);
        }
        long end = readU32(f, head + (file + 1) * 4L) & 0x7FFFFFFFL;
        long size = end - start;
        if (size <= 0 || size > 1 << 20 || POINTER_TABLE + end > f.length()) {
            return null;
        }
        byte[] data = new byte[(int) size];
        f.seek(POINTER_TABLE + start);
        f.readFully(data);
        return data;
    }

    private static Bitmap load(RandomAccessFile f, Spec spec) {
        try {
            byte[] data = readFile(f, spec.table, spec.file, 0);
            if (data == null) {
                return null;
            }
            if (spec.table != TEXTURES_UNCOMPRESSED) {
                data = gunzip(data);
            }
            return decode(data, spec);
        } catch (IOException | RuntimeException e) {
            return null;
        }
    }

    private static byte[] gunzip(byte[] data) throws IOException {
        try (GZIPInputStream in = new GZIPInputStream(new ByteArrayInputStream(data))) {
            ByteArrayOutputStream out = new ByteArrayOutputStream(data.length * 4);
            byte[] buffer = new byte[8192];
            int n;
            while ((n = in.read(buffer)) > 0) {
                out.write(buffer, 0, n);
            }
            return out.toByteArray();
        }
    }

    /** RGBA5551 to ARGB. Rejects data that's too short or has no visible pixels. */
    private static Bitmap decode(byte[] data, Spec spec) {
        int w = spec.width, h = spec.height;
        if (data.length < w * h * 2) {
            return null;
        }
        int[] argb = new int[w * h];
        int opaque = 0;
        for (int y = 0; y < h; y++) {
            int row = spec.flipped ? h - 1 - y : y;
            for (int x = 0; x < w; x++) {
                int i = (row * w + x) * 2;
                int p = ((data[i] & 0xFF) << 8) | (data[i + 1] & 0xFF);
                if ((p & 1) == 0) {
                    continue;
                }
                int r = (p >> 11) & 0x1F, g = (p >> 6) & 0x1F, b = (p >> 1) & 0x1F;
                argb[y * w + x] = 0xFF000000 | (r << 19 | (r >> 2) << 16) | (g << 11 | (g >> 2) << 8) | (b << 3 | b >> 2);
                opaque++;
            }
        }
        // A real icon has a solid middle and some transparent surroundings.
        if (opaque < w * h / 8 || opaque == w * h) {
            return null;
        }
        return Bitmap.createBitmap(argb, w, h, Bitmap.Config.ARGB_8888);
    }
}
