package io.github.bownz.dk64recomp;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Handler;
import android.os.Looper;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * On-screen N64 controller.
 *
 * The buttons go straight into the game as N64 buttons (TouchInput.setOverlayN64Button),
 * so they always do what their label says, whatever the controller bindings are. The sticks
 * drive the virtual SDL gamepad's sticks: the left one is the N64 stick, and the right one
 * (shown only with the Analog Camera) turns the camera.
 *
 * The default layout is in units of the view's shorter side, anchored to its edges. The
 * player can move and resize each group of controls ({@link #startEditing()}); the changes
 * are kept per camera mode (normal / Analog Camera) in SharedPreferences.
 *
 * The controller hides itself while a menu (the launcher, the menu bar, settings) takes
 * input, so touches reach the menu, and comes back when it closes.
 */
public final class TouchControllerView extends View {

    private enum AnchorX { LEFT, CENTER, RIGHT }
    private enum AnchorY { TOP, BOTTOM }
    private enum Kind { BUTTON, STICK }

    /** A default position: offsets from the anchored edges, in units of the shorter side. */
    private static final class Place {
        final AnchorX ax; final float ox; final AnchorY ay; final float oy; final float radius;
        Place(AnchorX ax, float ox, AnchorY ay, float oy, float radius) {
            this.ax = ax; this.ox = ox; this.ay = ay; this.oy = oy; this.radius = radius;
        }
    }

    private static final class Control {
        final Kind kind;
        final String group;
        final String label;
        final int color;
        final Place normal;   // null: not shown with the normal camera
        final Place analog;   // null: not shown with the Analog Camera
        final Runnable onPress, onRelease;
        // Sticks only.
        final int axisX, axisY;
        final float captureSlack;
        int pointerId = -1;
        float sx, sy;

        // Resolved on layout, in pixels.
        float cx, cy, r;
        boolean shown;

        Control(Kind kind, String group, String label, int color, Place normal, Place analog,
                Runnable onPress, Runnable onRelease, int axisX, int axisY, float captureSlack) {
            this.kind = kind; this.group = group; this.label = label; this.color = color;
            this.normal = normal; this.analog = analog;
            this.onPress = onPress; this.onRelease = onRelease;
            this.axisX = axisX; this.axisY = axisY; this.captureSlack = captureSlack;
        }

        boolean hit(float x, float y, float slack) {
            float dx = x - cx, dy = y - cy, reach = r * slack;
            return dx * dx + dy * dy <= reach * reach;
        }
    }

    /** How the player moved and resized a group: offset as a fraction of the view, and scale. */
    private static final class Adjust {
        float dx, dy, scale = 1f;
    }

    private static final float BUTTON_SLACK = 1.25f;
    private static final float MIN_LAYOUT_ASPECT = 1.4f;
    private static final float MIN_SCALE = 0.5f, MAX_SCALE = 2.5f;
    private static final String PREFS = "touch_layout";

    private static final int COLOR_A = Color.rgb(59, 111, 216);
    private static final int COLOR_B = Color.rgb(46, 158, 79);
    private static final int COLOR_C = Color.rgb(232, 194, 27);
    private static final int COLOR_START = Color.rgb(200, 55, 55);
    private static final int COLOR_NEUTRAL = Color.rgb(120, 120, 128);
    private static final int COLOR_SELECT = Color.rgb(255, 214, 64);

    private final List<Control> controls = new ArrayList<>();
    private final List<Control> sticks = new ArrayList<>();

    private final SparseArray<float[]> buttonPointers = new SparseArray<>();
    private final Set<Control> pressed = new HashSet<>();

    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);

    private float opacity = 0.55f;
    private boolean active = false;   // Touch controls wanted (setting / no controller)
    private boolean menuOpen = false; // A menu is taking input: hide and let touches through
    private boolean analogMode = false;
    private float unit = 1f;

    // Layout adjustments for the current camera mode, by group.
    private final Map<String, Adjust> adjust = new HashMap<>();

    // ---- Editing ----
    private boolean editing = false;
    private String selected;
    private int dragPointer = -1;
    private float dragLastX, dragLastY;
    private int pinchPointer = -1;
    private float pinchStartDist, pinchStartScale;
    private final RectF[] toolbar = { new RectF(), new RectF(), new RectF(), new RectF() };
    private static final String[] TOOLBAR_LABELS = { "−", "+", "Reset", "Done" };

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Runnable poll = new Runnable() {
        @Override
        public void run() {
            pollUiState();
            if (active || editing) {
                handler.postDelayed(this, 150);
            }
        }
    };

    public TouchControllerView(Context context) {
        super(context);

        stroke.setStyle(Paint.Style.STROKE);
        stroke.setColor(Color.WHITE);
        text.setColor(Color.WHITE);
        text.setTextAlign(Paint.Align.CENTER);
        text.setFakeBoldText(true);

        // Three columns. Left: Z, D-pad, stick. Right: R, C-buttons, B, A. Centre: menu, Start.
        // With the Analog Camera the C-buttons get smaller and move up, the camera stick
        // takes their place, and Start moves to the top beside the menu button.

        // ---- Left column ----
        Place z = new Place(AnchorX.LEFT, 0.13f, AnchorY.TOP, 0.13f, 0.085f);
        controls.add(button("z", "Z", COLOR_NEUTRAL, z, z, TouchInput.N64_Z));
        float dpx = 0.30f, dpy = 0.70f, dpOff = 0.075f, dpR = 0.042f;
        controls.add(button("dpad", "▲", COLOR_NEUTRAL, both(AnchorX.LEFT, dpx, AnchorY.BOTTOM, dpy + dpOff, dpR), TouchInput.N64_DPAD_UP));
        controls.add(button("dpad", "▼", COLOR_NEUTRAL, both(AnchorX.LEFT, dpx, AnchorY.BOTTOM, dpy - dpOff, dpR), TouchInput.N64_DPAD_DOWN));
        controls.add(button("dpad", "◀", COLOR_NEUTRAL, both(AnchorX.LEFT, dpx - dpOff, AnchorY.BOTTOM, dpy, dpR), TouchInput.N64_DPAD_LEFT));
        controls.add(button("dpad", "▶", COLOR_NEUTRAL, both(AnchorX.LEFT, dpx + dpOff, AnchorY.BOTTOM, dpy, dpR), TouchInput.N64_DPAD_RIGHT));
        Place ls = new Place(AnchorX.LEFT, 0.27f, AnchorY.BOTTOM, 0.27f, 0.16f);
        addStick(new Control(Kind.STICK, "stick", "", COLOR_NEUTRAL, ls, ls, null, null,
            TouchInput.AXIS_LEFT_X, TouchInput.AXIS_LEFT_Y, 1.5f));

        // ---- Right column ----
        Place r = new Place(AnchorX.RIGHT, 0.13f, AnchorY.TOP, 0.13f, 0.085f);
        controls.add(button("r", "R", COLOR_NEUTRAL, r, r, TouchInput.N64_R));

        float cx = 0.30f, cy = 0.70f, cOff = 0.105f, cR = 0.056f;          // normal camera
        float acx = 0.37f, acy = 0.31f, acOff = 0.072f, acR = 0.040f;      // Analog Camera (from the top)
        controls.add(button("c", "C▲", COLOR_C,
            new Place(AnchorX.RIGHT, cx, AnchorY.BOTTOM, cy + cOff, cR),
            new Place(AnchorX.RIGHT, acx, AnchorY.TOP, acy - acOff, acR), TouchInput.N64_C_UP));
        controls.add(button("c", "C▼", COLOR_C,
            new Place(AnchorX.RIGHT, cx, AnchorY.BOTTOM, cy - cOff, cR),
            new Place(AnchorX.RIGHT, acx, AnchorY.TOP, acy + acOff, acR), TouchInput.N64_C_DOWN));
        controls.add(button("c", "C◀", COLOR_C,
            new Place(AnchorX.RIGHT, cx + cOff, AnchorY.BOTTOM, cy, cR),
            new Place(AnchorX.RIGHT, acx + acOff, AnchorY.TOP, acy, acR), TouchInput.N64_C_LEFT));
        controls.add(button("c", "C▶", COLOR_C,
            new Place(AnchorX.RIGHT, cx - cOff, AnchorY.BOTTOM, cy, cR),
            new Place(AnchorX.RIGHT, acx - acOff, AnchorY.TOP, acy, acR), TouchInput.N64_C_RIGHT));
        controls.add(button("b", "B", COLOR_B, both(AnchorX.RIGHT, 0.40f, AnchorY.BOTTOM, 0.32f, 0.078f), TouchInput.N64_B));
        controls.add(button("a", "A", COLOR_A, both(AnchorX.RIGHT, 0.20f, AnchorY.BOTTOM, 0.20f, 0.09f), TouchInput.N64_A));
        addStick(new Control(Kind.STICK, "camera", "CAM", COLOR_NEUTRAL, null,
            new Place(AnchorX.RIGHT, 0.72f, AnchorY.BOTTOM, 0.30f, 0.13f), null, null,
            TouchInput.AXIS_RIGHT_X, TouchInput.AXIS_RIGHT_Y, 1.2f));

        // ---- Centre ----
        controls.add(sdlButton("menu", "☰", COLOR_NEUTRAL,
            new Place(AnchorX.CENTER, 0f, AnchorY.TOP, 0.08f, 0.045f),
            new Place(AnchorX.CENTER, -0.09f, AnchorY.TOP, 0.08f, 0.045f), TouchInput.BUTTON_BACK));
        controls.add(button("start", "START", COLOR_START,
            new Place(AnchorX.CENTER, 0f, AnchorY.BOTTOM, 0.10f, 0.06f),
            new Place(AnchorX.CENTER, 0.09f, AnchorY.TOP, 0.08f, 0.05f), TouchInput.N64_START));

        loadLayout();
        setContentDescription("On-screen N64 controller");
        setHapticFeedbackEnabled(true);
        setVisibility(GONE);
    }

    private static Place both(AnchorX ax, float ox, AnchorY ay, float oy, float radius) {
        return new Place(ax, ox, ay, oy, radius);
    }

    private Control button(String group, String label, int color, Place place, int n64Bit) {
        return button(group, label, color, place, place, n64Bit);
    }

    private Control button(String group, String label, int color, Place normal, Place analog, int n64Bit) {
        return new Control(Kind.BUTTON, group, label, color, normal, analog,
            () -> TouchInput.setOverlayN64Button(n64Bit, true),
            () -> TouchInput.setOverlayN64Button(n64Bit, false), 0, 0, 1f);
    }

    /** A button on the virtual SDL gamepad (the menu button: opens the game's menu). */
    private Control sdlButton(String group, String label, int color, Place normal, Place analog, int sdlButton) {
        return new Control(Kind.BUTTON, group, label, color, normal, analog,
            () -> TouchInput.setButton(sdlButton, true),
            () -> TouchInput.setButton(sdlButton, false), 0, 0, 1f);
    }

    private void addStick(Control c) {
        controls.add(c);
        sticks.add(c);
    }

    // ------------------------------------------------------------------------------
    // Public control
    // ------------------------------------------------------------------------------

    /** Turns the touch controls on (creating the virtual gamepad) or off. */
    public void setActive(boolean active) {
        if (this.active == active) {
            return;
        }
        this.active = active;
        if (!active) {
            releaseAll();
            TouchInput.release();
        } else {
            TouchInput.acquire();
            startPolling();
        }
        refreshVisibility();
    }

    public boolean isActive() {
        return active;
    }

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
        for (Control s : sticks) {
            if (s.pointerId != -1 || s.sx != 0f || s.sy != 0f) {
                s.pointerId = -1;
                setStick(s, 0f, 0f);
            }
        }
        invalidate();
    }

    /** Opens the layout editor (Input > Touch > Edit Touch Layout), over everything else. */
    public void startEditing() {
        releaseAll();
        pollUiState();
        editing = true;
        selected = null;
        dragPointer = pinchPointer = -1;
        startPolling();
        refreshVisibility();
        invalidate();
    }

    private void stopEditing() {
        editing = false;
        selected = null;
        dragPointer = pinchPointer = -1;
        saveLayout();
        refreshVisibility();
        invalidate();
    }

    private void startPolling() {
        handler.removeCallbacks(poll);
        handler.post(poll);
    }

    private void pollUiState() {
        int state = TouchInput.uiState();
        boolean menu = (state & 1) != 0;
        boolean analog = (state & 2) != 0;
        if (analog != analogMode && !editing) {
            releaseAll();
            analogMode = analog;
            loadLayout();
            layoutControls();
            invalidate();
        }
        if (menu != menuOpen) {
            menuOpen = menu;
            if (menu) {
                releaseAll();
            }
            refreshVisibility();
        }
    }

    private void refreshVisibility() {
        boolean show = editing || (active && !menuOpen);
        if (!show) {
            releaseAll();
        }
        setVisibility(show ? VISIBLE : GONE);
    }

    // ------------------------------------------------------------------------------
    // Layout
    // ------------------------------------------------------------------------------

    private String prefix() {
        return analogMode ? "analog." : "normal.";
    }

    private void loadLayout() {
        adjust.clear();
        SharedPreferences prefs = getContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        for (Control c : controls) {
            if (adjust.containsKey(c.group)) {
                continue;
            }
            Adjust a = new Adjust();
            String value = prefs.getString(prefix() + c.group, null);
            if (value != null) {
                String[] parts = value.split(",");
                try {
                    a.dx = Float.parseFloat(parts[0]);
                    a.dy = Float.parseFloat(parts[1]);
                    a.scale = clampScale(Float.parseFloat(parts[2]));
                } catch (RuntimeException ignored) {
                    a = new Adjust();
                }
            }
            adjust.put(c.group, a);
        }
    }

    private void saveLayout() {
        SharedPreferences.Editor editor = getContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit();
        for (Map.Entry<String, Adjust> e : adjust.entrySet()) {
            Adjust a = e.getValue();
            String key = prefix() + e.getKey();
            if (a.dx == 0f && a.dy == 0f && a.scale == 1f) {
                editor.remove(key);
            } else {
                editor.putString(key, a.dx + "," + a.dy + "," + a.scale);
            }
        }
        editor.apply();
    }

    private static float clampScale(float s) {
        return Math.max(MIN_SCALE, Math.min(MAX_SCALE, s));
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        layoutControls();
        if (active || editing) {
            startPolling();
        }
    }

    private void layoutControls() {
        int w = getWidth(), h = getHeight();
        if (w == 0 || h == 0) {
            return;
        }
        // Size controls from the shorter side, but never wider than a 1.4:1 working area,
        // so the columns stay apart on square or portrait screens.
        unit = Math.min(Math.min(w, h), w / MIN_LAYOUT_ASPECT);

        // Default positions first.
        Map<String, float[]> centres = new HashMap<>(); // group -> sum x, sum y, count
        for (Control c : controls) {
            Place p = analogMode ? c.analog : c.normal;
            c.shown = p != null;
            if (p == null) {
                continue;
            }
            c.r = p.radius * unit;
            switch (p.ax) {
                case LEFT: c.cx = p.ox * unit; break;
                case RIGHT: c.cx = w - p.ox * unit; break;
                default: c.cx = w / 2f + p.ox * unit; break;
            }
            c.cy = (p.ay == AnchorY.TOP) ? p.oy * unit : h - p.oy * unit;
            float[] sum = centres.get(c.group);
            if (sum == null) {
                sum = new float[3];
                centres.put(c.group, sum);
            }
            sum[0] += c.cx; sum[1] += c.cy; sum[2] += 1;
        }
        // Then the player's moves and sizes, per group, around the group's centre.
        for (Control c : controls) {
            if (!c.shown) {
                continue;
            }
            Adjust a = adjust.get(c.group);
            if (a == null) {
                continue;
            }
            float[] sum = centres.get(c.group);
            float gx = sum[0] / sum[2], gy = sum[1] / sum[2];
            c.cx = gx + (c.cx - gx) * a.scale + a.dx * w;
            c.cy = gy + (c.cy - gy) * a.scale + a.dy * h;
            c.r *= a.scale;
        }
        stroke.setStrokeWidth(Math.max(2f, unit * 0.006f));

        // Editor toolbar, in the middle of the screen.
        float bh = unit * 0.10f, gap = unit * 0.03f;
        float[] widths = { unit * 0.14f, unit * 0.14f, unit * 0.24f, unit * 0.24f };
        float total = gap * 3;
        for (float bw : widths) total += bw;
        float x = (w - total) / 2f, top = h * 0.5f - bh / 2f;
        for (int i = 0; i < toolbar.length; i++) {
            toolbar[i].set(x, top, x + widths[i], top + bh);
            x += widths[i] + gap;
        }
    }

    // ------------------------------------------------------------------------------
    // Drawing
    // ------------------------------------------------------------------------------

    @Override
    protected void onDraw(Canvas canvas) {
        int baseAlpha = Math.round(255 * (editing ? Math.max(opacity, 0.8f) : opacity));
        if (editing) {
            canvas.drawColor(Color.argb(150, 0, 0, 0));
        }
        for (Control c : controls) {
            if (!c.shown) {
                continue;
            }
            if (c.kind == Kind.STICK) {
                drawStick(canvas, c, baseAlpha);
            } else {
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
            if (editing && c.group.equals(selected)) {
                stroke.setColor(COLOR_SELECT);
                stroke.setAlpha(255);
                float old = stroke.getStrokeWidth();
                stroke.setStrokeWidth(old * 2f);
                canvas.drawCircle(c.cx, c.cy, c.r + old * 2f, stroke);
                stroke.setStrokeWidth(old);
                stroke.setColor(Color.WHITE);
            }
        }
        if (editing) {
            drawEditor(canvas);
        }
    }

    private void drawStick(Canvas canvas, Control c, int baseAlpha) {
        fill.setColor(Color.BLACK);
        fill.setAlpha(baseAlpha / 3);
        canvas.drawCircle(c.cx, c.cy, c.r, fill);
        stroke.setAlpha(baseAlpha);
        canvas.drawCircle(c.cx, c.cy, c.r, stroke);

        float knobX = c.cx + c.sx * c.r, knobY = c.cy + c.sy * c.r;
        fill.setColor(COLOR_NEUTRAL);
        fill.setAlpha(c.pointerId != -1 ? Math.min(255, baseAlpha + 90) : baseAlpha);
        canvas.drawCircle(knobX, knobY, c.r * 0.45f, fill);
        canvas.drawCircle(knobX, knobY, c.r * 0.45f, stroke);
        if (!c.label.isEmpty()) {
            text.setAlpha(Math.min(255, baseAlpha + 30));
            text.setTextSize(c.r * 0.22f);
            float baseline = knobY - (text.descent() + text.ascent()) / 2f;
            canvas.drawText(c.label, knobX, baseline, text);
        }
    }

    private void drawEditor(Canvas canvas) {
        text.setAlpha(255);
        text.setTextSize(unit * 0.045f);
        float y = toolbar[0].top - unit * 0.10f;
        canvas.drawText(analogMode ? "Touch layout (Analog Camera)" : "Touch layout", getWidth() / 2f, y, text);
        text.setTextSize(unit * 0.032f);
        canvas.drawText(selected == null ? "Tap a control to select it, drag to move, pinch to resize"
                : "Drag to move · pinch or − / + to resize", getWidth() / 2f, y + unit * 0.05f, text);

        float radius = unit * 0.02f;
        for (int i = 0; i < toolbar.length; i++) {
            RectF rect = toolbar[i];
            boolean primary = i == 3;
            fill.setColor(primary ? Color.rgb(46, 158, 79) : Color.rgb(60, 60, 68));
            fill.setAlpha(235);
            canvas.drawRoundRect(rect, radius, radius, fill);
            stroke.setAlpha(255);
            canvas.drawRoundRect(rect, radius, radius, stroke);
            text.setTextSize(rect.height() * 0.45f);
            float baseline = rect.centerY() - (text.descent() + text.ascent()) / 2f;
            canvas.drawText(TOOLBAR_LABELS[i], rect.centerX(), baseline, text);
        }
    }

    // ------------------------------------------------------------------------------
    // Touch handling
    // ------------------------------------------------------------------------------

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (editing) {
            onEditTouch(event);
            return true;
        }
        if (!active || menuOpen) {
            return false;
        }

        final int action = event.getActionMasked();
        final int index = event.getActionIndex();
        final int pointerId = event.getPointerId(index);

        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                float x = event.getX(index), y = event.getY(index);
                Control stick = null;
                for (Control s : sticks) {
                    if (s.shown && s.pointerId == -1 && s.hit(x, y, s.captureSlack)) {
                        stick = s;
                        break;
                    }
                }
                if (stick != null) {
                    stick.pointerId = pointerId;
                    updateStick(stick, x, y);
                } else if (hitButton(x, y) != null) {
                    buttonPointers.put(pointerId, new float[] { x, y });
                } else if (action == MotionEvent.ACTION_DOWN) {
                    return false;
                }
                TouchInput.ensure();
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                for (int i = 0; i < event.getPointerCount(); i++) {
                    int id = event.getPointerId(i);
                    Control stick = stickFor(id);
                    if (stick != null) {
                        updateStick(stick, event.getX(i), event.getY(i));
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
                    for (Control s : sticks) {
                        if (s.pointerId != -1) {
                            s.pointerId = -1;
                            setStick(s, 0f, 0f);
                        }
                    }
                } else {
                    Control stick = stickFor(pointerId);
                    if (stick != null) {
                        stick.pointerId = -1;
                        setStick(stick, 0f, 0f);
                    } else {
                        buttonPointers.remove(pointerId);
                    }
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

    private Control stickFor(int pointerId) {
        for (Control s : sticks) {
            if (s.pointerId == pointerId) {
                return s;
            }
        }
        return null;
    }

    private Control hitButton(float x, float y) {
        for (Control c : controls) {
            if (c.shown && c.kind == Kind.BUTTON && c.hit(x, y, BUTTON_SLACK)) {
                return c;
            }
        }
        return null;
    }

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
            Haptics.tap(this);
        }
    }

    private void updateStick(Control s, float x, float y) {
        float dx = (x - s.cx) / s.r, dy = (y - s.cy) / s.r;
        float length = (float) Math.sqrt(dx * dx + dy * dy);
        if (length > 1f) {
            dx /= length;
            dy /= length;
        }
        setStick(s, dx, dy);
    }

    private void setStick(Control s, float x, float y) {
        s.sx = x;
        s.sy = y;
        // Screen Y grows downward, as does SDL's stick Y; no flip needed.
        TouchInput.setAxis(s.axisX, x);
        TouchInput.setAxis(s.axisY, y);
    }

    // ------------------------------------------------------------------------------
    // Layout editor
    // ------------------------------------------------------------------------------

    private void onEditTouch(MotionEvent event) {
        final int action = event.getActionMasked();
        final int index = event.getActionIndex();
        final int pointerId = event.getPointerId(index);
        switch (action) {
            case MotionEvent.ACTION_DOWN: {
                float x = event.getX(index), y = event.getY(index);
                for (int i = 0; i < toolbar.length; i++) {
                    if (toolbar[i].contains(x, y)) {
                        onToolbar(i);
                        return;
                    }
                }
                Control c = controlAt(x, y);
                selected = c != null ? c.group : null;
                if (c != null) {
                    dragPointer = pointerId;
                    dragLastX = x;
                    dragLastY = y;
                    Haptics.tap(this);
                }
                break;
            }
            case MotionEvent.ACTION_POINTER_DOWN: {
                if (selected != null && pinchPointer == -1) {
                    pinchPointer = pointerId;
                    pinchStartDist = pinchDistance(event);
                    pinchStartScale = adjust.get(selected).scale;
                }
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                if (selected == null) {
                    break;
                }
                Adjust a = adjust.get(selected);
                if (pinchPointer != -1 && dragPointer != -1) {
                    float d = pinchDistance(event);
                    if (pinchStartDist > 1f && d > 1f) {
                        a.scale = clampScale(pinchStartScale * d / pinchStartDist);
                        layoutControls();
                    }
                } else if (dragPointer != -1) {
                    int i = event.findPointerIndex(dragPointer);
                    if (i >= 0) {
                        float x = event.getX(i), y = event.getY(i);
                        a.dx += (x - dragLastX) / getWidth();
                        a.dy += (y - dragLastY) / getHeight();
                        a.dx = Math.max(-1f, Math.min(1f, a.dx));
                        a.dy = Math.max(-1f, Math.min(1f, a.dy));
                        dragLastX = x;
                        dragLastY = y;
                        layoutControls();
                        keepGroupOnScreen(selected);
                    }
                }
                break;
            }
            case MotionEvent.ACTION_POINTER_UP: {
                if (pointerId == pinchPointer) {
                    pinchPointer = -1;
                } else if (pointerId == dragPointer) {
                    // The pinching finger carries on dragging.
                    dragPointer = pinchPointer;
                    pinchPointer = -1;
                    int i = dragPointer != -1 ? event.findPointerIndex(dragPointer) : -1;
                    if (i >= 0) {
                        dragLastX = event.getX(i);
                        dragLastY = event.getY(i);
                    }
                }
                break;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                dragPointer = pinchPointer = -1;
                break;
            default:
                break;
        }
        invalidate();
    }

    private float pinchDistance(MotionEvent event) {
        int a = event.findPointerIndex(dragPointer), b = event.findPointerIndex(pinchPointer);
        if (a < 0 || b < 0) {
            return 0f;
        }
        float dx = event.getX(a) - event.getX(b), dy = event.getY(a) - event.getY(b);
        return (float) Math.sqrt(dx * dx + dy * dy);
    }

    private Control controlAt(float x, float y) {
        Control best = null;
        float bestDist = Float.MAX_VALUE;
        for (Control c : controls) {
            if (!c.shown) {
                continue;
            }
            float dx = x - c.cx, dy = y - c.cy;
            float d = (float) Math.sqrt(dx * dx + dy * dy) - c.r;
            if (d < unit * 0.03f && d < bestDist) {
                best = c;
                bestDist = d;
            }
        }
        return best;
    }

    /** Pulls a group back so its controls' centres stay on screen. */
    private void keepGroupOnScreen(String group) {
        float shiftX = 0f, shiftY = 0f;
        int w = getWidth(), h = getHeight();
        for (Control c : controls) {
            if (!c.shown || !c.group.equals(group)) {
                continue;
            }
            if (c.cx < 0) shiftX = Math.max(shiftX, -c.cx);
            if (c.cx > w) shiftX = Math.min(shiftX, w - c.cx);
            if (c.cy < 0) shiftY = Math.max(shiftY, -c.cy);
            if (c.cy > h) shiftY = Math.min(shiftY, h - c.cy);
        }
        if (shiftX != 0f || shiftY != 0f) {
            Adjust a = adjust.get(group);
            a.dx += shiftX / w;
            a.dy += shiftY / h;
            layoutControls();
        }
    }

    private void onToolbar(int i) {
        Haptics.tap(this);
        switch (i) {
            case 0:
            case 1: {
                float step = i == 0 ? -0.1f : 0.1f;
                if (selected != null) {
                    Adjust a = adjust.get(selected);
                    a.scale = clampScale(a.scale + step);
                } else {
                    for (Adjust a : adjust.values()) {
                        a.scale = clampScale(a.scale + step);
                    }
                }
                break;
            }
            case 2:
                for (Adjust a : adjust.values()) {
                    a.dx = a.dy = 0f;
                    a.scale = 1f;
                }
                selected = null;
                break;
            default:
                stopEditing();
                return;
        }
        layoutControls();
        invalidate();
    }

    // Folding or unfolding a foldable can take the view off its window and put it back:
    // start watching the menus and camera type again.
    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (active || editing) {
            startPolling();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        handler.removeCallbacks(poll);
        super.onDetachedFromWindow();
    }
}
