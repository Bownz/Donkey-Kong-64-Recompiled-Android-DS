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
    private static final int TEXTURES_HUD = 14;
    private static final int TEXTURES_GEOMETRY = 25;

    private static final int RGBA5551 = 0, RGBA32 = 1, IA8 = 2;

    /** Where each icon lives: table, file, width, height, stored upside down. */
    private static final class Spec {
        final int table, file, width, height;
        final boolean flipped;
        final int format;       // RGBA5551, RGBA32 or IA8

        Spec(int table, int file, int width, int height, boolean flipped) {
            this(table, file, width, height, flipped, RGBA5551);
        }

        Spec(int table, int file, int width, int height, boolean flipped, int format) {
            this.table = table;
            this.file = file;
            this.width = width;
            this.height = height;
            this.flipped = flipped;
            this.format = format;
        }
    }

    private static final Spec MELON = new Spec(TEXTURES_UNCOMPRESSED, 544, 48, 42, true);
    private static final Spec ORANGE = new Spec(TEXTURES_UNCOMPRESSED, 0x136, 32, 32, true);
    private static final Spec FILM = new Spec(TEXTURES_UNCOMPRESSED, 439, 48, 42, true);
    private static final Spec GOLDEN_BANANA = new Spec(TEXTURES_GEOMETRY, 5468, 44, 44, true);
    private static final Spec MEDAL = new Spec(TEXTURES_GEOMETRY, 0x156C, 44, 44, true);
    private static final Spec BLUEPRINT = new Spec(TEXTURES_GEOMETRY, 0x15FC, 48, 42, true);
    private static final Spec CROWN = new Spec(TEXTURES_GEOMETRY, 5893, 44, 44, true);
    private static final Spec KEY = new Spec(TEXTURES_GEOMETRY, 0x16F5, 44, 44, true);
    private static final Spec RAINBOW_COIN = new Spec(TEXTURES_GEOMETRY, 5963, 48, 42, true);
    private static final Spec FAIRY = new Spec(TEXTURES_GEOMETRY, 5869, 32, 32, true, RGBA32);
    // The first-person / gun crosshair from the HUD.
    private static final Spec CROSSHAIR = new Spec(TEXTURES_HUD, 0x38, 64, 64, false, IA8);
    // Banana Coins, face-on frame of each Kong's spinning coin: DK, Diddy, Lanky, Tiny, Chunky.
    private static final Spec[] COIN = {
        new Spec(TEXTURES_UNCOMPRESSED, 0x112, 44, 44, true),
        new Spec(TEXTURES_UNCOMPRESSED, 0x356, 44, 44, true),
        new Spec(TEXTURES_UNCOMPRESSED, 0x332, 44, 44, true),
        new Spec(TEXTURES_UNCOMPRESSED, 0x34A, 44, 44, true),
        new Spec(TEXTURES_UNCOMPRESSED, 0x33E, 44, 44, true),
    };
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

    // Kong faces: each is two 32x64 halves, {left, right}, for DK, Diddy, Lanky, Tiny, Chunky.
    private static final int[][] FACE_HALVES = {
        { 0x27C, 0x27B }, { 0x279, 0x27A }, { 0x277, 0x278 }, { 0x276, 0x275 }, { 0x273, 0x274 },
    };

    static volatile Bitmap melon, orange, film, goldenBanana;
    static volatile Bitmap medal, blueprint, crown, key, rainbowCoin, fairy, crosshair;
    static final Bitmap[] coin = new Bitmap[5];
    // The game's medal is gold for everyone; this is it recoloured to each Kong's colour.
    static final Bitmap[] medalByKong = new Bitmap[5];
    private static final int[] KONG_HUES = { -1, 0xD9383A, 0x3C78D8, 0x9B59D0, 0x4CAF50 };
    // The HUD / pause-screen health melon by quarters left: [0] = 1 quarter ... [3] = full.
    // The game draws the full one faintly behind the current one (code_10D2D0.c).
    static final Bitmap[] healthMelon = new Bitmap[4];
    static final Bitmap[] face = new Bitmap[5];
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
                medal = load(f, MEDAL);
                blueprint = load(f, BLUEPRINT);
                crown = load(f, CROWN);
                key = load(f, KEY);
                rainbowCoin = load(f, RAINBOW_COIN);
                fairy = load(f, FAIRY);
                crosshair = load(f, CROSSHAIR);
                int[] medalPixels = pixels(f, MEDAL);
                int found = count(melon, orange, film, goldenBanana, medal, blueprint, crown, key, rainbowCoin, fairy, crosshair);
                for (int k = 0; k < 5; k++) {
                    coin[k] = load(f, COIN[k]);
                    if (medalPixels != null) {
                        medalByKong[k] = Bitmap.createBitmap(KONG_HUES[k] < 0 ? medalPixels : recolor(medalPixels, KONG_HUES[k]),
                            MEDAL.width, MEDAL.height, Bitmap.Config.ARGB_8888);
                    }
                    found += count(coin[k]);
                }
                for (int q = 0; q < 4; q++) {
                    // Unlike the sprites, these HUD textures are stored the right way up.
                    healthMelon[q] = load(f, new Spec(TEXTURES_HUD, 0x5A + q, 48, 42, false));
                    found += count(healthMelon[q]);
                }
                for (int k = 0; k < 5; k++) {
                    banana[k] = load(f, BANANA[k]);
                    weapon[k] = load(f, WEAPON[k]);
                    instrument[k] = load(f, INSTRUMENT[k]);
                    face[k] = loadFace(f, FACE_HALVES[k]);
                    found += count(banana[k], weapon[k], instrument[k], face[k]);
                }
                Log.i(TAG, "Bottom screen: " + found + " of 40 icons loaded from the ROM");
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
        int[] argb = pixels(f, spec);
        return argb == null ? null : Bitmap.createBitmap(argb, spec.width, spec.height, Bitmap.Config.ARGB_8888);
    }

    /** A Kong's face: two 32x64 halves side by side. */
    private static Bitmap loadFace(RandomAccessFile f, int[] halves) {
        int[] left = pixels(f, new Spec(TEXTURES_GEOMETRY, halves[0], 32, 64, true));
        int[] right = pixels(f, new Spec(TEXTURES_GEOMETRY, halves[1], 32, 64, true));
        if (left == null || right == null) {
            return null;
        }
        int[] whole = new int[64 * 64];
        for (int y = 0; y < 64; y++) {
            System.arraycopy(left, y * 32, whole, y * 64, 32);
            System.arraycopy(right, y * 32, whole, y * 64 + 32, 32);
        }
        return Bitmap.createBitmap(whole, 64, 64, Bitmap.Config.ARGB_8888);
    }

    private static int[] pixels(RandomAccessFile f, Spec spec) {
        try {
            byte[] data = readFile(f, spec.table, spec.file, 0);
            if (data == null) {
                return null;
            }
            if (data.length > 2 && (data[0] & 0xFF) == 0x1F && (data[1] & 0xFF) == 0x8B) {
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

    /**
     * Gold to another colour: keeps each pixel's brightness, takes the hue and saturation of
     * {@code rgb}, for the coloured (non-grey) pixels.
     */
    private static int[] recolor(int[] argb, int rgb) {
        int[] out = argb.clone();
        float tr = ((rgb >> 16) & 0xFF) / 255f, tg = ((rgb >> 8) & 0xFF) / 255f, tb = (rgb & 0xFF) / 255f;
        float tl = 0.3f * tr + 0.59f * tg + 0.11f * tb;
        for (int i = 0; i < out.length; i++) {
            int p = out[i];
            if ((p >>> 24) == 0) {
                continue;
            }
            int r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
            int max = Math.max(r, Math.max(g, b)), min = Math.min(r, Math.min(g, b));
            if (max - min < 40) {
                continue; // greys and whites stay as they are
            }
            float l = (0.3f * r + 0.59f * g + 0.11f * b) / 255f;
            float scale = l / Math.max(tl, 0.01f);
            int nr = Math.min(255, Math.round(tr * scale * 255)), ng = Math.min(255, Math.round(tg * scale * 255)), nb = Math.min(255, Math.round(tb * scale * 255));
            out[i] = (p & 0xFF000000) | nr << 16 | ng << 8 | nb;
        }
        return out;
    }

    /** RGBA5551, RGBA8888 or IA8 to ARGB. Rejects data that's too short or has no visible pixels. */
    private static int[] decode(byte[] data, Spec spec) {
        int w = spec.width, h = spec.height;
        int bpp = spec.format == RGBA32 ? 4 : spec.format == IA8 ? 1 : 2;
        if (data.length < w * h * bpp) {
            return null;
        }
        int[] argb = new int[w * h];
        int opaque = 0;
        for (int y = 0; y < h; y++) {
            int row = spec.flipped ? h - 1 - y : y;
            for (int x = 0; x < w; x++) {
                if (spec.format == IA8) {
                    int v = data[row * w + x] & 0xFF;
                    int a = (v & 0xF) * 17, in = (v >> 4) * 17;
                    if (a < 16) {
                        continue;
                    }
                    argb[y * w + x] = a << 24 | in << 16 | in << 8 | in;
                    opaque++;
                    continue;
                }
                if (spec.format == RGBA32) {
                    int i = (row * w + x) * 4;
                    int a = data[i + 3] & 0xFF;
                    if (a < 16) {
                        continue;
                    }
                    argb[y * w + x] = a << 24 | (data[i] & 0xFF) << 16 | (data[i + 1] & 0xFF) << 8 | (data[i + 2] & 0xFF);
                    opaque++;
                    continue;
                }
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
        // A real icon has a solid middle (a face half may be fully opaque).
        if (opaque < w * h / (spec.format == IA8 ? 60 : 8)) {
            return null;
        }
        return argb;
    }
}
