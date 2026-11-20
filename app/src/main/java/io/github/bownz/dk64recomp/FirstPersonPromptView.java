package io.github.bownz.dk64recomp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

/**
 * Walk in First Person (Input): button prompts in the bottom right of the game screen while in
 * first person: "Exit" with the controller button that presses N64 B, and "Shoot" above it (the
 * button that presses N64 R) while the gun is out. The buttons are drawn as the controller's own
 * glyphs (PromptFont, as in the controls menu), so they follow your controller and mapping.
 */
final class FirstPersonPromptView extends View {
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final GameState state = TouchMoves.get().state;
    private final Paint glyphPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint labelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint pillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF pill = new RectF();
    private final float density;

    private boolean shown, gun;
    private String[] glyphs = { "", "" };
    private long glyphsAt;

    FirstPersonPromptView(Context context) {
        super(context);
        density = context.getResources().getDisplayMetrics().density;
        Typeface prompt = null;
        try {
            prompt = Typeface.createFromAsset(context.getAssets(), "game/assets/promptfont/promptfont.ttf");
        } catch (RuntimeException e) {
            // Falls back to the button's letter in the default font.
        }
        glyphPaint.setTypeface(prompt != null ? prompt : Typeface.DEFAULT_BOLD);
        glyphPaint.setColor(0xFFFFFFFF);
        glyphPaint.setTextSize(22 * density);
        labelPaint.setTypeface(Typeface.DEFAULT_BOLD);
        labelPaint.setColor(0xFFFFFFFF);
        labelPaint.setTextSize(14 * density);
        pillPaint.setColor(0x99000000);
        setWillNotDraw(false);
        setFocusable(false);
        setClickable(false);
        handler.post(tick);
    }

    private final Runnable tick = new Runnable() {
        @Override
        public void run() {
            boolean nowShown = false, nowGun = false;
            if ((GameState.gameFeel() & GameState.FEEL_FP_WALK) != 0 && state.refresh() && state.inGame() && !state.paused) {
                int cs = state.controlState;
                nowShown = (cs >= 2 && cs <= 5) || cs == 0x62;
                nowGun = nowShown && (state.gunOut || cs == 0x62);
            }
            long now = android.os.SystemClock.uptimeMillis();
            if (nowShown && (now - glyphsAt > 2000)) {   // the mapping can change in the menu
                glyphs = GameState.buttonGlyphs();
                glyphsAt = now;
                invalidate();
            }
            if (nowShown != shown || nowGun != gun) {
                shown = nowShown;
                gun = nowGun;
                invalidate();
            }
            handler.postDelayed(this, 100);
        }
    };

    @Override
    protected void onDraw(Canvas canvas) {
        if (!shown) {
            return;
        }
        float margin = 16 * density;
        float rowH = 34 * density;
        float y = getHeight() - margin;
        drawRow(canvas, y, glyphs[0].isEmpty() ? "B" : glyphs[0], !glyphs[0].isEmpty(), "Exit");
        if (gun) {
            drawRow(canvas, y - rowH - (6 * density), glyphs[1].isEmpty() ? "R" : glyphs[1], !glyphs[1].isEmpty(), "Shoot");
        }
    }

    // One prompt, right-aligned, its bottom at y: [glyph] Label on a dark pill.
    private void drawRow(Canvas canvas, float bottom, String glyph, boolean isGlyph, String label) {
        Paint gp = isGlyph ? glyphPaint : labelPaint;
        float pad = 10 * density, gap = 8 * density, h = 34 * density;
        float gw = gp.measureText(glyph);
        float lw = labelPaint.measureText(label);
        float w = pad + gw + gap + lw + pad;
        float right = getWidth() - (16 * density);
        pill.set(right - w, bottom - h, right, bottom);
        canvas.drawRoundRect(pill, h / 2, h / 2, pillPaint);
        Paint.FontMetrics gm = gp.getFontMetrics();
        Paint.FontMetrics lm = labelPaint.getFontMetrics();
        float mid = bottom - (h / 2);
        canvas.drawText(glyph, pill.left + pad, mid - ((gm.ascent + gm.descent) / 2), gp);
        canvas.drawText(label, pill.left + pad + gw + gap, mid - ((lm.ascent + lm.descent) / 2), labelPaint);
    }
}
