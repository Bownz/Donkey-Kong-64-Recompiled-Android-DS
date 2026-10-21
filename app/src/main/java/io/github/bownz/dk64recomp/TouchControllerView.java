package io.github.bownz.dk64recomp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.SparseArray;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * On-screen N64 controller. Draws the controls and turns multitouch input into SDL
 * virtual-gamepad state through {@link TouchInput}.
 *
 * The layout is defined in units of the view's shorter side, anchored to its edges, so
 * the same controller works as an overlay on a phone in landscape and full-size on a
 * handheld's second screen.
 *
 * Touches that don't start on a control return false, so in overlay mode they fall
 * through to the SDL surface underneath (the launcher and menus accept touch input).
 */
public final class TouchControllerView extends View {

    /** Where a control is anchored; offsets are measured inward from that edge. */
    private enum AnchorX { LEFT, CENTER, RIGHT }
    private enum AnchorY { TOP, BOTTOM }

    private enum Kind { BUTTON, STICK }

    /** One on-screen control. Positions and radius are in units of the view's shorter side. */
    private static final class Control {
        final Kind kind;
        final String label;
        final int color;
        final AnchorX anchorX;
        final float offsetX;
        final AnchorY anchorY;
        final float offsetY;
        final float radius;
        final Runnable onPress;
        final Runnable onRelease;

        // Resolved on layout, in pixels.
        float cx, cy, r;

        Control(Kind kind, String label, int color, AnchorX ax, float ox, AnchorY ay, float oy,
                float radius, Runnable onPress, Runnable onRelease) {
            this.kind = kind;
            this.label = label;
            this.color = color;
            this.anchorX = ax;
            this.offsetX = ox;
            this.anchorY = ay;
            this.offsetY = oy;
            this.radius = radius;
            this.onPress = onPress;
            this.onRelease = onRelease;
        }

        boolean hit(float x, float y, float slack) {
            float dx = x - cx, dy = y - cy, reach = r * slack;
            return dx * dx + dy * dy <= reach * reach;
        }
    }

    // Touch targets are a little larger than the drawn circles, and the stick accepts
    // touches well outside its ring so the thumb doesn't have to land precisely.
    private static final float BUTTON_SLACK = 1.25f;
    private static final float STICK_CAPTURE_SLACK = 1.5f;
    private static final float MIN_LAYOUT_ASPECT = 1.4f;

    private static final int COLOR_A = Color.rgb(59, 111, 216);
    private static final int COLOR_B = Color.rgb(46, 158, 79);
    private static final int COLOR_C = Color.rgb(232, 194, 27);
    private static final int COLOR_START = Color.rgb(200, 55, 55);
    private static final int COLOR_NEUTRAL = Color.rgb(120, 120, 128);

    private final List<Control> controls = new ArrayList<>();
    private final Control stick;

    // C-buttons and Z/R are analog on the virtual pad; track their state to compose axes.
    private boolean cUp, cDown, cLeft, cRight;

    // Pointer bookkeeping: which pointer drives the stick, and which pointers press buttons.
    private int stickPointerId = -1;
    private float stickX, stickY; // -1..1
    private final SparseArray<float[]> buttonPointers = new SparseArray<>();
    private final Set<Control> pressed = new HashSet<>();

    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);

    private float opacity = 0.55f;
    private boolean active = false;

    public TouchControllerView(Context context) {
        super(context);

        stroke.setStyle(Paint.Style.STROKE);
        stroke.setColor(Color.WHITE);
        text.setColor(Color.WHITE);
        text.setTextAlign(Paint.Align.CENTER);
        text.setFakeBoldText(true);

        // Layout: three columns. Left: Z, D-pad, analog stick. Right: R, C-buttons, B, A.
        // Centre: menu and Start. Units are the view's shorter side; the columns stay clear
        // of each other (and of each other's enlarged touch targets) from 4:3 to 21:9.

        // ---- Left column ----
        controls.add(axisButton("Z", COLOR_NEUTRAL, AnchorX.LEFT, 0.13f, AnchorY.TOP, 0.13f, 0.085f, TouchInput.AXIS_TRIGGER_LEFT));
        float dpx = 0.30f, dpy = 0.70f, dpOff = 0.075f, dpR = 0.042f;
        controls.add(button("▲", COLOR_NEUTRAL, AnchorX.LEFT, dpx, AnchorY.BOTTOM, dpy + dpOff, dpR, TouchInput.BUTTON_DPAD_UP));
        controls.add(button("▼", COLOR_NEUTRAL, AnchorX.LEFT, dpx, AnchorY.BOTTOM, dpy - dpOff, dpR, TouchInput.BUTTON_DPAD_DOWN));
        controls.add(button("◀", COLOR_NEUTRAL, AnchorX.LEFT, dpx - dpOff, AnchorY.BOTTOM, dpy, dpR, TouchInput.BUTTON_DPAD_LEFT));
        controls.add(button("▶", COLOR_NEUTRAL, AnchorX.LEFT, dpx + dpOff, AnchorY.BOTTOM, dpy, dpR, TouchInput.BUTTON_DPAD_RIGHT));
        stick = new Control(Kind.STICK, "", COLOR_NEUTRAL, AnchorX.LEFT, 0.27f, AnchorY.BOTTOM, 0.27f, 0.16f, null, null);
        controls.add(stick);

        // ---- Right column ----
        controls.add(axisButton("R", COLOR_NEUTRAL, AnchorX.RIGHT, 0.13f, AnchorY.TOP, 0.13f, 0.085f, TouchInput.AXIS_TRIGGER_RIGHT));
        float cx = 0.30f, cy = 0.70f, cOff = 0.105f, cR = 0.056f;
        controls.add(cButton("C▲", AnchorX.RIGHT, cx, AnchorY.BOTTOM, cy + cOff, cR, () -> cUp = true, () -> cUp = false));
        controls.add(cButton("C▼", AnchorX.RIGHT, cx, AnchorY.BOTTOM, cy - cOff, cR, () -> cDown = true, () -> cDown = false));
        controls.add(cButton("C◀", AnchorX.RIGHT, cx + cOff, AnchorY.BOTTOM, cy, cR, () -> cLeft = true, () -> cLeft = false));
        controls.add(cButton("C▶", AnchorX.RIGHT, cx - cOff, AnchorY.BOTTOM, cy, cR, () -> cRight = true, () -> cRight = false));
        controls.add(button("B", COLOR_B, AnchorX.RIGHT, 0.40f, AnchorY.BOTTOM, 0.32f, 0.078f, TouchInput.BUTTON_WEST));
        controls.add(button("A", COLOR_A, AnchorX.RIGHT, 0.20f, AnchorY.BOTTOM, 0.20f, 0.09f, TouchInput.BUTTON_SOUTH));

        // ---- Centre ----
        controls.add(button("☰", COLOR_NEUTRAL, AnchorX.CENTER, 0f, AnchorY.TOP, 0.08f, 0.045f, TouchInput.BUTTON_BACK));
        controls.add(button("START", COLOR_START, AnchorX.CENTER, 0f, AnchorY.BOTTOM, 0.10f, 0.06f, TouchInput.BUTTON_START));

        setContentDescription("On-screen N64 controller");
        setHapticFeedbackEnabled(true);
        setVisibility(GONE);
    }

    private Control button(String label, int color, AnchorX ax, float ox, AnchorY ay, float oy, float radius, int sdlButton) {
        return new Control(Kind.BUTTON, label, color, ax, ox, ay, oy, radius,
            () -> TouchInput.setButton(sdlButton, true),
            () -> TouchInput.setButton(sdlButton, false));
    }

    private Control axisButton(String label, int color, AnchorX ax, float ox, AnchorY ay, float oy, float radius, int sdlAxis) {
        return new Control(Kind.BUTTON, label, color, ax, ox, ay, oy, radius,
            () -> TouchInput.setAxis(sdlAxis, 1f),
            () -> TouchInput.setAxis(sdlAxis, 0f));
    }

    private Control cButton(String label, AnchorX ax, float ox, AnchorY ay, float oy, float radius, Runnable set, Runnable clear) {
        return new Control(Kind.BUTTON, label, COLOR_C, ax, ox, ay, oy, radius,
            () -> { set.run(); sendCAxes(); },
            () -> { clear.run(); sendCAxes(); });
    }

    private void sendCAxes() {
        TouchInput.setAxis(TouchInput.AXIS_RIGHT_X, (cRight ? 1f : 0f) - (cLeft ? 1f : 0f));
        TouchInput.setAxis(TouchInput.AXIS_RIGHT_Y, (cDown ? 1f : 0f) - (cUp ? 1f : 0f));
    }

    // ------------------------------------------------------------------------------
    // Public control
    // ------------------------------------------------------------------------------

    /** Shows the controller and creates the virtual gamepad, or hides and removes both. */
    public void setActive(boolean active) {
        if (this.active == active) {
            return;
        }
        this.active = active;
        if (!active) {
            releaseAll();
        }
        TouchInput.setEnabled(active);
        setVisibility(active ? VISIBLE : GONE);
    }

    public boolean isActive() {
        return active;
    }

    /** 0..1. Lower when drawn over the game, full when it has a screen to itself. */
    public void setOpacity(float opacity) {
        this.opacity = Math.max(0.1f, Math.min(1f, opacity));
        invalidate();
    }

    /** Lifts every touch: call when the activity pauses so nothing stays held. */
    public void releaseAll() {
        for (Control c : new ArrayList<>(pressed)) {
            c.onRelease.run();
        }
        pressed.clear();
        buttonPointers.clear();
        if (stickPointerId != -1 || stickX != 0f || stickY != 0f) {
            stickPointerId = -1;
            setStick(0f, 0f);
        }
        invalidate();
    }

    // ------------------------------------------------------------------------------
    // Layout and drawing
    // ------------------------------------------------------------------------------

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        // Size controls from the shorter side, but never wider than a 1.4:1 working area,
        // so the left and right columns stay apart on square or portrait screens (such as
        // a dual-screen handheld's near-square lower display).
        float unit = Math.min(Math.min(w, h), w / MIN_LAYOUT_ASPECT);
        for (Control c : controls) {
            c.r = c.radius * unit;
            switch (c.anchorX) {
                case LEFT: c.cx = c.offsetX * unit; break;
                case RIGHT: c.cx = w - c.offsetX * unit; break;
                default: c.cx = w / 2f + c.offsetX * unit; break;
            }
            c.cy = (c.anchorY == AnchorY.TOP) ? c.offsetY * unit : h - c.offsetY * unit;
        }
        stroke.setStrokeWidth(Math.max(2f, unit * 0.006f));
    }

    @Override
    protected void onDraw(Canvas canvas) {
        int baseAlpha = Math.round(255 * opacity);
        for (Control c : controls) {
            if (c.kind == Kind.STICK) {
                drawStick(canvas, c, baseAlpha);
                continue;
            }
            boolean down = pressed.contains(c);
            fill.setColor(c.color);
            fill.setAlpha(down ? Math.min(255, baseAlpha + 90) : baseAlpha / 2);
            canvas.drawCircle(c.cx, c.cy, c.r, fill);
            stroke.setAlpha(baseAlpha);
            canvas.drawCircle(c.cx, c.cy, c.r, stroke);

            text.setAlpha(Math.min(255, baseAlpha + 60));
            text.setTextSize(c.r * (c.label.length() > 2 ? 0.55f : 0.8f));
            float baseline = c.cy - (text.descent() + text.ascent()) / 2f;
            canvas.drawText(c.label, c.cx, baseline, text);
        }
    }

    private void drawStick(Canvas canvas, Control c, int baseAlpha) {
        fill.setColor(Color.BLACK);
        fill.setAlpha(baseAlpha / 3);
        canvas.drawCircle(c.cx, c.cy, c.r, fill);
        stroke.setAlpha(baseAlpha);
        canvas.drawCircle(c.cx, c.cy, c.r, stroke);

        float knobX = c.cx + stickX * c.r, knobY = c.cy + stickY * c.r;
        fill.setColor(COLOR_NEUTRAL);
        fill.setAlpha(stickPointerId != -1 ? Math.min(255, baseAlpha + 90) : baseAlpha);
        canvas.drawCircle(knobX, knobY, c.r * 0.45f, fill);
        canvas.drawCircle(knobX, knobY, c.r * 0.45f, stroke);
    }

    // ------------------------------------------------------------------------------
    // Touch handling
    // ------------------------------------------------------------------------------

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (!active) {
            return false;
        }

        final int action = event.getActionMasked();
        final int index = event.getActionIndex();
        final int pointerId = event.getPointerId(index);

        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                float x = event.getX(index), y = event.getY(index);
                if (stickPointerId == -1 && stick.hit(x, y, STICK_CAPTURE_SLACK)) {
                    stickPointerId = pointerId;
                    updateStick(x, y);
                } else if (hitButton(x, y) != null) {
                    buttonPointers.put(pointerId, new float[] { x, y });
                } else if (action == MotionEvent.ACTION_DOWN) {
                    // Not on a control: let the game's own UI underneath have it.
                    return false;
                }
                // The gamepad can only be created once SDL is running; retry lazily.
                TouchInput.setEnabled(true);
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                for (int i = 0; i < event.getPointerCount(); i++) {
                    int id = event.getPointerId(i);
                    if (id == stickPointerId) {
                        updateStick(event.getX(i), event.getY(i));
                    } else {
                        float[] pos = buttonPointers.get(id);
                        if (pos != null) {
                            pos[0] = event.getX(i);
                            pos[1] = event.getY(i);
                        }
                    }
                }
                break;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_CANCEL: {
                if (action == MotionEvent.ACTION_CANCEL || action == MotionEvent.ACTION_UP) {
                    buttonPointers.clear();
                    if (stickPointerId != -1) {
                        stickPointerId = -1;
                        setStick(0f, 0f);
                    }
                } else if (pointerId == stickPointerId) {
                    stickPointerId = -1;
                    setStick(0f, 0f);
                } else {
                    buttonPointers.remove(pointerId);
                }
                break;
            }
            default:
                return true;
        }

        updatePressedButtons();
        invalidate();
        return true;
    }

    private Control hitButton(float x, float y) {
        for (Control c : controls) {
            if (c.kind == Kind.BUTTON && c.hit(x, y, BUTTON_SLACK)) {
                return c;
            }
        }
        return null;
    }

    /** Recomputes which buttons are held from all button pointers, so fingers can slide between them. */
    private void updatePressedButtons() {
        Set<Control> now = new HashSet<>();
        for (int i = 0; i < buttonPointers.size(); i++) {
            float[] pos = buttonPointers.valueAt(i);
            Control c = hitButton(pos[0], pos[1]);
            if (c != null) {
                now.add(c);
            }
        }

        boolean newPress = false;
        for (Control c : new ArrayList<>(pressed)) {
            if (!now.contains(c)) {
                c.onRelease.run();
                pressed.remove(c);
            }
        }
        for (Control c : now) {
            if (pressed.add(c)) {
                c.onPress.run();
                newPress = true;
            }
        }
        if (newPress) {
            performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        }
    }

    private void updateStick(float x, float y) {
        float dx = (x - stick.cx) / stick.r, dy = (y - stick.cy) / stick.r;
        float length = (float) Math.sqrt(dx * dx + dy * dy);
        if (length > 1f) {
            dx /= length;
            dy /= length;
        }
        setStick(dx, dy);
    }

    private void setStick(float x, float y) {
        stickX = x;
        stickY = y;
        // Screen Y grows downward, as does SDL's stick Y; no flip needed.
        TouchInput.setAxis(TouchInput.AXIS_LEFT_X, x);
        TouchInput.setAxis(TouchInput.AXIS_LEFT_Y, y);
    }
}
