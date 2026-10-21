package io.github.bownz.dk64recomp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.os.Handler;
import android.os.Looper;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;

/**
 * DK64 second screen (e.g. the AYN Thor's lower display), laid out in the spirit of Ocarina
 * of Time 3D's touch screen:
 *
 *  - top-left: first-person view toggle (C-Up);
 *  - top: watermelon health, read live from the game;
 *  - right column: one-tap Z + C moves with icons for the current Kong and ammo counts;
 *  - centre: Kong tabs and the selected Kong's collectibles.
 *
 * Buttons drive the shared virtual gamepad ({@link TouchInput}); game values come from
 * {@link GameState} (read-only).
 */
public final class BottomScreenView extends View {

    private static final String[] KONG_NAMES = { "Donkey Kong", "Diddy Kong", "Lanky Kong", "Tiny Kong", "Chunky Kong" };
    private static final String[] KONG_SHORT = { "DK", "Diddy", "Lanky", "Tiny", "Chunky" };
    private static final int[] KONG_COLORS = {
        Color.rgb(242, 194, 48), Color.rgb(217, 56, 58), Color.rgb(60, 120, 216),
        Color.rgb(155, 89, 208), Color.rgb(76, 175, 80),
    };

    private static final int GOLD = Color.rgb(201, 164, 76);
    private static final int PANEL = Color.argb(220, 42, 38, 32);
    private static final int TEXT = Color.rgb(240, 232, 214);
    private static final int TEXT_DIM = Color.argb(150, 240, 232, 214);

    // Input timing. DK64 reads input once per game frame (~33 ms at 30 fps), so each step
    // lasts a few frames: hold Z, press C while Z is still held, then let go.
    private static final long Z_LEAD_MS = 100;
    private static final long C_HOLD_MS = 150;
    private static final long Z_TAIL_MS = 60;
    private static final long POLL_MS = 250;

    private enum Move { WEAPON, ORANGE, INSTRUMENT, CAMERA, FIRST_PERSON }

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final GameState state = new GameState();
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);

    // Layout, in pixels (computed in onSizeChanged).
    private final RectF[] actionRects = { new RectF(), new RectF(), new RectF(), new RectF() };
    private final RectF[] tabRects = new RectF[GameState.KONG_COUNT];
    private final RectF panelRect = new RectF();
    private final RectF tmp = new RectF();
    private final android.graphics.Path facePath = new android.graphics.Path();
    private float fpX, fpY, fpR;
    private float healthX, healthY, melonR;
    private float unit;

    private Move running;
    private int selectedKong = -1;   // -1: follow the Kong being played
    private boolean active;

    private final Runnable poll = new Runnable() {
        @Override
        public void run() {
            if (state.refresh()) {
                invalidate();
            }
            RomIcons.ensureLoaded(BottomScreenView.this::postInvalidate);
            if (active) {
                handler.postDelayed(this, POLL_MS);
            }
        }
    };

    public BottomScreenView(Context context) {
        super(context);
        for (int i = 0; i < tabRects.length; i++) {
            tabRects[i] = new RectF();
        }
        text.setColor(TEXT);
        text.setFakeBoldText(true);
        setHapticFeedbackEnabled(true);
        setContentDescription("DK64 bottom screen: health, collectibles and action buttons");
    }

    // ------------------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------------------

    /** Takes or gives up the shared virtual gamepad, and starts or stops reading the game. */
    public void setActive(boolean active) {
        if (this.active == active) {
            return;
        }
        this.active = active;
        if (active) {
            TouchInput.acquire();
            handler.post(poll);
        } else {
            cancel();
            handler.removeCallbacks(poll);
            TouchInput.release();
        }
    }

    /** Stops any move in progress and lets go of every input (e.g. when the app pauses). */
    public void cancel() {
        handler.removeCallbacksAndMessages(null);
        if (running != null) {
            TouchInput.setAxis(TouchInput.AXIS_RIGHT_X, 0f);
            TouchInput.setAxis(TouchInput.AXIS_RIGHT_Y, 0f);
            TouchInput.setAxis(TouchInput.AXIS_TRIGGER_LEFT, 0f);
            running = null;
            invalidate();
        }
        if (active) {
            handler.post(poll);
        }
    }

    // ------------------------------------------------------------------------------
    // Moves
    // ------------------------------------------------------------------------------

    private void perform(Move move) {
        if (running != null) {
            return; // One move at a time; a sequence takes ~0.3 s.
        }
        running = move;
        TouchInput.ensure();
        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        invalidate();

        final int cAxis;
        final float cValue;
        switch (move) {
            case WEAPON:     cAxis = TouchInput.AXIS_RIGHT_X; cValue = -1f; break; // C-Left
            case ORANGE:     cAxis = TouchInput.AXIS_RIGHT_X; cValue = 1f;  break; // C-Right
            case INSTRUMENT: cAxis = TouchInput.AXIS_RIGHT_Y; cValue = -1f; break; // C-Up
            case CAMERA:     cAxis = TouchInput.AXIS_RIGHT_Y; cValue = 1f;  break; // C-Down
            default:         cAxis = TouchInput.AXIS_RIGHT_Y; cValue = -1f; break; // C-Up alone
        }

        if (move == Move.FIRST_PERSON) {
            // C-Up on its own enters first-person view, and pressing it again leaves it.
            TouchInput.setAxis(cAxis, cValue);
            handler.postDelayed(() -> {
                TouchInput.setAxis(cAxis, 0f);
                running = null;
                invalidate();
            }, C_HOLD_MS);
            return;
        }

        TouchInput.setAxis(TouchInput.AXIS_TRIGGER_LEFT, 1f);                              // Z
        handler.postDelayed(() -> TouchInput.setAxis(cAxis, cValue), Z_LEAD_MS);           // + C
        handler.postDelayed(() -> TouchInput.setAxis(cAxis, 0f), Z_LEAD_MS + C_HOLD_MS);
        handler.postDelayed(() -> {
            TouchInput.setAxis(TouchInput.AXIS_TRIGGER_LEFT, 0f);
            running = null;
            invalidate();
        }, Z_LEAD_MS + C_HOLD_MS + Z_TAIL_MS);
    }

    private int displayedKong() {
        if (selectedKong >= 0 && !isLocked(selectedKong)) {
            return selectedKong;
        }
        return state.inGame() ? state.currentKong : GameState.DK;
    }

    private int playingKong() {
        return state.inGame() ? state.currentKong : GameState.DK;
    }

    // ------------------------------------------------------------------------------
    // Layout
    // ------------------------------------------------------------------------------

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        unit = Math.min(w, h);
        float m = unit * 0.03f;

        // Right column: four action buttons.
        float colW = Math.min(w * 0.27f, h * 0.27f);
        float colLeft = w - m - colW;
        float gap = m * 0.7f;
        float cellH = (h - 2 * m - 3 * gap) / 4f;
        for (int i = 0; i < 4; i++) {
            float top = m + i * (cellH + gap);
            actionRects[i].set(colLeft, top, w - m, top + cellH);
        }

        // Left area.
        float left = m, right = colLeft - m;
        fpR = Math.min(unit * 0.085f, (right - left) * 0.12f);
        fpX = left + fpR;
        fpY = m + fpR;

        melonR = fpR * 0.7f;
        healthX = fpX + fpR + m * 1.5f + melonR;
        healthY = fpY;

        float tabsTop = m + 2 * fpR + m;
        float tabH = unit * 0.11f;
        float tabGap = m * 0.5f;
        float tabW = (right - left - tabGap * (GameState.KONG_COUNT - 1)) / GameState.KONG_COUNT;
        for (int i = 0; i < GameState.KONG_COUNT; i++) {
            float l = left + i * (tabW + tabGap);
            tabRects[i].set(l, tabsTop, l + tabW, tabsTop + tabH);
        }
        panelRect.set(left, tabsTop + tabH + m * 0.6f, right, h - m);
    }

    // ------------------------------------------------------------------------------
    // Drawing
    // ------------------------------------------------------------------------------

    @Override
    protected void onDraw(Canvas canvas) {
        int w = getWidth(), h = getHeight();
        paint.setStyle(Paint.Style.FILL);
        paint.setShader(new LinearGradient(0, 0, 0, h, Color.rgb(34, 40, 48), Color.rgb(16, 18, 24), Shader.TileMode.CLAMP));
        canvas.drawRect(0, 0, w, h, paint);
        paint.setShader(null);

        drawFirstPersonButton(canvas);
        drawHealth(canvas);
        drawTabs(canvas);
        drawKongPanel(canvas);
        drawActions(canvas);
    }

    private void goldRing(Canvas c, RectF r, float radius, boolean pressed) {
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * (pressed ? 0.012f : 0.007f));
        paint.setColor(pressed ? Color.WHITE : GOLD);
        c.drawRoundRect(r, radius, radius, paint);
        paint.setStyle(Paint.Style.FILL);
    }

    private void drawFirstPersonButton(Canvas c) {
        boolean pressed = running == Move.FIRST_PERSON;
        paint.setColor(pressed ? Color.rgb(70, 64, 50) : PANEL);
        c.drawCircle(fpX, fpY, fpR, paint);
        tmp.set(fpX - fpR, fpY - fpR, fpX + fpR, fpY + fpR);
        goldRing(c, tmp, fpR, pressed);
        DkIcons.eye(c, fpX, fpY - fpR * 0.15f, fpR * 0.55f);
        text.setTextAlign(Paint.Align.CENTER);
        text.setColor(TEXT_DIM);
        text.setTextSize(fitText("1st person", fpR * 1.25f, fpR * 0.24f));
        c.drawText("1st person", fpX, fpY + fpR * 0.55f, text);
    }

    private void drawHealth(Canvas c) {
        text.setTextAlign(Paint.Align.LEFT);
        if (!state.inGame()) {
            String hint = "Start a game to see health";
            float hintX = healthX - melonR;
            text.setColor(TEXT_DIM);
            text.setTextSize(fitText(hint, tabRects[GameState.KONG_COUNT - 1].right - hintX, melonR * 0.6f));
            c.drawText(hint, hintX, healthY + melonR * 0.2f, text);
            return;
        }
        float x = healthX;
        int remaining = state.health;
        for (int i = 0; i < state.melons; i++) {
            int quarters = Math.max(0, Math.min(4, remaining));
            drawMelon(c, x, healthY, melonR, quarters);
            remaining -= 4;
            x += melonR * 2.3f;
        }
        int kong = state.currentKong;
        float nameX = x - melonR * 0.6f;
        float nameW = tabRects[GameState.KONG_COUNT - 1].right - nameX;
        text.setTextAlign(Paint.Align.LEFT);
        text.setColor(KONG_COLORS[kong]);
        if (nameW > melonR) {
            text.setTextSize(fitText(KONG_NAMES[kong], nameW, melonR * 0.75f));
            c.drawText(KONG_NAMES[kong], nameX, healthY + melonR * 0.28f, text);
        }
    }

    /** The game's melon if the ROM gave us one, with lost quarters shaded; else ours. */
    private void drawMelon(Canvas c, float cx, float cy, float r, int quarters) {
        if (!RomIcons.draw(c, RomIcons.melon, cx, cy, r)) {
            DkIcons.melon(c, cx, cy, r, quarters);
            return;
        }
        if (quarters < 4) {
            float hh = r * 42f / 48f;
            tmp.set(cx - r, cy - hh, cx + r, cy + hh);
            paint.setColor(Color.argb(190, 16, 14, 12));
            // Quarters go clockwise from the top; the lost ones are the last ones.
            c.drawArc(tmp, -90 + quarters * 90, (4 - quarters) * 90, true, paint);
        }
    }

    /** Kongs not freed yet in the running game (unknown before a game starts: all shown). */
    private boolean isLocked(int k) {
        return state.inGame() && !state.isFreed(k);
    }

    private void drawTabs(Canvas c) {
        int shown = displayedKong();
        int playing = state.inGame() ? state.currentKong : -1;
        text.setTextAlign(Paint.Align.CENTER);
        for (int i = 0; i < tabRects.length; i++) {
            RectF r = tabRects[i];
            float radius = r.height() * 0.3f;
            if (isLocked(i)) {
                // Greyed out, no face or name, until the Kong is freed.
                paint.setColor(Color.rgb(40, 38, 36));
                c.drawRoundRect(r, radius, radius, paint);
                paint.setStyle(Paint.Style.STROKE);
                paint.setStrokeWidth(r.height() * 0.05f);
                paint.setColor(Color.rgb(80, 76, 70));
                c.drawRoundRect(r, radius, radius, paint);
                paint.setStyle(Paint.Style.FILL);
                continue;
            }
            paint.setColor(i == shown ? KONG_COLORS[i] : PANEL);
            c.drawRoundRect(r, radius, radius, paint);
            goldRing(c, r, radius, i == shown);
            float faceR = Math.min(r.height(), r.width()) * 0.4f;
            android.graphics.Bitmap face = RomIcons.of(RomIcons.face, i);
            if (face != null) {
                tmp.set(r.centerX() - faceR, r.centerY() - faceR, r.centerX() + faceR, r.centerY() + faceR);
                facePath.reset();
                facePath.addOval(tmp, android.graphics.Path.Direction.CW);
                int saved = c.save();
                c.clipPath(facePath);
                RomIcons.draw(c, face, r.centerX(), r.centerY(), faceR);
                c.restoreToCount(saved);
            } else {
                text.setTextSize(Math.min(r.height() * 0.38f, r.width() * 0.22f));
                text.setColor(i == shown ? Color.rgb(20, 16, 10) : KONG_COLORS[i]);
                c.drawText(KONG_SHORT[i], r.centerX(), r.centerY() + text.getTextSize() * 0.35f, text);
            }
            if (i == playing) {
                paint.setColor(Color.WHITE);
                c.drawCircle(r.right - radius * 0.7f, r.top + radius * 0.7f, radius * 0.22f, paint);
            }
        }
    }

    private void drawKongPanel(Canvas c) {
        RectF p = panelRect;
        float radius = unit * 0.03f;
        paint.setColor(PANEL);
        c.drawRoundRect(p, radius, radius, paint);
        goldRing(c, p, radius, false);

        int k = displayedKong();
        GameState.Kong kong = state.kongs[k];
        boolean known = state.inGame();
        float pad = p.width() * 0.05f;
        float rowH = p.height() / 7.2f;
        float x = p.left + pad;
        float y = p.top + rowH * 0.95f;

        float innerW = p.width() - 2 * pad;
        text.setTextAlign(Paint.Align.LEFT);
        text.setColor(KONG_COLORS[k]);
        text.setTextSize(fitText(KONG_NAMES[k], innerW, rowH * 0.55f));
        c.drawText(KONG_NAMES[k], x, y, text);
        y += rowH * 1.1f;

        statRow(c, x, y, p.right - pad, rowH, "Golden Bananas", known ? String.valueOf(kong.goldenBananas) : "–", Color.rgb(242, 194, 48), RomIcons.goldenBanana);
        y += rowH;
        statRow(c, x, y, p.right - pad, rowH, "Bananas", known ? String.valueOf(kong.colouredBananas) : "–", KONG_COLORS[k], RomIcons.of(RomIcons.banana, k));
        y += rowH;
        statRow(c, x, y, p.right - pad, rowH, "Coins", known ? String.valueOf(kong.coins) : "–", Color.rgb(230, 190, 80), null);
        y += rowH;

        y += rowH * 0.6f;

        // Shared items.
        text.setTextAlign(Paint.Align.LEFT);
        text.setColor(TEXT_DIM);
        String shared = known
            ? "Crystals " + state.crystals + "    Film " + state.film + "    Oranges " + state.oranges + "    Ammo " + state.ammo
            : "Shared items appear once a game is running";
        text.setTextSize(fitText(shared, innerW, rowH * 0.34f));
        c.drawText(shared, x, Math.min(y, p.bottom - rowH * 0.3f), text);
    }

    private void statRow(Canvas c, float x, float y, float right, float rowH, String label, String value, int dot, android.graphics.Bitmap icon) {
        if (!RomIcons.draw(c, icon, x + rowH * 0.26f, y - rowH * 0.17f, rowH * 0.3f)) {
            paint.setColor(dot);
            c.drawCircle(x + rowH * 0.26f, y - rowH * 0.17f, rowH * 0.16f, paint);
        }
        // Room for the label plus a gap and a 4-digit value.
        float labelX = x + rowH * 0.7f;
        text.setTextSize(rowH * 0.42f);
        float valueW = text.measureText("0000");
        text.setTextSize(fitText(label, right - labelX - valueW - rowH * 0.3f, rowH * 0.42f));
        text.setTextAlign(Paint.Align.LEFT);
        text.setColor(TEXT);
        c.drawText(label, labelX, y, text);
        text.setTextAlign(Paint.Align.RIGHT);
        c.drawText(value, right, y, text);
    }

    /**
     * Whether a move can be used right now: the Kong has bought it and there's something to
     * use. Unknown (no game running) counts as available so the buttons still work.
     */
    private boolean available(Move move) {
        if (!state.inGame()) {
            return true;
        }
        GameState.Kong k = state.kongs[state.currentKong];
        switch (move) {
            case WEAPON: return k.weapon > 0 && (state.ammo > 0 || state.homingAmmo > 0);
            case ORANGE: return state.oranges > 0;
            case INSTRUMENT: return k.instrument > 0 && k.instrumentEnergy > 0;
            case CAMERA: return state.film > 0;
            default: return true;
        }
    }

    private void drawActions(Canvas c) {
        int kong = playingKong();
        boolean known = state.inGame();
        GameState.Kong k = state.kongs[kong];
        Move[] moves = { Move.WEAPON, Move.ORANGE, Move.INSTRUMENT, Move.CAMERA };
        String[] counts = known
            ? new String[] { String.valueOf(state.ammo), String.valueOf(state.oranges), String.valueOf(k.instrumentEnergy), String.valueOf(state.film) }
            : new String[] { "", "", "", "" };

        for (int i = 0; i < 4; i++) {
            RectF r = actionRects[i];
            boolean pressed = running == moves[i];
            boolean enabled = available(moves[i]);
            float radius = r.height() * 0.18f;

            int saved = c.saveLayerAlpha(r.left - unit * 0.02f, r.top - unit * 0.02f,
                r.right + unit * 0.02f, r.bottom + unit * 0.02f, enabled ? 255 : 70);
            paint.setColor(pressed ? Color.rgb(78, 70, 52) : PANEL);
            c.drawRoundRect(r, radius, radius, paint);
            goldRing(c, r, radius, pressed);

            // Icon, with the count beside it.
            boolean hasCount = !counts[i].isEmpty();
            float iconR = Math.min(r.height() * 0.36f, r.width() * (hasCount ? 0.22f : 0.3f));
            float iconX = hasCount ? r.left + r.width() * 0.1f + iconR : r.centerX();
            float iconY = r.centerY();
            switch (moves[i]) {
                case WEAPON:
                    if (!RomIcons.draw(c, RomIcons.of(RomIcons.weapon, kong), iconX, iconY, iconR)) {
                        DkIcons.weapon(c, kong, iconX, iconY, iconR);
                    }
                    break;
                case ORANGE:
                    if (!RomIcons.draw(c, RomIcons.orange, iconX, iconY, iconR)) {
                        DkIcons.orange(c, iconX, iconY, iconR);
                    }
                    break;
                case INSTRUMENT:
                    if (!RomIcons.draw(c, RomIcons.of(RomIcons.instrument, kong), iconX, iconY, iconR)) {
                        DkIcons.instrument(c, kong, iconX, iconY, iconR);
                    }
                    break;
                default:
                    if (!RomIcons.draw(c, RomIcons.film, iconX, iconY, iconR)) {
                        DkIcons.camera(c, iconX, iconY, iconR);
                    }
                    break;
            }
            if (hasCount) {
                float countLeft = iconX + iconR * 1.2f;
                float countRight = r.right - r.width() * 0.08f;
                text.setTextAlign(Paint.Align.RIGHT);
                text.setColor(GOLD);
                text.setTextSize(fitText("000", countRight - countLeft, r.height() * 0.34f));
                c.drawText(counts[i], countRight, iconY + text.getTextSize() * 0.36f, text);
            }
            c.restoreToCount(saved);
        }
    }

    /** Largest text size up to {@code max} at which {@code s} fits in {@code width}. */
    private float fitText(String s, float width, float max) {
        text.setTextSize(max);
        float measured = text.measureText(s);
        return measured <= width ? max : max * width / measured;
    }

    // ------------------------------------------------------------------------------
    // Touch
    // ------------------------------------------------------------------------------

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (!active) {
            return false;
        }
        int action = event.getActionMasked();
        if (action != MotionEvent.ACTION_DOWN && action != MotionEvent.ACTION_POINTER_DOWN) {
            return true;
        }
        int index = event.getActionIndex();
        float x = event.getX(index), y = event.getY(index);

        float dx = x - fpX, dy = y - fpY;
        if (dx * dx + dy * dy <= fpR * fpR * 1.3f) {
            perform(Move.FIRST_PERSON);
            return true;
        }
        Move[] moves = { Move.WEAPON, Move.ORANGE, Move.INSTRUMENT, Move.CAMERA };
        for (int i = 0; i < 4; i++) {
            if (actionRects[i].contains(x, y)) {
                if (available(moves[i])) {
                    perform(moves[i]);
                }
                return true;
            }
        }
        for (int i = 0; i < tabRects.length; i++) {
            if (tabRects[i].contains(x, y)) {
                if (isLocked(i)) {
                    return true;
                }
                // Tapping the Kong you're playing (or the one already shown) goes back to
                // following whoever you play as.
                boolean current = state.inGame() && i == state.currentKong;
                selectedKong = (current || i == selectedKong) ? -1 : i;
                performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
                invalidate();
                return true;
            }
        }
        return true;
    }
}
