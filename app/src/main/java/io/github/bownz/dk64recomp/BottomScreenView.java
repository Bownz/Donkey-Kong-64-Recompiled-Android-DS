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
    private static final long POLL_MS = 150;

    private enum Move { WEAPON, ORANGE, INSTRUMENT, CAMERA, FIRST_PERSON }

    private static final Move[] ACTIONS = { Move.ORANGE, Move.WEAPON, Move.INSTRUMENT, Move.CAMERA };

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
    private final Paint bitmapPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
    private Bitmap logo;
    private boolean logoTried;
    private final RectF tmp2 = new RectF();
    private final RectF boardRect = new RectF();
    private final RectF barrelRect = new RectF();
    private final Path path = new Path();
    private float fpX, fpY, fpR;
    private float osX, osY, osR;
    private float healthX, healthY, melonR;
    private float unit;
    private float rowText;

    private Move running;
    private int selectedKong = -1;   // -1: follow the Kong being played
    private boolean active;

    private final Runnable poll = new Runnable() {
        @Override
        public void run() {
            int levelBefore = state.level;
            int crystalsBefore = state.crystals;
            boolean changed = state.refresh();
            if (state.inGame() && state.crystals < crystalsBefore) {
                crystalsDrainedAt = android.os.SystemClock.uptimeMillis();
            }
            boolean cancelBefore = cancelShown;
            cancelShown = showCancel();
            changed |= cancelShown != cancelBefore;
            updateOrangstand();
            if (changed) {
                if (state.level != levelBefore && state.level >= 0) {
                    showTotals = false;   // entering a new place shows that place first
                }
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
            handler.post(poll);
        } else {
            cancel();
            handler.removeCallbacksAndMessages(null);
        }
    }

    /** Stops any move in progress and lets go of every input (e.g. when the app pauses). */
    public void cancel() {
        handler.removeCallbacksAndMessages(null);
        if (running != null) {
            TouchInput.setN64Buttons(0);
            running = null;
            invalidate();
        }
        if (swapTarget >= 0) {
            swapEnd();
        }
        if (orangstandBusy || orangstand) {
            TouchInput.setN64Buttons(0);
            TouchInput.setHeldN64Buttons(0);
            orangstandBusy = false;
            orangstand = false;
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
        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        invalidate();

        // The buttons press N64 Z and C directly (not through the controller mappings).
        final int c;
        switch (move) {
            case WEAPON:     c = TouchInput.N64_C_LEFT; break;
            case ORANGE:     c = TouchInput.N64_C_RIGHT; break;
            case INSTRUMENT: c = TouchInput.N64_C_UP; break;
            case CAMERA:     c = TouchInput.N64_C_DOWN; break;
            default:         c = TouchInput.N64_C_UP; break;
        }

        if (move == Move.FIRST_PERSON) {
            // C-Up on its own enters first-person view, and pressing it again leaves it.
            TouchInput.setN64Buttons(c);
            handler.postDelayed(() -> {
                TouchInput.setN64Buttons(0);
                running = null;
                invalidate();
            }, C_HOLD_MS);
            return;
        }

        final int z = TouchInput.N64_Z;
        TouchInput.setN64Buttons(z);                                                        // Z
        handler.postDelayed(() -> TouchInput.setN64Buttons(z | c), Z_LEAD_MS);              // + C
        handler.postDelayed(() -> TouchInput.setN64Buttons(z), Z_LEAD_MS + C_HOLD_MS);
        handler.postDelayed(() -> {
            TouchInput.setN64Buttons(0);
            running = null;
            invalidate();
        }, Z_LEAD_MS + C_HOLD_MS + Z_TAIL_MS);
    }

    // ------------------------------------------------------------------------------
    // Touch Kong Swap (with the Tag Anywhere mod)
    // ------------------------------------------------------------------------------
    //
    // Tag Anywhere swaps to the next / previous freed Kong on D-pad Right / Left. To reach the
    // tapped Kong, the D-pad is pressed (directly, like the action buttons) in the shorter
    // direction and held until the Kong changes, released, and repeated until it's the
    // tapped one. The mod ignores the D-pad while tagging isn't allowed (in the air, holding
    // something, ...); if the Kong doesn't change in time the swap gives up.

    private static final long SWAP_TICK_MS = 20;
    private static final long SWAP_STEP_TIMEOUT_MS = 700;
    private static final long SWAP_RELEASE_MS = 90;

    private int swapTarget = -1;
    private int swapButton;
    private int swapFrom;
    private long swapStepStart;
    private long swapReleasedAt;
    private boolean swapPressed;
    private boolean swapHintShown;
    private int swapSteps;

    /**
     * Swaps to Kong {@code k} if Touch Kong Swap is on. Returns false to fall back to just
     * showing that Kong's tab.
     */
    private boolean trySwap(int k, boolean current) {
        int bits = GameState.kongSwap();
        if ((bits & GameState.SWAP_SETTING) == 0) {
            return false;
        }
        if ((bits & GameState.SWAP_MOD) == 0) {
            if (!swapHintShown) {
                swapHintShown = true;
                android.widget.Toast.makeText(getContext(),
                    "Touch Kong Swap needs the Tag Anywhere mod: enable it in Mods, then restart the game.",
                    android.widget.Toast.LENGTH_LONG).show();
            }
            return false;
        }
        selectedKong = -1;   // follow the Kong being played
        if (current || swapTarget >= 0 || running != null) {
            invalidate();
            return true;
        }
        if ((bits & GameState.SWAP_PLAYING) == 0) {
            return true;     // paused or in a cutscene: the D-pad would go to a menu
        }
        // Count steps each way through the freed Kongs (the mod skips locked ones).
        int right = 0, left = 0;
        for (int i = 1, kong = state.currentKong; i <= GameState.KONG_COUNT; i++) {
            kong = (kong + 1) % GameState.KONG_COUNT;
            if (!isLocked(kong)) {
                right++;
            }
            if (kong == k) {
                break;
            }
        }
        for (int i = 1, kong = state.currentKong; i <= GameState.KONG_COUNT; i++) {
            kong = (kong + GameState.KONG_COUNT - 1) % GameState.KONG_COUNT;
            if (!isLocked(kong)) {
                left++;
            }
            if (kong == k) {
                break;
            }
        }
        swapButton = right <= left ? TouchInput.N64_DPAD_RIGHT : TouchInput.N64_DPAD_LEFT;
        swapTarget = k;
        swapSteps = 0;
        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        swapPress();
        handler.postDelayed(swapTick, SWAP_TICK_MS);
        invalidate();
        return true;
    }

    private void swapPress() {
        if (++swapSteps > GameState.KONG_COUNT) {
            swapEnd();   // went all the way round without reaching it
            return;
        }
        swapFrom = state.currentKong;
        swapStepStart = android.os.SystemClock.uptimeMillis();
        swapPressed = true;
        TouchInput.setN64Buttons(swapButton);
    }

    private void swapEnd() {
        TouchInput.setN64Buttons(0);
        swapPressed = false;
        swapTarget = -1;
        invalidate();
    }

    private final Runnable swapTick = new Runnable() {
        @Override
        public void run() {
            if (swapTarget < 0) {
                return;
            }
            long now = android.os.SystemClock.uptimeMillis();
            if (state.refresh()) {
                invalidate();
            }
            if (!state.inGame() || (GameState.kongSwap() & GameState.SWAP_PLAYING) == 0 && !swapPressed) {
                swapEnd();
                return;
            }
            if (swapPressed) {
                if (state.currentKong != swapFrom) {
                    // This step worked; let go so the mod sees the next press.
                    TouchInput.setN64Buttons(0);
                    swapPressed = false;
                    swapReleasedAt = now;
                    if (state.currentKong == swapTarget) {
                        swapEnd();
                        return;
                    }
                } else if (now - swapStepStart > SWAP_STEP_TIMEOUT_MS) {
                    swapEnd();   // can't tag here right now
                    return;
                }
            } else if (now - swapReleasedAt >= SWAP_RELEASE_MS) {
                swapPress();
            }
            handler.postDelayed(this, SWAP_TICK_MS);
        }
    };

    // ------------------------------------------------------------------------------
    // Lanky's Orangstand toggle
    // ------------------------------------------------------------------------------
    //
    // A toggle: Z then B (as on a controller) puts Lanky on his hands, and Z stays held to
    // keep him there, as the game needs. Tapping again lets go of Z. It also lets go when he
    // ends up back on his feet or in water (Z stays held through the pause menu).

    private static final int SET_STANDING = 0x0C, SET_WALKING = 0x0D, SET_CROUCHING = 0x3C;
    private boolean orangstand;      // Lanky is on his hands (entered with this button)
    private boolean orangstandBusy;

    private boolean onFeet() {
        int set = state.inputSet;
        return set == SET_STANDING || set == SET_WALKING || set == SET_CROUCHING;
    }

    private boolean showOrangstand() {
        return state.inGame() && state.currentKong == GameState.LANKY
            && state.kongs[GameState.LANKY].moves >= 1;
    }

    /** Called on each poll: notice when Lanky is back on his feet (or isn't Lanky). */
    private void updateOrangstand() {
        if (orangstand && !orangstandBusy && (!showOrangstand() || (!state.paused && (onFeet() || state.swimming)))) {
            stopOrangstand();
        }
    }

    private void toggleOrangstand() {
        if (orangstandBusy || running != null || state.paused) {
            return;
        }
        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        if (orangstand) {
            // Letting go of Z puts Lanky back on his feet.
            stopOrangstand();
            return;
        }
        if (!state.canZMoves || state.gunOut) {
            return;
        }
        // Z, then B while Z is held; Z then stays held to keep him on his hands.
        orangstandBusy = true;
        invalidate();
        final int z = TouchInput.N64_Z, b = TouchInput.N64_B;
        TouchInput.setHeldN64Buttons(z);
        handler.postDelayed(() -> TouchInput.setN64Buttons(b), Z_LEAD_MS);
        handler.postDelayed(() -> TouchInput.setN64Buttons(0), Z_LEAD_MS + C_HOLD_MS);
        handler.postDelayed(() -> {
            state.refresh();
            orangstandBusy = false;
            orangstand = !onFeet() && !state.swimming;
            if (!orangstand) {
                TouchInput.setHeldN64Buttons(0);   // it didn't take: don't leave Z held
            }
            invalidate();
        }, Z_LEAD_MS + C_HOLD_MS + 350);
    }

    private void stopOrangstand() {
        orangstand = false;
        TouchInput.setHeldN64Buttons(0);
        invalidate();
    }

    // ------------------------------------------------------------------------------
    // Other Z + B moves (same place): Diddy's Chimpy Charge, Chunky's Primate Punch
    // ------------------------------------------------------------------------------
    //
    // Crouching then B (code_E4090.c): Diddy charges once he has his first Cranky move,
    // Chunky punches once he has his second.

    private boolean zbBusy;

    /** The current Kong's Z + B move, if bought; null if none (Lanky's is the toggle above). */
    private String zbMoveName() {
        if (!state.inGame()) {
            return null;
        }
        int k = state.currentKong;
        if (k == GameState.DIDDY && state.kongs[k].moves >= 1) {
            return "Charge";
        }
        if (k == GameState.CHUNKY && state.kongs[k].moves >= 2) {
            return "Punch";
        }
        return null;
    }

    private boolean zbMoveUsable() {
        return !state.paused && !state.swimming && !state.climbing && !state.gunOut;
    }

    private void performZbMove() {
        if (zbBusy || running != null || !zbMoveUsable() || !state.canZMoves) {
            return;
        }
        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        zbBusy = true;
        invalidate();
        final int z = TouchInput.N64_Z, b = TouchInput.N64_B;
        TouchInput.setN64Buttons(z);
        handler.postDelayed(() -> TouchInput.setN64Buttons(z | b), Z_LEAD_MS);
        handler.postDelayed(() -> TouchInput.setN64Buttons(z), Z_LEAD_MS + C_HOLD_MS);
        handler.postDelayed(() -> {
            TouchInput.setN64Buttons(0);
            zbBusy = false;
            invalidate();
        }, Z_LEAD_MS + C_HOLD_MS + Z_TAIL_MS);
    }

    /** A round orange badge with the Kong's face, named after the move. */
    private void drawZbMoveButton(Canvas c) {
        String name = zbMoveName();
        boolean enabled = zbMoveUsable();
        float r = osR * 0.92f;
        float cy = osY - osR * 0.08f;
        int saved = c.saveLayerAlpha(osX - osR * 1.1f, osY - osR * 1.1f, osX + osR * 1.1f, osY + osR * 1.1f, enabled ? 255 : 90);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(OUTLINE);
        c.drawCircle(osX, cy, r, paint);
        int top = zbBusy ? Color.rgb(250, 200, 90) : Color.rgb(200, 130, 50);
        int bottom = zbBusy ? Color.rgb(200, 120, 30) : Color.rgb(120, 70, 20);
        paint.setShader(new LinearGradient(0, cy - r, 0, cy + r, top, bottom, Shader.TileMode.CLAMP));
        c.drawCircle(osX, cy, r * 0.87f, paint);
        paint.setShader(null);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * 0.006f);
        paint.setColor(zbBusy ? BANANA : Color.argb(140, 255, 255, 255));
        c.drawCircle(osX, cy, r * 0.87f, paint);
        paint.setStyle(Paint.Style.FILL);
        if (!RomIcons.draw(c, RomIcons.face[state.currentKong], osX, cy, r * 0.62f)) {
            DkIcons.eye(c, osX, cy, r * 0.5f);
        }
        tmp.set(osX - osR * 0.95f, osY + osR * 0.58f, osX + osR * 0.95f, osY + osR * 0.98f);
        tagPlate(c, tmp);
        text.setTextAlign(Paint.Align.CENTER);
        text.setTextSize(fitText(name, tmp.width() * 0.8f, tmp.height() * 0.66f));
        outlined(c, name, tmp.centerX(), tmp.centerY() + text.getTextSize() * 0.36f, CREAM);
        c.restoreToCount(saved);
    }

    // ------------------------------------------------------------------------------
    // Ending a Kong barrel ability (same place as the Orangstand button)
    // ------------------------------------------------------------------------------
    //
    // Mini Monkey, Hunky Chunky, Strong Kong, Rocketbarrel, Baboon Balloon and the like use
    // up crystal coconuts while they last, and Z + C-Left ends them (code_CEAE0.c). The
    // button shows while the game reports one, or while crystals are running down.

    private long crystalsDrainedAt;
    private boolean cancelShown;
    private boolean cancelBusy;

    private boolean showCancel() {
        return state.inGame() && (state.transformed || android.os.SystemClock.uptimeMillis() - crystalsDrainedAt < 1500);
    }

    private void cancelAbility() {
        if (cancelBusy || running != null || state.paused) {
            return;
        }
        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        cancelBusy = true;
        invalidate();
        final int z = TouchInput.N64_Z, c = TouchInput.N64_C_LEFT;
        TouchInput.setN64Buttons(z);
        handler.postDelayed(() -> TouchInput.setN64Buttons(z | c), Z_LEAD_MS);
        handler.postDelayed(() -> TouchInput.setN64Buttons(z), Z_LEAD_MS + C_HOLD_MS);
        handler.postDelayed(() -> {
            TouchInput.setN64Buttons(0);
            crystalsDrainedAt = 0;
        }, Z_LEAD_MS + C_HOLD_MS + Z_TAIL_MS);
        handler.postDelayed(() -> {
            cancelBusy = false;
            invalidate();
        }, Z_LEAD_MS + C_HOLD_MS + Z_TAIL_MS + 400);
    }

    /** A round red badge with the Kong's face and a cross: ends the barrel ability. */
    private void drawCancelButton(Canvas c) {
        boolean enabled = !state.paused;
        float r = osR * 0.92f;
        float cy = osY - osR * 0.08f;
        int saved = c.saveLayerAlpha(osX - osR * 1.1f, osY - osR * 1.1f, osX + osR * 1.1f, osY + osR * 1.1f, enabled ? 255 : 90);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(OUTLINE);
        c.drawCircle(osX, cy, r, paint);
        int top = cancelBusy ? Color.rgb(255, 140, 120) : Color.rgb(220, 70, 50);
        int bottom = cancelBusy ? Color.rgb(200, 60, 40) : Color.rgb(120, 20, 15);
        paint.setShader(new LinearGradient(0, cy - r, 0, cy + r, top, bottom, Shader.TileMode.CLAMP));
        c.drawCircle(osX, cy, r * 0.87f, paint);
        paint.setShader(null);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * 0.006f);
        paint.setColor(cancelBusy ? BANANA : Color.argb(140, 255, 255, 255));
        c.drawCircle(osX, cy, r * 0.87f, paint);
        paint.setStyle(Paint.Style.FILL);
        int kong = state.currentKong;
        if (kong < 0 || kong >= GameState.KONG_COUNT || !RomIcons.draw(c, RomIcons.face[kong], osX, cy, r * 0.55f)) {
            DkIcons.eye(c, osX, cy, r * 0.45f);
        }
        // Cross over the face.
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeCap(Paint.Cap.ROUND);
        float k = r * 0.42f;
        paint.setStrokeWidth(r * 0.2f);
        paint.setColor(OUTLINE);
        c.drawLine(osX - k, cy - k, osX + k, cy + k, paint);
        c.drawLine(osX - k, cy + k, osX + k, cy - k, paint);
        paint.setStrokeWidth(r * 0.12f);
        paint.setColor(CREAM);
        c.drawLine(osX - k, cy - k, osX + k, cy + k, paint);
        c.drawLine(osX - k, cy + k, osX + k, cy - k, paint);
        paint.setStrokeCap(Paint.Cap.BUTT);
        paint.setStyle(Paint.Style.FILL);
        tmp.set(osX - osR * 0.95f, osY + osR * 0.58f, osX + osR * 0.95f, osY + osR * 0.98f);
        tagPlate(c, tmp);
        text.setTextAlign(Paint.Align.CENTER);
        text.setTextSize(fitText("Cancel", tmp.width() * 0.8f, tmp.height() * 0.66f));
        outlined(c, "Cancel", tmp.centerX(), tmp.centerY() + text.getTextSize() * 0.36f, CREAM);
        c.restoreToCount(saved);
    }

    /** A round badge like the first-person one: Lanky's face, upside down. */
    private void drawOrangstandButton(Canvas c) {
        boolean pressed = orangstand || orangstandBusy;
        boolean enabled = !state.paused && (orangstand || (!state.swimming && !state.climbing && !state.gunOut));
        float r = osR * 0.92f;
        float cy = osY - osR * 0.08f;
        int saved = c.saveLayerAlpha(osX - osR * 1.1f, osY - osR * 1.1f, osX + osR * 1.1f, osY + osR * 1.1f, enabled ? 255 : 90);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(OUTLINE);
        c.drawCircle(osX, cy, r, paint);
        int top = pressed ? Color.rgb(250, 200, 90) : Color.rgb(200, 130, 50);
        int bottom = pressed ? Color.rgb(200, 120, 30) : Color.rgb(120, 70, 20);
        paint.setShader(new LinearGradient(0, cy - r, 0, cy + r, top, bottom, Shader.TileMode.CLAMP));
        c.drawCircle(osX, cy, r * 0.87f, paint);
        paint.setShader(null);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * 0.006f);
        paint.setColor(pressed ? BANANA : Color.argb(140, 255, 255, 255));
        c.drawCircle(osX, cy, r * 0.87f, paint);
        paint.setStyle(Paint.Style.FILL);
        c.save();
        c.rotate(180f, osX, cy);
        if (!RomIcons.draw(c, RomIcons.face[GameState.LANKY], osX, cy, r * 0.62f)) {
            DkIcons.eye(c, osX, cy, r * 0.5f);
        }
        c.restore();
        tmp.set(osX - osR * 0.95f, osY + osR * 0.58f, osX + osR * 0.95f, osY + osR * 0.98f);
        tagPlate(c, tmp);
        text.setTextAlign(Paint.Align.CENTER);
        text.setTextSize(fitText("Orangstand", tmp.width() * 0.86f, tmp.height() * 0.66f));
        outlined(c, "Orangstand", tmp.centerX(), tmp.centerY() + text.getTextSize() * 0.36f, CREAM);
        c.restoreToCount(saved);
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

    /**
     * Whether the game takes this move right now. When it doesn't only because of what the
     * Kong is doing for a moment (jumping, attacking, rolling...), the button looks normal
     * but does nothing; see {@link #greyed} for when it's greyed out.
     */
    private boolean usableNow(Move move) {
        if (running == move) {
            return true;
        }
        switch (move) {
            case FIRST_PERSON: return state.canFirstPerson || state.firstPerson || state.gunOut;
            case WEAPON: return state.canZMoves || state.gunOut;
            default: return state.canZMoves && !state.gunOut;
        }
    }

    /**
     * Greyed out: nothing to use it with (no ammo, oranges, film or instrument energy),
     * swimming, or (for the other moves) while the gun is out. The gun button stays lit while
     * the gun is out so it can always be put away, even with no ammo left.
     */
    private boolean greyed(Move move) {
        if (!state.inGame() || running == move) {
            return false;
        }
        if (state.paused) {
            return true;   // nothing works while the pause menu is open
        }
        GameState.Kong k = state.kongs[state.currentKong];
        switch (move) {
            case FIRST_PERSON: return state.climbing;
            case WEAPON:
                if (state.gunOut) {
                    return false;
                }
                return state.swimming || state.climbing || (state.ammo <= 0 && state.homingAmmo <= 0);
            case ORANGE: return state.swimming || state.climbing || state.gunOut || state.oranges <= 0;
            case INSTRUMENT: return state.swimming || state.climbing || state.gunOut || k.instrumentEnergy <= 0;
            case CAMERA: return state.swimming || state.climbing || state.gunOut || state.film <= 0;
            default: return false;
        }
    }

    /** A tap on this button does something now. */
    private boolean available(Move move) {
        return !state.inGame() || (!greyed(move) && usableNow(move));
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

        // Lanky's Orangstand toggle, at the right end of the top row, after up to 3 melons.
        float healthRight = healthX + 2 * melonR * 2.4f + melonR * 1.1f;
        osR = Math.max(fpR * 0.5f, Math.min(fpR * 0.8f, (right - healthRight - m * 0.4f) / 2f));
        osX = right - osR;
        osY = fpY;

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
        if (!state.inGame()) {
            drawTitle(canvas, getWidth(), getHeight());
            return;
        }
        drawFirstPersonButton(canvas);
        drawHealth(canvas);
        if (showCancel()) {
            drawCancelButton(canvas);
        } else if (showOrangstand()) {
            drawOrangstandButton(canvas);
        } else if (zbMoveName() != null) {
            drawZbMoveButton(canvas);
        }
        drawTabs(canvas);
        drawKongPanel(canvas);
        drawLocationBar(canvas);
        drawActions(canvas);
        if (state.paused) {
            drawPausedTag(canvas);
        }
    }

    /** A wooden "Paused" tag across the (dimmed) action buttons. */
    private void drawPausedTag(Canvas c) {
        RectF first = actionRects[0], last = actionRects[actionRects.length - 1];
        float cy = (first.top + last.bottom) / 2f;
        float h = unit * 0.11f;
        tmp.set(first.left - unit * 0.01f, cy - h / 2f, first.right + unit * 0.01f, cy + h / 2f);
        tagPlate(c, tmp);
        text.setTextAlign(Paint.Align.CENTER);
        text.setTextSize(fitText("Paused", tmp.width() * 0.8f, tmp.height() * 0.62f));
        outlined(c, "Paused", tmp.centerX(), tmp.centerY() + text.getTextSize() * 0.36f, CREAM);
    }

    /**
     * Shown until a save is loaded: just the game's name over the jungle. (Our own lettering;
     * the official logo artwork isn't something this app ships.)
     */
    private void drawTitle(Canvas c, int w, int h) {
        Bitmap logo = logo();
        if (logo != null) {
            float scale = Math.min(w * 0.78f / logo.getWidth(), h * 0.62f / logo.getHeight());
            float lw = logo.getWidth() * scale, lh = logo.getHeight() * scale;
            tmp.set((w - lw) / 2, (h - lh) / 2, (w + lw) / 2, (h + lh) / 2);
            c.drawBitmap(logo, null, tmp, bitmapPaint);
        }
        // Build version, small, for bug reports.
        if (version == null) {
            version = getContext() != null ? Diagnostics.version(getContext()) : "";
        }
        if (!version.isEmpty()) {
            text.setTextAlign(Paint.Align.CENTER);
            text.setTextSize(unit * 0.035f);
            outlined(c, "v" + version, w / 2f, h - unit * 0.04f, CREAM);
        }
    }

    private String version;

    /** The app's logo, from the APK's assets (the same one the launcher shows). */
    private Bitmap logo() {
        if (logo == null && !logoTried) {
            logoTried = true;
            try (java.io.InputStream in = getContext().getAssets().open("game/assets/Logo.png")) {
                logo = android.graphics.BitmapFactory.decodeStream(in);
            } catch (Exception | LinkageError e) {
                logo = null;
            }
        }
        return logo;
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

    /**
     * First-person view: a round badge (like the item badges) with the game's aiming
     * crosshair, and a small wooden tag naming it.
     */
    private void drawFirstPersonButton(Canvas c) {
        boolean pressed = running == Move.FIRST_PERSON || state.firstPerson;
        boolean enabled = !greyed(Move.FIRST_PERSON);
        int saved = c.saveLayerAlpha(fpX - fpR * 1.1f, fpY - fpR * 1.1f, fpX + fpR * 1.1f, fpY + fpR * 1.1f, enabled ? 255 : 90);
        float r = fpR * 0.92f;
        float cy = fpY - fpR * 0.08f;
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(OUTLINE);
        c.drawCircle(fpX, cy, r, paint);
        int top = pressed ? Color.rgb(120, 200, 230) : Color.rgb(70, 150, 190);
        int bottom = pressed ? Color.rgb(40, 120, 160) : Color.rgb(24, 80, 120);
        paint.setShader(new LinearGradient(0, cy - r, 0, cy + r, top, bottom, Shader.TileMode.CLAMP));
        c.drawCircle(fpX, cy, r * 0.87f, paint);
        paint.setShader(null);
        paint.setColor(Color.argb(60, 255, 255, 255));
        tmp2.set(fpX - r * 0.55f, cy - r * 0.8f, fpX + r * 0.55f, cy - r * 0.12f);
        c.drawOval(tmp2, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * 0.006f);
        paint.setColor(pressed ? BANANA : Color.argb(140, 255, 255, 255));
        c.drawCircle(fpX, cy, r * 0.87f, paint);
        paint.setStyle(Paint.Style.FILL);
        if (!RomIcons.draw(c, RomIcons.crosshair, fpX, cy, r * 0.62f)) {
            DkIcons.eye(c, fpX, cy, r * 0.5f);
        }
        // Name tag across the bottom.
        tmp.set(fpX - fpR * 0.78f, fpY + fpR * 0.58f, fpX + fpR * 0.78f, fpY + fpR * 0.98f);
        tagPlate(c, tmp);
        text.setTextAlign(Paint.Align.CENTER);
        text.setTextSize(fitText("1st Person", tmp.width() * 0.84f, tmp.height() * 0.66f));
        outlined(c, "1st Person", tmp.centerX(), tmp.centerY() + text.getTextSize() * 0.36f, CREAM);
        c.restoreToCount(saved);
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
        int color = Color.rgb(90, 160, 60);

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

    private static Row colored(Row r, int color) {
        r.color = color;
        return r;
    }

    private java.util.List<Row> rowsFor(int tab, int level) {
        java.util.List<Row> rows = new java.util.ArrayList<>();
        boolean all = level < 0;
        if (tab == SHARED_TAB) {
            int[][][] lists = { GameState.FAIRY_FLAGS, GameState.CROWN_FLAGS, GameState.KEY_FLAGS, GameState.RAINBOW_COIN_FLAGS };
            int[] colors = { Color.rgb(226, 90, 170), Color.rgb(200, 60, 50), Color.rgb(70, 110, 200), Color.rgb(130, 70, 200) };
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
                rows.add(colored(new Row(labels[i], icons[i], 2, have, total), colors[i]));
            }
            if (all) {
                int golden = 0;
                for (GameState.Kong k : state.kongs) {
                    golden += k.goldenBananas;
                }
                rows.add(0, colored(new Row("Golden Bananas", RomIcons.goldenBanana, 0, golden, 201), Color.rgb(60, 150, 60)));
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
        rows.add(colored(new Row("Golden Bananas", RomIcons.goldenBanana, 0, golden, goldenTotal), Color.rgb(60, 150, 60)));
        rows.add(colored(new Row("Bananas", RomIcons.of(RomIcons.banana, tab), 0, coloured, colouredTotal), KONG_COLORS[tab]));
        rows.add(colored(new Row("Blueprints", RomIcons.blueprint, 2, prints, printsTotal), Color.rgb(60, 120, 200)));
        rows.add(colored(new Row("Banana Medals", RomIcons.of(RomIcons.medalByKong, tab), 2, medals, medalsTotal), darker(KONG_COLORS[tab])));
        rows.add(colored(new Row("Coins", RomIcons.of(RomIcons.coin, tab), 1, kong.coins, -1), darker(KONG_COLORS[tab])));
        return rows;
    }

    /**
     * Collectibles as a grid of round badges with wooden value tags, in the style of DKC
     * Returns 3D's item screen: a wooden frame around a dark inset board.
     */
    private void drawKongPanel(Canvas c) {
        RectF p = panelRect;
        woodPanel(c, p, unit * 0.03f, false);

        int tab = displayedKong();
        java.util.List<Row> rows = rowsFor(tab, shownLevel());
        rows.removeIf(r -> r.total == 0);   // nothing of that kind here (e.g. bananas in Helm)

        // Dark inset board.
        float frame = unit * 0.035f;
        RectF board = boardRect;
        board.set(p.left + frame, p.top + frame, p.right - frame, p.bottom - frame);
        float br = unit * 0.02f;
        paint.setStyle(Paint.Style.FILL);
        paint.setShader(new LinearGradient(0, board.top, 0, board.bottom, Color.rgb(46, 26, 12), Color.rgb(70, 40, 18), Shader.TileMode.CLAMP));
        c.drawRoundRect(board, br, br, paint);
        paint.setShader(null);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * 0.008f);
        paint.setColor(Color.rgb(30, 16, 6));
        c.drawRoundRect(board, br, br, paint);
        paint.setStyle(Paint.Style.FILL);

        int cols = 2;
        int lines = (rows.size() + cols - 1) / cols;
        float cellW = board.width() / cols;
        float cellH = board.height() / Math.max(lines, 1);
        float badgeR = Math.min(cellH * 0.34f, cellW * 0.17f);
        for (int i = 0; i < rows.size(); i++) {
            Row r = rows.get(i);
            float cx = board.left + (i % cols) * cellW, cy = board.top + (i / cols) * cellH + cellH / 2;
            float bx = cx + cellW * 0.08f + badgeR;
            String value = r.total < 0 ? String.valueOf(r.have) : r.have + "/" + r.total;
            boolean complete = r.total > 0 && r.have >= r.total;
            // Tag first, so the badge overlaps its left end.
            tmp.set(bx + badgeR * 0.4f, cy - badgeR * 0.55f, cx + cellW * 0.93f, cy + badgeR * 0.55f);
            tag(c, tmp, value, complete);
            badge(c, bx, cy, badgeR, r);
        }
    }

    /** A round coloured badge with the item's icon. */
    private void badge(Canvas c, float x, float y, float r, Row row) {
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(OUTLINE);
        c.drawCircle(x, y, r, paint);
        int base = row.color;
        paint.setShader(new LinearGradient(0, y - r, 0, y + r, lighten(base), base, Shader.TileMode.CLAMP));
        c.drawCircle(x, y, r * 0.86f, paint);
        paint.setShader(null);
        // Gloss.
        paint.setColor(Color.argb(70, 255, 255, 255));
        tmp2.set(x - r * 0.55f, y - r * 0.78f, x + r * 0.55f, y - r * 0.1f);
        c.drawOval(tmp2, paint);
        if (!RomIcons.draw(c, row.icon, x, y, r * 0.62f)) {
            switch (row.fallback) {
                case 0: DkIcons.banana(c, x, y, r * 0.55f); break;
                case 1: DkIcons.coin(c, x, y, r * 0.5f); break;
                default: DkIcons.star(c, x, y, r * 0.55f); break;
            }
        }
    }

    /** An orange wooden plate (the item tags and the first-person label). */
    private void tagPlate(Canvas c, RectF r) {
        float rad = r.height() * 0.3f;
        paint.setStyle(Paint.Style.FILL);
        paint.setShader(new LinearGradient(0, r.top, 0, r.bottom, Color.rgb(240, 150, 50), Color.rgb(184, 96, 26), Shader.TileMode.CLAMP));
        c.drawRoundRect(r, rad, rad, paint);
        paint.setShader(null);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * 0.006f);
        paint.setColor(OUTLINE);
        c.drawRoundRect(r, rad, rad, paint);
        paint.setStyle(Paint.Style.FILL);
    }

    /** An orange wooden tag with a value on it. */
    private void tag(Canvas c, RectF r, String value, boolean complete) {
        float rad = r.height() * 0.3f;
        paint.setStyle(Paint.Style.FILL);
        paint.setShader(new LinearGradient(0, r.top, 0, r.bottom, Color.rgb(240, 150, 50), Color.rgb(184, 96, 26), Shader.TileMode.CLAMP));
        c.drawRoundRect(r, rad, rad, paint);
        paint.setShader(null);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * 0.0025f);
        paint.setColor(Color.argb(70, 90, 40, 6));
        c.drawLine(r.left + rad, r.top + r.height() * 0.3f, r.right - rad, r.top + r.height() * 0.34f, paint);
        c.drawLine(r.left + rad * 2, r.top + r.height() * 0.72f, r.right - rad * 1.5f, r.top + r.height() * 0.7f, paint);
        paint.setStrokeWidth(unit * 0.007f);
        paint.setColor(OUTLINE);
        c.drawRoundRect(r, rad, rad, paint);
        paint.setStyle(Paint.Style.FILL);
        text.setTextAlign(Paint.Align.CENTER);
        float textLeft = r.left + r.height() * 0.55f;   // clear of the badge
        text.setTextSize(fitText(value, r.right - textLeft - rad, r.height() * 0.68f));
        outlined(c, value, (textLeft + r.right) / 2, r.centerY() + text.getTextSize() * 0.36f, complete ? COMPLETE : CREAM);
    }

    private static int darker(int color) {
        return Color.rgb(((color >> 16) & 0xFF) * 3 / 5, ((color >> 8) & 0xFF) * 3 / 5, (color & 0xFF) * 3 / 5);
    }

    private static int lighten(int color) {
        int r = Math.min(255, ((color >> 16) & 0xFF) + 70);
        int g = Math.min(255, ((color >> 8) & 0xFF) + 70);
        int b = Math.min(255, (color & 0xFF) + 70);
        return Color.rgb(r, g, b);
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
        String[] counts = new String[4];
        for (int i = 0; i < 4; i++) {
            int n;
            switch (ACTIONS[i]) {
                case ORANGE: n = state.oranges; break;
                case WEAPON: n = state.ammo; break;
                case INSTRUMENT: n = k.instrumentEnergy; break;
                default: n = state.film; break;
            }
            counts[i] = known ? String.valueOf(n) : "";
        }

        for (int i = 0; i < 4; i++) {
            Move move = ACTIONS[i];
            RectF r = actionRects[i];
            if (!owned(move)) {
                // Not bought yet: just an empty notch in the wood.
                paint.setColor(Color.argb(70, 0, 0, 0));
                c.drawRoundRect(r, r.height() * 0.18f, r.height() * 0.18f, paint);
                continue;
            }
            // The weapon button stays lit while the Kong has the gun out.
            boolean pressed = running == move || (move == Move.WEAPON && state.gunOut);
            boolean enabled = !greyed(move);

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
        if (!active || !state.inGame()) {
            return false;
        }
        int action = event.getActionMasked();
        if (action != MotionEvent.ACTION_DOWN && action != MotionEvent.ACTION_POINTER_DOWN) {
            return true;
        }
        int index = event.getActionIndex();
        float x = event.getX(index), y = event.getY(index);

        float ox = x - osX, oy = y - osY;
        if (showCancel() && ox * ox + oy * oy <= osR * osR * 1.3f) {
            cancelAbility();
            return true;
        }
        if (showOrangstand() && ox * ox + oy * oy <= osR * osR * 1.3f) {
            toggleOrangstand();
            return true;
        }
        if (zbMoveName() != null && ox * ox + oy * oy <= osR * osR * 1.3f) {
            performZbMove();
            return true;
        }
        float dx = x - fpX, dy = y - fpY;
        if (dx * dx + dy * dy <= fpR * fpR * 1.3f) {
            if (available(Move.FIRST_PERSON)) {
                perform(Move.FIRST_PERSON);
            }
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
                boolean current = state.inGame() && i == state.currentKong;
                if (i < GameState.KONG_COUNT && trySwap(i, current)) {
                    return true;
                }
                // Tapping the Kong you're playing (or the one already shown) goes back to
                // following whoever you play as.
                selectedKong = (current || i == selectedKong) ? -1 : i;
                performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
                invalidate();
                return true;
            }
        }
        return true;
    }
}
