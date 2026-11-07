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
import java.util.LinkedHashMap;
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
 * player can move and resize each group of controls, and add or remove groups
 * ({@link #startEditing()}); the changes are kept per camera mode (normal / Analog Camera)
 * in SharedPreferences. Besides the N64 buttons there are the bottom screen's moves: the
 * action buttons (oranges, gun, instrument, camera), first person and the dynamic button,
 * shared with it through TouchMoves.
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
        // Move buttons (TouchMoves) only: the move, or the dynamic button.
        TouchMoves.Move move;
        boolean dynamic;
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

    /**
     * Groups the player can add or remove in the layout editor, with their names. The menu
     * button can't be removed (it's the way back to the editor). On by default: the ones in
     * {@link #DEFAULT_ON}.
     */
    private static final Map<String, String> GROUP_NAMES = new LinkedHashMap<>();
    static {
        GROUP_NAMES.put("stick", "Control stick");
        GROUP_NAMES.put("dpad", "D-pad");
        GROUP_NAMES.put("a", "A");
        GROUP_NAMES.put("b", "B");
        GROUP_NAMES.put("z", "Z");
        GROUP_NAMES.put("r", "R");
        GROUP_NAMES.put("start", "Start");
        GROUP_NAMES.put("c", "C buttons (all four)");
        GROUP_NAMES.put("c_up", "C-Up");
        GROUP_NAMES.put("c_down", "C-Down");
        GROUP_NAMES.put("c_left", "C-Left");
        GROUP_NAMES.put("c_right", "C-Right");
        GROUP_NAMES.put("camera", "Camera stick");
        GROUP_NAMES.put("actions", "Action buttons (all four)");
        GROUP_NAMES.put("act_orange", "Oranges");
        GROUP_NAMES.put("act_gun", "Gun");
        GROUP_NAMES.put("act_instrument", "Instrument");
        GROUP_NAMES.put("act_film", "Camera (film)");
        GROUP_NAMES.put("first_person", "First person");
        GROUP_NAMES.put("dynamic", "Dynamic move (Orangstand, Charge, Punch, Cancel)");
    }
    private static final Set<String> DEFAULT_ON = new HashSet<>(java.util.Arrays.asList(
        "stick", "dpad", "a", "b", "z", "r", "start", "c", "camera", "menu"));

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
    // Drag to Move Camera (Input > Touch): a finger that lands away from the buttons turns the camera.
    private boolean dragCamera = true;
    private int cameraPointer = -1;
    private float cameraLastX, cameraLastY;
    // Floating Control Stick (Input > Touch): the stick moves to wherever the left half is
    // touched, and back to its place when let go.
    private boolean floatingStick = false;
    private boolean stickFloating = false;
    private float stickHomeX, stickHomeY;
    // Greys out move buttons that can't be used (not bought yet, nothing to use, swimming...).
    private final Paint greyLayer = new Paint();
    private final Paint alphaLayer = new Paint();
    private float unit = 1f;

    // Layout adjustments for the current camera mode, by group.
    private final Map<String, Adjust> adjust = new HashMap<>();
    // Groups shown in the current camera mode (added or removed in the editor).
    private final Set<String> on = new HashSet<>();

    // The moves (action buttons, first person, dynamic), shared with the bottom screen.
    private final TouchMoves moves = TouchMoves.get();
    private boolean usingMoves;
    private final Runnable movesChanged = this::invalidate;

    // ---- Editing ----
    private boolean editing = false;
    private String selected;
    private int dragPointer = -1;
    private float dragLastX, dragLastY;
    private int pinchPointer = -1;
    private float pinchStartDist, pinchStartScale;
    private final RectF[] toolbar = { new RectF(), new RectF(), new RectF(), new RectF(), new RectF(), new RectF() };
    private static final String[] TOOLBAR_LABELS = { "−", "+", "Remove", "Add", "Reset", "Done" };
    private static final int TB_SMALLER = 0, TB_BIGGER = 1, TB_REMOVE = 2, TB_ADD = 3, TB_RESET = 4, TB_DONE = 5;

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

        android.graphics.ColorMatrix grey = new android.graphics.ColorMatrix();
        grey.setSaturation(0f);
        greyLayer.setColorFilter(new android.graphics.ColorMatrixColorFilter(grey));
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

        // ---- Added in the layout editor (off by default) ----
        // Single C buttons, in a row across the middle.
        float rowY = 0.40f, rowR = 0.05f;
        controls.add(button("c_up", "C▲", COLOR_C, both(AnchorX.CENTER, -0.27f, AnchorY.TOP, rowY, rowR), TouchInput.N64_C_UP));
        controls.add(button("c_down", "C▼", COLOR_C, both(AnchorX.CENTER, -0.09f, AnchorY.TOP, rowY, rowR), TouchInput.N64_C_DOWN));
        controls.add(button("c_left", "C◀", COLOR_C, both(AnchorX.CENTER, 0.09f, AnchorY.TOP, rowY, rowR), TouchInput.N64_C_LEFT));
        controls.add(button("c_right", "C▶", COLOR_C, both(AnchorX.CENTER, 0.27f, AnchorY.TOP, rowY, rowR), TouchInput.N64_C_RIGHT));
        // The action buttons as a cluster, laid out like the C buttons they use (Z + C):
        // instrument up, camera down, gun left, oranges right.
        float ax = 0f, ay = 0.62f, aOff = 0.085f, aR = 0.05f;
        controls.add(moveButton("actions", TouchMoves.Move.INSTRUMENT, both(AnchorX.CENTER, ax, AnchorY.TOP, ay - aOff, aR)));
        controls.add(moveButton("actions", TouchMoves.Move.CAMERA, both(AnchorX.CENTER, ax, AnchorY.TOP, ay + aOff, aR)));
        controls.add(moveButton("actions", TouchMoves.Move.WEAPON, both(AnchorX.CENTER, ax - aOff, AnchorY.TOP, ay, aR)));
        controls.add(moveButton("actions", TouchMoves.Move.ORANGE, both(AnchorX.CENTER, ax + aOff, AnchorY.TOP, ay, aR)));
        // Single action buttons, in a row above the C buttons' row.
        float actY = 0.25f;
        controls.add(moveButton("act_orange", TouchMoves.Move.ORANGE, both(AnchorX.CENTER, -0.27f, AnchorY.TOP, actY, rowR)));
        controls.add(moveButton("act_gun", TouchMoves.Move.WEAPON, both(AnchorX.CENTER, -0.09f, AnchorY.TOP, actY, rowR)));
        controls.add(moveButton("act_instrument", TouchMoves.Move.INSTRUMENT, both(AnchorX.CENTER, 0.09f, AnchorY.TOP, actY, rowR)));
        controls.add(moveButton("act_film", TouchMoves.Move.CAMERA, both(AnchorX.CENTER, 0.27f, AnchorY.TOP, actY, rowR)));
        controls.add(moveButton("first_person", TouchMoves.Move.FIRST_PERSON, both(AnchorX.CENTER, -0.18f, AnchorY.TOP, 0.55f, 0.055f)));
        Control dyn = new Control(Kind.BUTTON, "dynamic", "", COLOR_NEUTRAL,
            both(AnchorX.CENTER, 0.18f, AnchorY.TOP, 0.55f, 0.055f), both(AnchorX.CENTER, 0.18f, AnchorY.TOP, 0.55f, 0.055f),
            () -> moves.performDynamic(null), () -> { }, 0, 0, 1f);
        dyn.dynamic = true;
        controls.add(dyn);

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

    /** One of the bottom screen's moves (TouchMoves): does it on press, if it can now. */
    private Control moveButton(String group, TouchMoves.Move move, Place place) {
        Control c = new Control(Kind.BUTTON, group, "", COLOR_NEUTRAL, place, place,
            () -> {
                if (moves.owned(move, -1) && moves.available(move)) {
                    moves.perform(move, null);
                }
            }, () -> { }, 0, 0, 1f);
        c.move = move;
        return c;
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
        TouchInput.setOverlayActive(active);
        if (!active) {
            releaseAll();
            TouchInput.release();
        } else {
            TouchInput.acquire();
            startPolling();
        }
        updateMovesUse();
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
        cameraPointer = -1;
        for (Control s : sticks) {
            if (s.pointerId != -1 || s.sx != 0f || s.sy != 0f) {
                s.pointerId = -1;
                setStick(s, 0f, 0f);
            }
            stickReleased(s);
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
        updateMovesUse();
        startPolling();
        refreshVisibility();
        invalidate();
    }

    private void stopEditing() {
        editing = false;
        selected = null;
        dragPointer = pinchPointer = -1;
        saveLayout();
        layoutControls();
        refreshVisibility();
        invalidate();
    }

    /** Reads the game's state (for the move buttons) only while any are shown. */
    private void updateMovesUse() {
        boolean want = (active || editing) && anyMoveGroupOn();
        if (want == usingMoves) {
            return;
        }
        usingMoves = want;
        if (want) {
            moves.addListener(movesChanged);
            moves.acquire();
        } else {
            moves.release();
            moves.removeListener(movesChanged);
        }
    }

    private boolean anyMoveGroupOn() {
        for (Control c : controls) {
            if ((c.move != null || c.dynamic) && on.contains(c.group)) {
                return true;
            }
        }
        return false;
    }

    private void startPolling() {
        handler.removeCallbacks(poll);
        handler.post(poll);
    }

    private void pollUiState() {
        if (usingMoves && moves.state.inGame()) {
            RomIcons.ensureLoaded(this::postInvalidate);   // the move buttons' icons (from the ROM)
        }
        int state = TouchInput.uiState();
        boolean menu = (state & 1) != 0;
        boolean analog = (state & 2) != 0;
        dragCamera = (state & 4) != 0;
        if (!dragCamera) {
            cameraPointer = -1;
        }
        floatingStick = (state & 8) != 0;
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
        on.clear();
        SharedPreferences prefs = getContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        for (Control c : controls) {
            String shown = prefs.getString(prefix() + "on." + c.group, null);
            if (shown != null ? shown.equals("1") : DEFAULT_ON.contains(c.group)) {
                on.add(c.group);
            }
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
        for (String group : adjust.keySet()) {
            String key = prefix() + "on." + group;
            boolean shown = on.contains(group);
            if (shown == DEFAULT_ON.contains(group)) {
                editor.remove(key);
            } else {
                editor.putString(key, shown ? "1" : "0");
            }
        }
        editor.apply();
        updateMovesUse();
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
            c.shown = p != null && on.contains(c.group);
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

        // Editor toolbar, in the middle of the screen: − + Remove / Add Reset Done.
        float bh = unit * 0.10f, gap = unit * 0.03f;
        float[] widths = { unit * 0.14f, unit * 0.14f, unit * 0.26f, unit * 0.22f, unit * 0.22f, unit * 0.22f };
        for (int row = 0; row < 2; row++) {
            float total = gap * 2;
            for (int i = row * 3; i < row * 3 + 3; i++) total += widths[i];
            float x = (w - total) / 2f, top = h * 0.5f - bh / 2f + row * (bh + gap);
            for (int i = row * 3; i < row * 3 + 3; i++) {
                toolbar[i].set(x, top, x + widths[i], top + bh);
                x += widths[i] + gap;
            }
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
            } else if (c.move != null || c.dynamic) {
                if (hiddenNow(c)) {
                    continue;
                }
                drawMoveButton(canvas, c, baseAlpha);
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

    /** A move button: the bottom screen's icon on a round button, dimmed when it can't be used. */
    private void drawMoveButton(Canvas canvas, Control c, int baseAlpha) {
        boolean lit, usable;
        String label = null;
        int kong = moves.state.currentKong;
        if (kong < 0 || kong >= GameState.KONG_COUNT) {
            kong = GameState.DK;
        }
        TouchMoves.Dynamic dyn = c.dynamic ? moves.dynamic() : TouchMoves.Dynamic.NONE;
        if (c.dynamic) {
            switch (dyn) {
                case CANCEL: label = "Cancel"; lit = moves.cancelBusy; usable = !moves.state.paused; break;
                case ORANGSTAND: label = "Orangstand"; lit = moves.orangstand || moves.orangstandBusy;
                    usable = !moves.state.paused && (moves.orangstand || (!moves.state.swimming && !moves.state.climbing && !moves.state.gunOut));
                    break;
                case ZB_MOVE: label = moves.zbMoveName(); lit = moves.zbBusy; usable = moves.zbMoveUsable(); break;
                default: lit = false; usable = editing; break;
            }
        } else {
            lit = moves.lit(c.move);
            usable = editing || (moves.owned(c.move, -1) && !moves.greyed(c.move));
        }
        int alpha = usable ? baseAlpha : baseAlpha / 3;
        boolean down = usable && (pressed.contains(c) || lit);
        fill.setColor(!usable ? Color.rgb(90, 90, 90)
            : c.dynamic && dyn == TouchMoves.Dynamic.CANCEL ? Color.rgb(200, 60, 45)
            : c.dynamic ? Color.rgb(200, 130, 50) : Color.rgb(150, 100, 55));
        fill.setAlpha(down ? Math.min(255, alpha + 90) : alpha / 2);
        canvas.drawCircle(c.cx, c.cy, c.r, fill);
        stroke.setAlpha(alpha);
        canvas.drawCircle(c.cx, c.cy, c.r, stroke);

        Paint layer = usable ? alphaLayer : greyLayer;
        layer.setAlpha(usable ? Math.min(255, alpha + 60) : Math.min(255, baseAlpha / 2 + 30));
        int saved = canvas.saveLayer(c.cx - c.r, c.cy - c.r, c.cx + c.r, c.cy + c.r, layer);
        float iconY = label != null ? c.cy - c.r * 0.12f : c.cy, iconR = c.r * (label != null ? 0.5f : 0.62f);
        if (c.dynamic) {
            if (dyn == TouchMoves.Dynamic.ORANGSTAND) {
                canvas.rotate(180f, c.cx, iconY);   // Lanky upside down
            }
            if (!RomIcons.draw(canvas, RomIcons.face[kong], c.cx, iconY, iconR)) {
                DkIcons.eye(canvas, c.cx, iconY, iconR * 0.8f);
            }
            if (dyn == TouchMoves.Dynamic.ORANGSTAND) {
                canvas.rotate(-180f, c.cx, iconY);
            }
        } else {
            switch (c.move) {
                case WEAPON:
                    if (!RomIcons.draw(canvas, RomIcons.of(RomIcons.weapon, kong), c.cx, iconY, iconR)) {
                        DkIcons.weapon(canvas, kong, c.cx, iconY, iconR);
                    }
                    break;
                case ORANGE:
                    if (!RomIcons.draw(canvas, RomIcons.orange, c.cx, iconY, iconR)) {
                        DkIcons.orange(canvas, c.cx, iconY, iconR);
                    }
                    break;
                case INSTRUMENT:
                    if (!RomIcons.draw(canvas, RomIcons.of(RomIcons.instrument, kong), c.cx, iconY, iconR)) {
                        DkIcons.instrument(canvas, kong, c.cx, iconY, iconR);
                    }
                    break;
                case CAMERA:
                    if (!RomIcons.draw(canvas, RomIcons.film, c.cx, iconY, iconR)) {
                        DkIcons.camera(canvas, c.cx, iconY, iconR);
                    }
                    break;
                default:   // first person
                    if (!RomIcons.draw(canvas, RomIcons.crosshair, c.cx, iconY, iconR)) {
                        DkIcons.eye(canvas, c.cx, iconY, iconR * 0.8f);
                    }
                    break;
            }
        }
        canvas.restoreToCount(saved);
        if (label != null) {
            text.setAlpha(Math.min(255, alpha + 60));
            text.setTextSize(c.r * 0.3f);
            canvas.drawText(label, c.cx, c.cy + c.r * 0.72f, text);
        } else if (c.dynamic && editing) {
            text.setAlpha(255);
            text.setTextSize(c.r * 0.26f);
            canvas.drawText("Dynamic", c.cx, c.cy + c.r * 0.72f, text);
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
        canvas.drawText(selected == null ? "Tap a control to select it, drag to move, pinch to resize · Add for more"
                : (GROUP_NAMES.containsKey(selected) ? GROUP_NAMES.get(selected) + ": " : "")
                    + "drag to move · pinch or − / + to resize · Remove to hide", getWidth() / 2f, y + unit * 0.05f, text);

        float radius = unit * 0.02f;
        for (int i = 0; i < toolbar.length; i++) {
            RectF rect = toolbar[i];
            boolean primary = i == TB_DONE;
            boolean disabled = i == TB_REMOVE && (selected == null || !GROUP_NAMES.containsKey(selected));
            fill.setColor(primary ? Color.rgb(46, 158, 79) : i == TB_REMOVE && !disabled ? Color.rgb(170, 60, 50) : Color.rgb(60, 60, 68));
            fill.setAlpha(disabled ? 110 : 235);
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
                Control left = floatingStick ? leftStick() : null;
                if (stick != null) {
                    stick.pointerId = pointerId;
                    updateStick(stick, x, y);
                } else if (hitButton(x, y) != null) {
                    buttonPointers.put(pointerId, new float[] { x, y });
                } else if (left != null && left.shown && left.pointerId == -1 && x < getWidth() / 2f) {
                    // Floating Control Stick: the stick comes to the thumb, centred under it.
                    stickHomeX = left.cx;
                    stickHomeY = left.cy;
                    stickFloating = true;
                    left.cx = x;
                    left.cy = y;
                    left.pointerId = pointerId;
                    updateStick(left, x, y);
                } else if (dragCamera && cameraPointer == -1) {
                    // Away from the buttons: this finger turns the camera (Drag to Move Camera).
                    cameraPointer = pointerId;
                    cameraLastX = x;
                    cameraLastY = y;
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
                    } else if (id == cameraPointer) {
                        float x = event.getX(i), y = event.getY(i);
                        float density = getResources().getDisplayMetrics().density;
                        TouchInput.addCameraDrag((x - cameraLastX) / density, (y - cameraLastY) / density);
                        cameraLastX = x;
                        cameraLastY = y;
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
                    cameraPointer = -1;
                    for (Control s : sticks) {
                        if (s.pointerId != -1) {
                            s.pointerId = -1;
                            setStick(s, 0f, 0f);
                            stickReleased(s);
                        }
                    }
                } else {
                    Control stick = stickFor(pointerId);
                    if (stick != null) {
                        stick.pointerId = -1;
                        setStick(stick, 0f, 0f);
                        stickReleased(stick);
                    } else if (pointerId == cameraPointer) {
                        cameraPointer = -1;
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

    /** The left (control) stick. */
    private Control leftStick() {
        for (Control s : sticks) {
            if (s.group.equals("stick")) {
                return s;
            }
        }
        return null;
    }

    /** Puts a floating stick back in its place once the thumb lifts. */
    private void stickReleased(Control s) {
        if (stickFloating && s.group.equals("stick")) {
            stickFloating = false;
            s.cx = stickHomeX;
            s.cy = stickHomeY;
        }
    }

    private Control stickFor(int pointerId) {
        for (Control s : sticks) {
            if (s.pointerId == pointerId) {
                return s;
            }
        }
        return null;
    }

    /**
     * Shown but not drawn or touchable right now: the dynamic button while there's nothing
     * for it to do (outside the layout editor).
     */
    private boolean hiddenNow(Control c) {
        return c.dynamic && !editing && !dynamicUsable();
    }

    private boolean dynamicUsable() {
        switch (moves.dynamic()) {
            case CANCEL: return !moves.state.paused;
            case ORANGSTAND: return !moves.state.paused && (moves.orangstand || moves.orangstandBusy
                || (!moves.state.swimming && !moves.state.climbing && !moves.state.gunOut));
            case ZB_MOVE: return moves.zbBusy || moves.zbMoveUsable();
            default: return false;
        }
    }

    private Control hitButton(float x, float y) {
        for (Control c : controls) {
            if (c.shown && c.kind == Kind.BUTTON && !hiddenNow(c) && c.hit(x, y, BUTTON_SLACK)) {
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
            case TB_REMOVE:
                if (selected != null && GROUP_NAMES.containsKey(selected)) {
                    on.remove(selected);
                    selected = null;
                    updateMovesUse();
                }
                break;
            case TB_ADD:
                showAddMenu();
                return;
            case TB_SMALLER:
            case TB_BIGGER: {
                float step = i == TB_SMALLER ? -0.1f : 0.1f;
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
            case TB_RESET:
                for (Adjust a : adjust.values()) {
                    a.dx = a.dy = 0f;
                    a.scale = 1f;
                }
                on.clear();
                on.addAll(DEFAULT_ON);
                selected = null;
                updateMovesUse();
                break;
            default:
                stopEditing();
                return;
        }
        layoutControls();
        invalidate();
    }

    /** Lists the groups not on screen; picking one adds it (selected, ready to move). */
    private void showAddMenu() {
        final List<String> groups = new ArrayList<>();
        List<String> names = new ArrayList<>();
        for (Map.Entry<String, String> e : GROUP_NAMES.entrySet()) {
            String group = e.getKey();
            if (on.contains(group) || (group.equals("camera") && !analogMode)) {
                continue;   // the camera stick is only there with the Analog Camera
            }
            groups.add(group);
            names.add(e.getValue());
        }
        if (groups.isEmpty()) {
            android.widget.Toast.makeText(getContext(), "Everything is already on screen.", android.widget.Toast.LENGTH_SHORT).show();
            return;
        }
        new android.app.AlertDialog.Builder(getContext())
            .setTitle("Add to the touch controls")
            .setItems(names.toArray(new String[0]), (dialog, which) -> {
                String group = groups.get(which);
                on.add(group);
                selected = group;
                updateMovesUse();
                layoutControls();
                keepGroupOnScreen(group);
                invalidate();
            })
            .setNegativeButton("Cancel", null)
            .show();
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
