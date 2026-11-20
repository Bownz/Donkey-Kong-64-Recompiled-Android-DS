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
 * sound_options_patches.c). The buttons are your controller's glyphs (PromptFont, as in the
 * controls menu) for whatever presses N64 B (exit) and N64 R (shoot, with the gun out), so they
 * follow your controller and mapping.
 *
 * Placement follows the game's: its HUD is 320 x 240 units filling the screen's height, the
 * prompts right-aligned to the screen's right edge, the text starting at TEXT_X on rows
 * ROW_SHOOT / ROW_EXIT (keep in step with FP_TEXT_X / FP_ROW_* in the patch).
 */
final class FirstPersonPromptView extends View {
    private static final float TEXT_X = 252f, ROW_SHOOT = 200f, ROW_EXIT = 214f;
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
                glyphs = GameState.buttonGlyphs();
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
        float s = getHeight() / 240f;   // pixels per game unit
        drawGlyph(canvas, s, ROW_EXIT, glyphs[0], "B");
        if ((status & 2) != 0) {
            drawGlyph(canvas, s, ROW_SHOOT, glyphs[1], "R");
        }
    }

    // The glyph ending just left of the text, centred on its row.
    private void drawGlyph(Canvas canvas, float s, float row, String glyph, String fallback) {
        boolean useGlyph = fontOk && !glyph.isEmpty();
        String text = useGlyph ? glyph : fallback;
        glyphPaint.setTypeface(useGlyph ? promptFont : Typeface.DEFAULT_BOLD);
        glyphPaint.setTextSize(ROW_HEIGHT * 1.5f * s);
        float textLeft = getWidth() - ((320f - TEXT_X) * s);
        float w = glyphPaint.measureText(text);
        float x = textLeft - (3f * s) - w;
        float mid = (row + (ROW_HEIGHT / 2f)) * s;
        Paint.FontMetrics fm = glyphPaint.getFontMetrics();
        canvas.drawText(text, x, mid - ((fm.ascent + fm.descent) / 2f), glyphPaint);
    }
}
