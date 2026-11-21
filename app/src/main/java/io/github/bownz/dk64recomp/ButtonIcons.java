package io.github.bownz.dk64recomp;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;

import java.io.InputStream;
import java.util.HashMap;

/**
 * The controller button pictures the in-game prompts use (assets/button_icons/<name>.rgba,
 * 64 x 64 RGBA from android/tools/gen_button_icons.py; recomp_fill_button_icon draws the same
 * ones into the game's dialogs), for the Android views. Names come from GameState.buttonIcons.
 */
final class ButtonIcons {
    private static final int SIZE = 64;
    private static final HashMap<String, Bitmap> cache = new HashMap<>();
    private static final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private static final RectF dst = new RectF();

    private ButtonIcons() {}

    /** The icon, or null when there's none of that name. */
    static synchronized Bitmap get(Context context, String name) {
        if (name == null || name.isEmpty()) {
            return null;
        }
        if (cache.containsKey(name)) {
            return cache.get(name);
        }
        Bitmap bmp = null;
        try (InputStream in = context.getAssets().open("game/assets/button_icons/" + name + ".rgba")) {
            byte[] raw = new byte[SIZE * SIZE * 4];
            int off = 0;
            while (off < raw.length) {
                int n = in.read(raw, off, raw.length - off);
                if (n < 0) {
                    break;
                }
                off += n;
            }
            if (off == raw.length) {
                int[] px = new int[SIZE * SIZE];
                for (int i = 0; i < px.length; i++) {
                    int r = raw[i * 4] & 0xFF, g = raw[i * 4 + 1] & 0xFF, b = raw[i * 4 + 2] & 0xFF, a = raw[i * 4 + 3] & 0xFF;
                    px[i] = (a << 24) | (r << 16) | (g << 8) | b;
                }
                bmp = Bitmap.createBitmap(px, SIZE, SIZE, Bitmap.Config.ARGB_8888);
            }
        } catch (Exception e) {
            bmp = null;
        }
        cache.put(name, bmp);
        return bmp;
    }

    /** Draws the icon centred on (cx, cy), {@code size} across. False when there's no icon. */
    static boolean draw(Canvas c, Context context, String name, float cx, float cy, float size) {
        return draw(c, context, name, cx, cy, size, 255);
    }

    /** As above, at {@code alpha} (0-255). */
    static boolean draw(Canvas c, Context context, String name, float cx, float cy, float size, int alpha) {
        Bitmap bmp = get(context, name);
        if (bmp == null) {
            return false;
        }
        dst.set(cx - size / 2f, cy - size / 2f, cx + size / 2f, cy + size / 2f);
        paint.setAlpha(alpha);
        c.drawBitmap(bmp, null, dst, paint);
        paint.setAlpha(255);
        return true;
    }
}
