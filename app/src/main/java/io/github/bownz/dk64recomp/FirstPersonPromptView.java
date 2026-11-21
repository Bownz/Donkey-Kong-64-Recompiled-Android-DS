package io.github.bownz.dk64recomp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

/**
 * Walk in First Person (Input): the controller's buttons beside the game's own "EXIT" and
 * "SHOOT" text in the bottom right (the game patch draws the text: fp_draw_prompts in
 * sound_options_patches.c). The buttons are your controller's button pictures (ButtonIcons, as
 * the in-game prompts draw them) for whatever presses N64 B (exit) and N64 R (shoot, with the
 * gun out), so they follow your controller and mapping.
 *
 * Placement follows the game's: its HUD is 320 x 240 units filling the screen's height, the
 * prompts right-aligned to the screen's right edge, the text starting at TEXT_X on rows
 * ROW_SHOOT / ROW_EXIT (keep in step with FP_TEXT_X / FP_ROW_* in the patch).
 */
final class FirstPersonPromptView extends View {
    private static final float TEXT_X = 252f, TEXT_X_PHOTO = 190f,   // (status & 4: the camera)
            ROW_SHOOT = 200f, ROW_EXIT = 214f;
    private static final float ROW_HEIGHT = 10f;   // the text's height, game units

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Paint glyphPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private boolean fontOk;
    private Typeface promptFont;

    private int status;
    private String[] glyphs = { "", "" };
    private long glyphsAt;

    FirstPersonPromptView(Context context) {
        super(context);
        Typeface prompt = null;
        try {
            prompt = Typeface.createFromAsset(context.getAssets(), "game/assets/promptfont/promptfont.ttf");
        } catch (RuntimeException e) {
            // No glyph font: the N64 button letters instead.
        }
        fontOk = prompt != null;
        promptFont = prompt;
        glyphPaint.setTypeface(fontOk ? prompt : Typeface.DEFAULT_BOLD);
        glyphPaint.setColor(0xFFFFFFFF);
        glyphPaint.setShadowLayer(3f, 0f, 1f, 0xFF000000);
        setWillNotDraw(false);
        setFocusable(false);
        setClickable(false);
        handler.post(tick);
    }

    private final Runnable tick = new Runnable() {
        @Override
        public void run() {
            int now = GameState.fpStatus();
            long t = android.os.SystemClock.uptimeMillis();
            if ((now & 1) != 0 && t - glyphsAt > 2000) {   // the mapping can change in the menu
                String[] icons = GameState.buttonIcons();
                glyphs = new String[] { icons[0], icons[1] };
                glyphsAt = t;
                invalidate();
            }
            if (now != status) {
                status = now;
                invalidate();
            }
            handler.postDelayed(this, 100);
        }
    };

    @Override
    protected void onDraw(Canvas canvas) {
        if ((status & 1) == 0) {
            return;
        }
        // Where the game's 320 x 240 HUD lands on this view: it fills the height and is
        // right-aligned on screens 4:3 to 16:9 wide; on narrower screens (4:3 or squarer, e.g.
        // the bottom screen) the picture fits the width and is centred vertically; wider than
        // 16:9 the HUD is clamped to a centred 16:9 area.
        float w = getWidth(), h = getHeight();
        float s;
        if (w / h < 4f / 3f) {
            s = w / 320f;
            hudTop = (h - 240f * s) / 2f;
            hudRight = w;
        } else {
            s = h / 240f;
            hudTop = 0f;
            float hudW = Math.min(w, h * 16f / 9f);
            hudRight = (w + hudW) / 2f;
        }
        drawGlyph(canvas, s, ROW_EXIT, glyphs[0], "B");
        if ((status & 2) != 0) {
            drawGlyph(canvas, s, ROW_SHOOT, glyphs[1], "R");
        }
    }

    private float hudTop, hudRight;

    // The glyph ending just left of the text, centred on its row.
    private void drawGlyph(Canvas canvas, float s, float row, String glyph, String fallback) {
        {
            float size = ROW_HEIGHT * 1.6f * s;
            float textLeft = hudRight - ((320f - ((status & 4) != 0 ? TEXT_X_PHOTO : TEXT_X)) * s);
            float mid = hudTop + (row + (ROW_HEIGHT / 2f)) * s;
            if (ButtonIcons.draw(canvas, getContext(), glyph, textLeft - (3f * s) - size / 2f, mid, size)) {
                return;
            }
        }
        boolean useGlyph = false;
        String text = useGlyph ? glyph : fallback;
        glyphPaint.setTypeface(useGlyph ? promptFont : Typeface.DEFAULT_BOLD);
        glyphPaint.setTextSize(ROW_HEIGHT * 1.5f * s);
        float textLeft = hudRight - ((320f - ((status & 4) != 0 ? TEXT_X_PHOTO : TEXT_X)) * s);
        float w = glyphPaint.measureText(text);
        float x = textLeft - (3f * s) - w;
        float mid = hudTop + (row + (ROW_HEIGHT / 2f)) * s;
        Paint.FontMetrics fm = glyphPaint.getFontMetrics();
        canvas.drawText(text, x, mid - ((fm.ascent + fm.descent) / 2f), glyphPaint);
    }
}
