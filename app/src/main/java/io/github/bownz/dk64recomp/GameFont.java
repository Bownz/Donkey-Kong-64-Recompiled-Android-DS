package io.github.bownz.dk64recomp;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Log;

import java.io.IOException;
import java.io.RandomAccessFile;

/**
 * The game's own pause-screen lettering (text style 1, printStyledText in code_100180.c) for
 * the bottom screen. Nothing ships in the APK: the glyph sheets are decoded from the player's
 * ROM and the metrics (which sheet, where, how wide) are read from the running game's data.
 * Characters the font doesn't have are drawn with the fallback paint, and until the font is
 * ready everything is.
 *
 * Style 1's glyph sheets are 76x24 RGBA5551 HUD textures (table 14, from the style's first
 * texture); each glyph record is { s16 x_start, u8 width, u8 sheet (1-based) }.
 */
final class GameFont {
    private static final String TAG = "DK64Recomp";
    private static final int SHEET_W = 76, SHEET_H = 24, GLYPHS = 96, MAP_LENGTH = 57;

    private static volatile boolean started;
    private static volatile boolean ready;
    private static Bitmap[] sheets;
    private static final int[] glyphOfChar = new int[128];
    private static final Rect[] glyphRect = new Rect[GLYPHS];
    private static final int[] glyphSheet = new int[GLYPHS];
    private static int spaceWidth, kerning;

    private static final Paint PAINT = new Paint(Paint.FILTER_BITMAP_FLAG);
    private static final RectF DST = new RectF();
    private static final ColorMatrix MATRIX = new ColorMatrix();
    private static float gain = 1f;
    private static final java.util.HashMap<Long, ColorMatrixColorFilter> FILTERS = new java.util.HashMap<>();

    private GameFont() {}

    static boolean ready() {
        return ready;
    }

    /**
     * Once the game is running (its metrics are readable) and the ROM is in the data folder,
     * decodes the font in the background and calls {@code onLoaded}. Safe to call every frame.
     */
    static void ensureLoaded(Runnable onLoaded) {
        if (started || RomIcons.dataDirectory == null) {
            return;
        }
        byte[] m = GameState.fontMetrics();
        if (m == null) {
            return;
        }
        started = true;
        new Thread(() -> {
            if (load(m)) {
                ready = true;
                if (onLoaded != null) {
                    onLoaded.run();
                }
            }
        }, "DK64GameFont").start();
    }

    private static boolean load(byte[] m) {
        int first = m[0] & 0xFF, count = m[1] & 0xFF;
        spaceWidth = m[3] & 0xFF;
        kerning = m[4];
        java.util.Arrays.fill(glyphOfChar, -1);
        int base = 6 + MAP_LENGTH;
        int minSheet = Integer.MAX_VALUE, maxSheet = 0, mapped = 0;
        for (int i = 0; i < MAP_LENGTH; i++) {
            int ch = m[6 + i] & 0xFF;
            if (ch == 0) {
                break;
            }
            if (ch >= 128 || (ch >= 'a' && ch <= 'z') || glyphOfChar[ch] >= 0) {
                continue;   // lowercase letters are icons in the game's fonts
            }
            int g = base + i * 4;
            int x = (short) (((m[g] & 0xFF) << 8) | (m[g + 1] & 0xFF));
            int w = m[g + 2] & 0xFF, sheet = m[g + 3] & 0xFF;
            if (w == 0 || x < 0 || x + w > SHEET_W) {
                continue;
            }
            glyphOfChar[ch] = i;
            glyphRect[i] = new Rect(x, 0, x + w, SHEET_H);
            glyphSheet[i] = sheet;
            minSheet = Math.min(minSheet, sheet);
            maxSheet = Math.max(maxSheet, sheet);
            mapped++;
        }
        // Sheets are 1-based in the game (texture = first + sheet - 1); allow 0-based too.
        int offset = minSheet == 0 ? 0 : -1;
        Log.i(TAG, "Game font: style 1 first texture " + first + ", " + count + " sheets, " + mapped
            + " glyphs, sheet numbers " + minSheet + "-" + maxSheet + ", space " + spaceWidth + ", kerning " + kerning);
        if (mapped < 20 || count == 0 || count > 40 || maxSheet + offset >= count || first == 0) {
            Log.w(TAG, "Game font: metrics don't look right, keeping the built-in font");
            return false;
        }
        Bitmap[] loaded = new Bitmap[count];
        float maxLum = 0;
        try (RandomAccessFile f = RomIcons.openUsRom()) {
            if (f == null) {
                return false;
            }
            for (int s = 0; s < count; s++) {
                int[] argb = RomIcons.hudTexture(f, first + s, SHEET_W, SHEET_H);
                if (argb == null) {
                    continue;
                }
                for (int p : argb) {
                    if ((p >>> 24) > 128) {
                        float l = 0.3f * ((p >> 16) & 0xFF) + 0.59f * ((p >> 8) & 0xFF) + 0.11f * (p & 0xFF);
                        maxLum = Math.max(maxLum, l);
                    }
                }
                loaded[s] = Bitmap.createBitmap(argb, SHEET_W, SHEET_H, Bitmap.Config.ARGB_8888);
            }
        } catch (IOException | RuntimeException e) {
            Log.w(TAG, "Game font: couldn't read the glyphs", e);
            return false;
        }
        int missing = 0;
        for (int ch = 0; ch < 128; ch++) {
            int i = glyphOfChar[ch];
            if (i >= 0) {
                int s = glyphSheet[i] + offset;
                if (s < 0 || s >= count || loaded[s] == null) {
                    glyphOfChar[ch] = -1;
                    missing++;
                } else {
                    glyphSheet[i] = s;
                }
            }
        }
        if (mapped - missing < 20) {
            Log.w(TAG, "Game font: glyph sheets missing, keeping the built-in font");
            return false;
        }
        gain = maxLum > 1 ? 255f / maxLum : 1f;
        sheets = loaded;
        Log.i(TAG, "Game font: ready (" + (mapped - missing) + " glyphs)");
        return true;
    }

    private static int glyph(char ch) {
        ch = Character.toUpperCase(ch);
        return ch < 128 ? glyphOfChar[ch] : -1;
    }

    /** Pixels per sheet pixel for text of {@code size} (a Paint text size). */
    private static float scale(float size) {
        return size * 0.95f / SHEET_H;
    }

    /** Width of {@code s} at {@code size}; characters the font lacks are measured with {@code fallback}. */
    static float measure(String s, float size, Paint fallback) {
        float k = scale(size), w = 0;
        for (int i = 0; i < s.length(); i++) {
            char ch = s.charAt(i);
            int g = glyph(ch);
            if (ch == ' ') {
                w += spaceWidth * k;
            } else if (g >= 0) {
                w += glyphRect[g].width() * k;
            } else {
                w += fallback.measureText(s, i, i + 1);
            }
            w += kerning * k;
        }
        return Math.max(0, w - kerning * k);
    }

    /**
     * Draws {@code s} like Paint.drawText with {@code fallback}'s size and alignment: {@code y}
     * is the baseline. The glyphs are tinted to {@code fill} (keeping their shading), with an
     * {@code outline} edge.
     */
    static void draw(Canvas c, String s, float x, float y, Paint fallback, int fill, int outline) {
        float size = fallback.getTextSize();
        float k = scale(size);
        float width = measure(s, size, fallback);
        Paint.Align align = fallback.getTextAlign();
        float left = align == Paint.Align.CENTER ? x - width / 2 : align == Paint.Align.RIGHT ? x - width : x;
        // Callers centre text with a baseline at centre + 0.36 size; centre the glyphs there.
        float top = y - size * 0.36f - SHEET_H * k / 2;
        float edge = Math.max(1f, size * 0.05f);
        for (int pass = 0; pass < 2; pass++) {
            PAINT.setColorFilter(pass == 0 ? tint(outline, true) : tint(fill, false));
            float pen = left;
            for (int i = 0; i < s.length(); i++) {
                char ch = s.charAt(i);
                int g = glyph(ch);
                if (ch == ' ') {
                    pen += spaceWidth * k;
                } else if (g >= 0) {
                    Rect src = glyphRect[g];
                    float gw = src.width() * k;
                    if (pass == 0) {
                        for (int d = 0; d < 4; d++) {
                            float dx = (d == 0 ? -edge : d == 1 ? edge : 0), dy = (d == 2 ? -edge : d == 3 ? edge : 0);
                            DST.set(pen + dx, top + dy, pen + gw + dx, top + SHEET_H * k + dy);
                            c.drawBitmap(sheets[glyphSheet[g]], src, DST, PAINT);
                        }
                    } else {
                        DST.set(pen, top, pen + gw, top + SHEET_H * k);
                        c.drawBitmap(sheets[glyphSheet[g]], src, DST, PAINT);
                    }
                    pen += gw;
                } else {
                    float cw = fallback.measureText(s, i, i + 1);
                    if (pass == 1) {
                        Paint.Align was = fallback.getTextAlign();
                        fallback.setTextAlign(Paint.Align.LEFT);
                        fallback.setStyle(Paint.Style.STROKE);
                        fallback.setStrokeWidth(size * 0.16f);
                        fallback.setColor(outline);
                        c.drawText(s, i, i + 1, pen, y, fallback);
                        fallback.setStyle(Paint.Style.FILL);
                        fallback.setColor(fill);
                        c.drawText(s, i, i + 1, pen, y, fallback);
                        fallback.setTextAlign(was);
                    }
                    pen += cw;
                }
                pen += kerning * k;
            }
        }
        PAINT.setColorFilter(null);
    }

    /** Each pixel's brightness times {@code rgb} (or flat {@code rgb} for the outline pass). */
    private static ColorMatrixColorFilter tint(int rgb, boolean flat) {
        long key = (rgb & 0xFFFFFFFFL) | (flat ? 1L << 32 : 0);
        ColorMatrixColorFilter cached = FILTERS.get(key);
        if (cached != null) {
            return cached;
        }
        float r = ((rgb >> 16) & 0xFF) / 255f, g = ((rgb >> 8) & 0xFF) / 255f, b = (rgb & 0xFF) / 255f;
        if (flat) {
            MATRIX.set(new float[] {
                0, 0, 0, 0, r * 255,
                0, 0, 0, 0, g * 255,
                0, 0, 0, 0, b * 255,
                0, 0, 0, 1, 0,
            });
        } else {
            float lr = 0.3f * gain, lg = 0.59f * gain, lb = 0.11f * gain;
            MATRIX.set(new float[] {
                r * lr, r * lg, r * lb, 0, 0,
                g * lr, g * lg, g * lb, 0, 0,
                b * lr, b * lg, b * lb, 0, 0,
                0, 0, 0, 1, 0,
            });
        }
        ColorMatrixColorFilter filter = new ColorMatrixColorFilter(MATRIX);
        FILTERS.put(key, filter);
        return filter;
    }
}
