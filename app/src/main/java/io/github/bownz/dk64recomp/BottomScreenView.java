package io.github.bownz.dk64recomp;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;
import android.os.Handler;
import android.os.Looper;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;

/**
 * DK64 second screen (e.g. the AYN Thor's lower display), themed after the jungle-and-wood
 * look of Donkey Kong Country Returns 3D's touch screen:
 *
 *  - top-left: first-person view toggle (C-Up) on a barrel lid;
 *  - top: the game's own bobbing health melons, read live;
 *  - right column: one-tap Z + C moves for the current Kong, shown once they're bought;
 *  - centre: a barrel per Kong (with its face once freed) and that Kong's collectibles.
 *
 * Buttons drive the shared virtual gamepad ({@link TouchInput}); game values come from
 * {@link GameState} (read-only); icons from the player's ROM ({@link RomIcons}).
 */
public final class BottomScreenView extends View {

    private static final String[] KONG_NAMES = { "Donkey Kong", "Diddy Kong", "Lanky Kong", "Tiny Kong", "Chunky Kong" };
    private static final String[] KONG_SHORT = { "DK", "Diddy", "Lanky", "Tiny", "Chunky" };
    private static final int[] KONG_COLORS = {
        Color.rgb(242, 194, 48), Color.rgb(217, 56, 58), Color.rgb(60, 120, 216),
        Color.rgb(155, 89, 208), Color.rgb(76, 175, 80),
    };

    // Jungle and wood palette.
    private static final int JUNGLE_TOP = Color.rgb(24, 70, 44);
    private static final int JUNGLE_BOTTOM = Color.rgb(10, 30, 20);
    private static final int WOOD_LIGHT = Color.rgb(150, 96, 48);
    private static final int WOOD_DARK = Color.rgb(104, 62, 28);
    private static final int WOOD_SEAM = Color.rgb(70, 40, 16);
    private static final int WOOD_EDGE = Color.rgb(52, 29, 10);
    private static final int WOOD_SHINE = Color.argb(110, 214, 160, 96);
    private static final int BANANA = Color.rgb(255, 214, 40);
    private static final int OUTLINE = Color.rgb(58, 30, 6);
    private static final int CREAM = Color.rgb(255, 244, 214);
    private static final int COMPLETE = Color.rgb(140, 230, 110);

    // Input timing. DK64 reads input once per game frame (~33 ms at 30 fps), so each step
    // lasts a few frames: hold Z, press C while Z is still held, then let go.
    private static final long Z_LEAD_MS = 100;
    private static final long C_HOLD_MS = 150;
    private static final long Z_TAIL_MS = 60;
    private static final long POLL_MS = 250;

    private enum Move { WEAPON, ORANGE, INSTRUMENT, CAMERA, FIRST_PERSON }

    private static final Move[] ACTIONS = { Move.WEAPON, Move.ORANGE, Move.INSTRUMENT, Move.CAMERA };

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final GameState state = new GameState();
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);

    // Layout, in pixels (computed in onSizeChanged).
    private final RectF[] actionRects = { new RectF(), new RectF(), new RectF(), new RectF() };
    /** Tabs: the five Kongs, then shared (non-Kong) collectibles. */
    private static final int SHARED_TAB = GameState.KONG_COUNT;
    private static final int TAB_COUNT = GameState.KONG_COUNT + 1;
    private final RectF[] tabRects = new RectF[TAB_COUNT];
    private final RectF locationRect = new RectF();
    private boolean showTotals;      // location bar: this level, or the whole game
    private final RectF panelRect = new RectF();
    private final RectF tmp = new RectF();
    private final RectF barrelRect = new RectF();
    private final Path path = new Path();
    private float fpX, fpY, fpR;
    private float healthX, healthY, melonR;
    private float unit;
    private float rowText;

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
            handler.removeCallbacksAndMessages(null);
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

    /** The tab being shown: a Kong (0-4) or SHARED_TAB. */
    private int displayedKong() {
        if (selectedKong == SHARED_TAB || (selectedKong >= 0 && !isLocked(selectedKong))) {
            return selectedKong;
        }
        return state.inGame() ? state.currentKong : GameState.DK;
    }

    private int playingKong() {
        return state.inGame() ? state.currentKong : GameState.DK;
    }

    /** Kongs not freed yet in the running game (unknown before a game starts: all shown). */
    private boolean isLocked(int k) {
        return k < GameState.KONG_COUNT && state.inGame() && !state.isFreed(k);
    }

    /**
     * Whether the current Kong has this move at all; buttons for moves not bought yet are
     * hidden. Before a game is running everything is shown.
     */
    private boolean owned(Move move) {
        if (!state.inGame()) {
            return true;
        }
        GameState.Kong k = state.kongs[state.currentKong];
        switch (move) {
            case WEAPON: return k.weapon > 0;
            case ORANGE: return state.hasOranges() || state.oranges > 0;
            case INSTRUMENT: return k.instrument > 0;
            case CAMERA: return state.hasCamera() || state.film > 0;
            default: return true;
        }
    }

    /** Whether an owned move has something to use; if not its button is greyed out. */
    private boolean available(Move move) {
        if (!state.inGame()) {
            return true;
        }
        GameState.Kong k = state.kongs[state.currentKong];
        switch (move) {
            case WEAPON: return state.ammo > 0 || state.homingAmmo > 0;
            case ORANGE: return state.oranges > 0;
            case INSTRUMENT: return k.instrumentEnergy > 0;
            case CAMERA: return state.film > 0;
            default: return true;
        }
    }

    // ------------------------------------------------------------------------------
    // Layout
    // ------------------------------------------------------------------------------

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        layoutFor(w, h);
    }

    void layoutFor(int w, int h) {
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

        melonR = fpR * 0.75f;
        healthX = fpX + fpR + m * 1.8f + melonR;
        healthY = fpY;

        float tabsTop = m + 2 * fpR + m;
        float tabH = unit * 0.13f;
        float tabGap = m * 0.5f;
        float tabW = (right - left - tabGap * (TAB_COUNT - 1)) / TAB_COUNT;
        for (int i = 0; i < TAB_COUNT; i++) {
            float l = left + i * (tabW + tabGap);
            tabRects[i].set(l, tabsTop, l + tabW, tabsTop + tabH);
        }
        float barH = unit * 0.1f;
        locationRect.set(left, h - m - barH, right, h - m);
        panelRect.set(left, tabsTop + tabH + m * 0.6f, right, locationRect.top - m * 0.5f);
    }

    // ------------------------------------------------------------------------------
    // Drawing
    // ------------------------------------------------------------------------------

    @Override
    protected void onDraw(Canvas canvas) {
        drawJungle(canvas, getWidth(), getHeight());
        drawFirstPersonButton(canvas);
        drawHealth(canvas);
        drawTabs(canvas);
        drawKongPanel(canvas);
        drawLocationBar(canvas);
        drawActions(canvas);
    }

    /** Deep jungle backdrop with big leaves around the edges. */
    private void drawJungle(Canvas c, int w, int h) {
        paint.setStyle(Paint.Style.FILL);
        paint.setShader(new LinearGradient(0, 0, 0, h, JUNGLE_TOP, JUNGLE_BOTTOM, Shader.TileMode.CLAMP));
        c.drawRect(0, 0, w, h, paint);
        paint.setShader(null);
        // Soft light from above, through the canopy.
        paint.setShader(new LinearGradient(0, 0, 0, h * 0.5f, Color.argb(60, 190, 230, 120), Color.argb(0, 190, 230, 120), Shader.TileMode.CLAMP));
        c.drawRect(0, 0, w, h * 0.5f, paint);
        paint.setShader(null);
        float s = unit;
        leaf(c, w * 0.02f, h * 1.02f, s * 0.42f, -60, Color.argb(120, 20, 96, 46));
        leaf(c, w * 0.30f, h * 1.05f, s * 0.34f, -100, Color.argb(100, 30, 110, 50));
        leaf(c, w * 0.62f, h * 1.03f, s * 0.38f, -75, Color.argb(110, 16, 84, 40));
        leaf(c, w * 1.01f, h * 0.55f, s * 0.40f, -160, Color.argb(90, 26, 100, 48));
        leaf(c, w * 0.50f, -h * 0.03f, s * 0.30f, 115, Color.argb(80, 40, 120, 56));
        leaf(c, -w * 0.01f, h * 0.30f, s * 0.30f, 10, Color.argb(80, 24, 92, 44));
    }

    /** A jungle leaf from (x, y), length len, pointing at angle degrees, with a midrib. */
    private void leaf(Canvas c, float x, float y, float len, float angle, int color) {
        int saved = c.save();
        c.translate(x, y);
        c.rotate(angle);
        float wdt = len * 0.32f;
        path.reset();
        path.moveTo(0, 0);
        path.quadTo(len * 0.35f, -wdt, len, 0);
        path.quadTo(len * 0.35f, wdt, 0, 0);
        path.close();
        paint.setColor(color);
        c.drawPath(path, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(len * 0.012f);
        paint.setColor(Color.argb(Color.alpha(color) / 2, 160, 210, 120));
        c.drawLine(0, 0, len * 0.95f, 0, paint);
        for (int i = 1; i <= 4; i++) {
            float vx = len * i * 0.18f;
            c.drawLine(vx, 0, vx + len * 0.12f, -wdt * 0.55f * (1 - i * 0.15f), paint);
            c.drawLine(vx, 0, vx + len * 0.12f, wdt * 0.55f * (1 - i * 0.15f), paint);
        }
        paint.setStyle(Paint.Style.FILL);
        c.restoreToCount(saved);
    }

    /** A wooden board: planks with seams and grain, a dark edge and nails. */
    private void woodPanel(Canvas c, RectF r, float radius, boolean lit) {
        paint.setStyle(Paint.Style.FILL);
        paint.setShader(new LinearGradient(0, r.top, 0, r.bottom,
            lit ? Color.rgb(186, 124, 64) : WOOD_LIGHT, lit ? Color.rgb(136, 84, 40) : WOOD_DARK, Shader.TileMode.CLAMP));
        c.drawRoundRect(r, radius, radius, paint);
        paint.setShader(null);

        int saved = c.save();
        path.reset();
        path.addRoundRect(r, radius, radius, Path.Direction.CW);
        c.clipPath(path);
        // Planks, about a tenth of the screen tall.
        int planks = Math.max(1, Math.round(r.height() / (unit * 0.1f)));
        float plankH = r.height() / planks;
        paint.setStyle(Paint.Style.STROKE);
        for (int i = 0; i < planks; i++) {
            float top = r.top + i * plankH;
            if (i > 0) {
                paint.setStrokeWidth(unit * 0.005f);
                paint.setColor(WOOD_SEAM);
                c.drawLine(r.left, top, r.right, top, paint);
            }
            // Grain: a couple of long, faint streaks per plank, placed by a stable hash.
            paint.setStrokeWidth(unit * 0.0025f);
            paint.setColor(Color.argb(45, 40, 20, 6));
            int seed = (int) (r.left * 7 + top * 13);
            for (int g = 0; g < 2; g++) {
                float gy = top + plankH * (0.3f + 0.4f * g) + ((seed >> (g * 3)) & 7) * plankH * 0.02f;
                float gx0 = r.left + r.width() * (((seed >> (g * 5)) & 15) / 40f);
                c.drawLine(gx0, gy, gx0 + r.width() * 0.55f, gy + plankH * 0.04f, paint);
            }
        }
        paint.setStyle(Paint.Style.FILL);
        c.restoreToCount(saved);

        // Edge, highlight and corner nails.
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * 0.011f);
        paint.setColor(lit ? BANANA : WOOD_EDGE);
        c.drawRoundRect(r, radius, radius, paint);
        float inset = unit * 0.011f;
        tmp.set(r.left + inset, r.top + inset, r.right - inset, r.bottom - inset);
        paint.setStrokeWidth(unit * 0.004f);
        paint.setColor(WOOD_SHINE);
        c.drawRoundRect(tmp, radius * 0.8f, radius * 0.8f, paint);
        paint.setStyle(Paint.Style.FILL);
        float n = Math.max(radius * 0.55f, unit * 0.022f);
        nail(c, r.left + n, r.top + n);
        nail(c, r.right - n, r.top + n);
        nail(c, r.left + n, r.bottom - n);
        nail(c, r.right - n, r.bottom - n);
    }

    private void nail(Canvas c, float x, float y) {
        float r = unit * 0.008f;
        paint.setColor(Color.rgb(60, 52, 44));
        c.drawCircle(x, y, r, paint);
        paint.setColor(Color.rgb(170, 160, 146));
        c.drawCircle(x - r * 0.25f, y - r * 0.25f, r * 0.45f, paint);
    }

    /** Chunky banana-yellow text with a dark outline, DKC style. */
    private void outlined(Canvas c, String s, float x, float y, int fill) {
        text.setStyle(Paint.Style.STROKE);
        text.setStrokeWidth(text.getTextSize() * 0.16f);
        text.setStrokeJoin(Paint.Join.ROUND);
        text.setColor(OUTLINE);
        c.drawText(s, x, y, text);
        text.setStyle(Paint.Style.FILL);
        text.setColor(fill);
        c.drawText(s, x, y, text);
    }

    /** A DK barrel (front view) filling r: bulging staves, two iron hoops. */
    private void barrel(Canvas c, RectF r, boolean lit, boolean locked) {
        float bulge = r.width() * 0.07f;
        path.reset();
        path.moveTo(r.left + bulge, r.top);
        path.lineTo(r.right - bulge, r.top);
        path.quadTo(r.right + bulge, r.centerY(), r.right - bulge, r.bottom);
        path.lineTo(r.left + bulge, r.bottom);
        path.quadTo(r.left - bulge, r.centerY(), r.left + bulge, r.top);
        path.close();
        paint.setStyle(Paint.Style.FILL);
        if (locked) {
            paint.setColor(Color.rgb(58, 56, 52));
        } else {
            paint.setShader(new LinearGradient(r.left, 0, r.right, 0,
                new int[] { WOOD_DARK, lit ? Color.rgb(196, 132, 66) : WOOD_LIGHT, WOOD_DARK }, null, Shader.TileMode.CLAMP));
        }
        c.drawPath(path, paint);
        paint.setShader(null);

        // Staves.
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * 0.003f);
        paint.setColor(locked ? Color.rgb(44, 42, 40) : WOOD_SEAM);
        for (int i = 1; i < 4; i++) {
            float x = r.left + r.width() * i / 4f;
            c.drawLine(x, r.top, x, r.bottom, paint);
        }
        // Hoops.
        paint.setStrokeWidth(r.height() * 0.07f);
        paint.setColor(locked ? Color.rgb(72, 72, 72) : Color.rgb(58, 58, 64));
        float h1 = r.top + r.height() * 0.17f, h2 = r.bottom - r.height() * 0.17f;
        c.drawLine(r.left + bulge * 0.2f, h1, r.right - bulge * 0.2f, h1, paint);
        c.drawLine(r.left + bulge * 0.2f, h2, r.right - bulge * 0.2f, h2, paint);
        // Outline (gold when selected).
        paint.setStrokeWidth(unit * (lit ? 0.011f : 0.007f));
        paint.setColor(lit ? BANANA : (locked ? Color.rgb(34, 34, 34) : WOOD_EDGE));
        c.drawPath(path, paint);
        paint.setStyle(Paint.Style.FILL);
    }

    private void drawFirstPersonButton(Canvas c) {
        boolean pressed = running == Move.FIRST_PERSON;
        // Barrel lid: rings of wood.
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(WOOD_EDGE);
        c.drawCircle(fpX, fpY, fpR, paint);
        paint.setColor(pressed ? Color.rgb(186, 124, 64) : WOOD_LIGHT);
        c.drawCircle(fpX, fpY, fpR * 0.9f, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * 0.003f);
        paint.setColor(Color.argb(90, 60, 32, 10));
        c.drawCircle(fpX, fpY, fpR * 0.7f, paint);
        c.drawCircle(fpX, fpY, fpR * 0.48f, paint);
        paint.setStrokeWidth(unit * (pressed ? 0.012f : 0.008f));
        paint.setColor(pressed ? BANANA : Color.rgb(58, 58, 64));
        c.drawCircle(fpX, fpY, fpR * 0.95f, paint);
        paint.setStyle(Paint.Style.FILL);
        DkIcons.eye(c, fpX, fpY - fpR * 0.12f, fpR * 0.5f);
        text.setTextAlign(Paint.Align.CENTER);
        text.setTextSize(fitText("1st person", fpR * 1.3f, fpR * 0.27f));
        outlined(c, "1st person", fpX, fpY + fpR * 0.58f, CREAM);
    }

    private void drawHealth(Canvas c) {
        text.setTextAlign(Paint.Align.LEFT);
        if (!state.inGame()) {
            String hint = "Start a game to see health";
            float hintX = healthX - melonR;
            text.setTextSize(fitText(hint, tabRects[TAB_COUNT - 1].right - hintX, melonR * 0.55f));
            outlined(c, hint, hintX, healthY + melonR * 0.2f, CREAM);
            return;
        }
        float x = healthX;
        int remaining = state.health;
        for (int i = 0; i < state.melons; i++) {
            drawMelon(c, x, healthY, melonR, Math.max(0, Math.min(4, remaining)));
            remaining -= 4;
            x += melonR * 2.4f;
        }
    }

    /**
     * One health melon, as the game's HUD and pause screen show it (code_10D2D0.c): the full
     * melon drawn faintly, the remaining quarters on top. Falls back to our own drawing.
     */
    private void drawMelon(Canvas c, float cx, float cy, float r, int quarters) {
        Bitmap full = RomIcons.healthMelon[3];
        if (full == null) {
            DkIcons.melon(c, cx, cy, r, quarters);
            return;
        }
        int faint = c.saveLayerAlpha(cx - r * 1.2f, cy - r * 1.2f, cx + r * 1.2f, cy + r * 1.2f, 80);
        RomIcons.draw(c, full, cx, cy, r);
        c.restoreToCount(faint);
        if (quarters > 0) {
            RomIcons.draw(c, RomIcons.healthMelon[quarters - 1], cx, cy, r);
        }
    }

    private void drawTabs(Canvas c) {
        int shown = displayedKong();
        int playing = state.inGame() ? state.currentKong : -1;
        text.setTextAlign(Paint.Align.CENTER);
        for (int i = 0; i < tabRects.length; i++) {
            // Barrels keep a barrel's proportions however wide the tab is.
            RectF t = tabRects[i];
            float bw = Math.min(t.width(), t.height() * 0.95f);
            RectF r = barrelRect;
            r.set(t.centerX() - bw / 2, t.top, t.centerX() + bw / 2, t.bottom);
            boolean locked = isLocked(i);
            boolean lit = !locked && i == shown;
            barrel(c, r, lit, locked);
            if (locked) {
                continue; // Greyed out, no face or name, until the Kong is freed.
            }
            float faceR = Math.min(r.height(), r.width()) * 0.36f;
            if (i == SHARED_TAB) {
                // Shared collectibles: a banana fairy (or a golden banana) on the barrel.
                Bitmap icon = RomIcons.fairy != null ? RomIcons.fairy : RomIcons.goldenBanana;
                if (!RomIcons.draw(c, icon, r.centerX(), r.centerY(), faceR)) {
                    DkIcons.banana(c, r.centerX(), r.centerY(), faceR * 0.8f);
                }
                continue;
            }
            Bitmap face = RomIcons.of(RomIcons.face, i);
            if (face != null) {
                tmp.set(r.centerX() - faceR, r.centerY() - faceR, r.centerX() + faceR, r.centerY() + faceR);
                path.reset();
                path.addOval(tmp, Path.Direction.CW);
                int saved = c.save();
                c.clipPath(path);
                RomIcons.draw(c, face, r.centerX(), r.centerY(), faceR);
                c.restoreToCount(saved);
                paint.setStyle(Paint.Style.STROKE);
                paint.setStrokeWidth(unit * 0.005f);
                paint.setColor(lit ? BANANA : WOOD_EDGE);
                c.drawOval(tmp, paint);
                paint.setStyle(Paint.Style.FILL);
            } else {
                text.setTextSize(fitText(KONG_SHORT[i], r.width() * 0.8f, r.height() * 0.3f));
                outlined(c, KONG_SHORT[i], r.centerX(), r.centerY() + text.getTextSize() * 0.35f, KONG_COLORS[i]);
            }
            if (i == playing) {
                // A little banana marks the Kong you're playing.
                DkIcons.banana(c, r.right - r.width() * 0.16f, r.top + r.height() * 0.17f, r.height() * 0.14f);
            }
        }
    }

    /** One line of the collectibles panel. total < 0: just a count (no "of"). */
    private static final class Row {
        final String label;
        final Bitmap icon;
        final int fallback;   // drawn icon if the ROM one is missing: 0 banana, 1 coin, 2 star
        final int have, total;

        Row(String label, Bitmap icon, int fallback, int have, int total) {
            this.label = label;
            this.icon = icon;
            this.fallback = fallback;
            this.have = have;
            this.total = total;
        }
    }

    /** The level the location bar is on, or -1 for the whole game. */
    private int shownLevel() {
        return showTotals || state.level < 0 ? -1 : state.level;
    }

    private java.util.List<Row> rowsFor(int tab, int level) {
        java.util.List<Row> rows = new java.util.ArrayList<>();
        boolean all = level < 0;
        if (tab == SHARED_TAB) {
            int[][][] lists = { GameState.FAIRY_FLAGS, GameState.CROWN_FLAGS, GameState.KEY_FLAGS, GameState.RAINBOW_COIN_FLAGS };
            String[] labels = { "Banana Fairies", "Battle Crowns", "Boss Keys", "Rainbow Coins" };
            Bitmap[] icons = { RomIcons.fairy, RomIcons.crown, RomIcons.key, RomIcons.rainbowCoin };
            for (int i = 0; i < lists.length; i++) {
                int have = 0, total = 0;
                for (int l = 0; l < GameState.LEVELS; l++) {
                    if (all || l == level) {
                        have += state.countFlags(lists[i][l]);
                        total += lists[i][l].length;
                    }
                }
                rows.add(new Row(labels[i], icons[i], 2, have, total));
            }
            if (all) {
                int golden = 0;
                for (GameState.Kong k : state.kongs) {
                    golden += k.goldenBananas;
                }
                rows.add(0, new Row("Golden Bananas", RomIcons.goldenBanana, 0, golden, 201));
            }
            return rows;
        }

        GameState.Kong kong = state.kongs[tab];
        int golden = 0, goldenTotal = 0, coloured = 0, colouredTotal = 0;
        int prints = 0, printsTotal = 0, medals = 0, medalsTotal = 0;
        for (int l = 0; l < GameState.LEVELS; l++) {
            if (!all && l != level) {
                continue;
            }
            golden += state.goldenByLevel[tab][l];
            coloured += state.colouredByLevel[tab][l];
            goldenTotal += l <= GameState.ISLES ? 5 : 0;        // 5 per Kong per level and Isles
            colouredTotal += l < GameState.ISLES ? 100 : 0;     // 100 per Kong per level
            int bp = GameState.blueprintFlag(tab, l), md = GameState.medalFlag(tab, l);
            if (bp >= 0) {
                printsTotal++;
                prints += state.flag(bp) ? 1 : 0;
            }
            if (md >= 0) {
                medalsTotal++;
                medals += state.flag(md) ? 1 : 0;
            }
        }
        if (all) {
            // Includes the odd one out (Rareware) where the per-level split doesn't.
            golden = kong.goldenBananas;
            coloured = kong.colouredBananas;
        }
        rows.add(new Row("Golden Bananas", RomIcons.goldenBanana, 0, golden, goldenTotal));
        rows.add(new Row("Bananas", RomIcons.of(RomIcons.banana, tab), 0, coloured, colouredTotal));
        rows.add(new Row(printsTotal == 1 ? "Blueprint" : "Blueprints", RomIcons.blueprint, 2, prints, printsTotal));
        rows.add(new Row(medalsTotal == 1 ? "Banana Medal" : "Banana Medals", RomIcons.medal, 2, medals, medalsTotal));
        rows.add(new Row("Coins", null, 1, kong.coins, -1));
        return rows;
    }

    private void drawKongPanel(Canvas c) {
        RectF p = panelRect;
        woodPanel(c, p, unit * 0.03f, false);

        int tab = displayedKong();
        boolean known = state.inGame();
        java.util.List<Row> rows = rowsFor(tab, shownLevel());
        rows.removeIf(r -> r.total == 0);   // nothing of that kind here (e.g. bananas in Helm)

        float pad = p.width() * 0.06f;
        float rowH = Math.min((p.height() - pad * 0.5f) / (rows.size() + 1.75f), unit * 0.13f);
        float x = p.left + pad;
        float y = p.top + pad * 0.4f + rowH * 0.85f;

        String title = tab == SHARED_TAB ? "Shared Items" : KONG_NAMES[tab];
        text.setTextAlign(Paint.Align.LEFT);
        text.setTextSize(fitText(title, p.width() - 2 * pad, rowH * 0.62f));
        outlined(c, title, x, y, tab == SHARED_TAB ? BANANA : KONG_COLORS[tab]);
        y += rowH * 1.2f;

        // One text size for every row, set by the longest label.
        float labelX = x + rowH * 0.8f;
        text.setTextSize(rowH * 0.46f);
        float valueW = text.measureText("000/000");
        rowText = rowH * 0.46f;
        for (Row r : rows) {
            rowText = Math.min(rowText, fitText(r.label, p.right - pad - labelX - valueW - rowH * 0.3f, rowH * 0.46f));
        }
        for (Row r : rows) {
            String value = !known ? "–" : r.total < 0 ? String.valueOf(r.have) : r.have + "/" + r.total;
            boolean complete = known && r.total > 0 && r.have >= r.total;
            statRow(c, x, y, p.right - pad, rowH, r.label, value, r.icon, r.fallback, complete);
            y += rowH;
        }
    }

    private void statRow(Canvas c, float x, float y, float right, float rowH, String label, String value,
                         Bitmap icon, int fallback, boolean complete) {
        float iconX = x + rowH * 0.3f, iconY = y - rowH * 0.18f, iconR = rowH * 0.34f;
        if (!RomIcons.draw(c, icon, iconX, iconY, iconR)) {
            switch (fallback) {
                case 0: DkIcons.banana(c, iconX, iconY, iconR * 0.8f); break;
                case 1: DkIcons.coin(c, iconX, iconY, iconR * 0.7f); break;
                default: DkIcons.star(c, iconX, iconY, iconR * 0.8f); break;
            }
        }
        float labelX = x + rowH * 0.8f;
        text.setTextSize(rowText);
        text.setTextAlign(Paint.Align.LEFT);
        outlined(c, label, labelX, y, CREAM);
        text.setTextAlign(Paint.Align.RIGHT);
        outlined(c, value, right, y, complete ? COMPLETE : BANANA);
    }

    /** Wooden sign under the panel: the level shown, with arrows to switch to all levels. */
    private void drawLocationBar(Canvas c) {
        RectF r = locationRect;
        woodPanel(c, r, r.height() * 0.3f, false);
        int level = shownLevel();
        String title = level < 0 ? "All Levels" : GameState.LEVEL_NAMES[level];
        boolean canSwitch = state.level >= 0;
        float arrowR = r.height() * 0.26f;
        if (canSwitch) {
            arrow(c, r.left + r.height() * 0.55f, r.centerY(), arrowR, -1);
            arrow(c, r.right - r.height() * 0.55f, r.centerY(), arrowR, 1);
        }
        text.setTextAlign(Paint.Align.CENTER);
        text.setTextSize(fitText(title, r.width() - r.height() * 2.4f, r.height() * 0.5f));
        outlined(c, title, r.centerX(), r.centerY() + text.getTextSize() * 0.36f, BANANA);
    }

    /** A chunky outlined triangle pointing left (dir -1) or right (dir 1). */
    private void arrow(Canvas c, float cx, float cy, float r, int dir) {
        path.reset();
        path.moveTo(cx + dir * r, cy);
        path.lineTo(cx - dir * r * 0.7f, cy - r);
        path.lineTo(cx - dir * r * 0.7f, cy + r);
        path.close();
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(BANANA);
        c.drawPath(path, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeJoin(Paint.Join.ROUND);
        paint.setStrokeWidth(r * 0.25f);
        paint.setColor(OUTLINE);
        c.drawPath(path, paint);
        paint.setStyle(Paint.Style.FILL);
    }

    private void drawActions(Canvas c) {
        int kong = playingKong();
        boolean known = state.inGame();
        GameState.Kong k = state.kongs[kong];
        String[] counts = known
            ? new String[] { String.valueOf(state.ammo), String.valueOf(state.oranges), String.valueOf(k.instrumentEnergy), String.valueOf(state.film) }
            : new String[] { "", "", "", "" };

        for (int i = 0; i < 4; i++) {
            Move move = ACTIONS[i];
            RectF r = actionRects[i];
            if (!owned(move)) {
                // Not bought yet: just an empty notch in the wood.
                paint.setColor(Color.argb(70, 0, 0, 0));
                c.drawRoundRect(r, r.height() * 0.18f, r.height() * 0.18f, paint);
                continue;
            }
            boolean pressed = running == move;
            boolean enabled = available(move);

            int saved = c.saveLayerAlpha(r.left - unit * 0.02f, r.top - unit * 0.02f,
                r.right + unit * 0.02f, r.bottom + unit * 0.02f, enabled ? 255 : 90);
            woodPanel(c, r, r.height() * 0.18f, pressed);

            // Icon, with the count beside it.
            boolean hasCount = !counts[i].isEmpty();
            float iconR = Math.min(r.height() * 0.36f, r.width() * (hasCount ? 0.22f : 0.3f));
            float iconX = hasCount ? r.left + r.width() * 0.1f + iconR : r.centerX();
            float iconY = r.centerY();
            switch (move) {
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
                float countRight = r.right - r.width() * 0.1f;
                text.setTextAlign(Paint.Align.RIGHT);
                text.setTextSize(fitText("000", countRight - countLeft, r.height() * 0.36f));
                outlined(c, counts[i], countRight, iconY + text.getTextSize() * 0.36f, BANANA);
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
        for (int i = 0; i < 4; i++) {
            if (actionRects[i].contains(x, y)) {
                if (owned(ACTIONS[i]) && available(ACTIONS[i])) {
                    perform(ACTIONS[i]);
                }
                return true;
            }
        }
        if (locationRect.contains(x, y)) {
            if (state.level >= 0) {
                showTotals = !showTotals;
                performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
                invalidate();
            }
            return true;
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
