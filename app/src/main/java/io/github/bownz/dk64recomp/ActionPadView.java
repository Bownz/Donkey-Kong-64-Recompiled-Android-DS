package io.github.bownz.dk64recomp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Handler;
import android.os.Looper;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;

/**
 * One-tap action buttons for DK64's Z + C-button moves, for a second screen.
 *
 * In DK64 these need Z held, then a C-button: fruit weapon (Z+C-Left), orange grenade
 * (Z+C-Right), instrument (Z+C-Up) and fairy camera (Z+C-Down). Each button here plays that
 * sequence on the virtual gamepad ({@link TouchInput}), which the game merges with the
 * built-in controls, so it works while you're also using the physical buttons.
 */
public final class ActionPadView extends View {

    /** A Z + C-button move. */
    private static final class Action {
        final String title;
        final String combo;
        final int color;
        final int cAxis;      // TouchInput.AXIS_RIGHT_X or AXIS_RIGHT_Y
        final float cValue;   // -1 or 1
        final RectF bounds = new RectF();

        Action(String title, String combo, int color, int cAxis, float cValue) {
            this.title = title;
            this.combo = combo;
            this.color = color;
            this.cAxis = cAxis;
            this.cValue = cValue;
        }
    }

    // Timing. DK64 reads input once per game frame (~33 ms at 30 fps), so each step must last
    // at least a frame or two: hold Z, then press C while Z is still held, then let go.
    private static final long Z_LEAD_MS = 100;
    private static final long C_HOLD_MS = 150;
    private static final long Z_TAIL_MS = 60;

    private final Action[] actions = {
        new Action("Fruit weapon", "Z + C◀", Color.rgb(46, 158, 79), TouchInput.AXIS_RIGHT_X, -1f),
        new Action("Orange", "Z + C▶", Color.rgb(232, 140, 27), TouchInput.AXIS_RIGHT_X, 1f),
        new Action("Instrument", "Z + C▲", Color.rgb(59, 111, 216), TouchInput.AXIS_RIGHT_Y, -1f),
        new Action("Fairy camera", "Z + C▼", Color.rgb(170, 70, 190), TouchInput.AXIS_RIGHT_Y, 1f),
    };

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint titlePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint comboPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private Action running;      // The move currently being sent, or null.
    private boolean active;

    public ActionPadView(Context context) {
        super(context);
        stroke.setStyle(Paint.Style.STROKE);
        stroke.setColor(Color.WHITE);
        titlePaint.setColor(Color.WHITE);
        titlePaint.setTextAlign(Paint.Align.CENTER);
        titlePaint.setFakeBoldText(true);
        comboPaint.setColor(Color.argb(200, 255, 255, 255));
        comboPaint.setTextAlign(Paint.Align.CENTER);
        setHapticFeedbackEnabled(true);
        setContentDescription("DK64 action buttons");
    }

    /** Takes or gives up the shared virtual gamepad. */
    public void setActive(boolean active) {
        if (this.active == active) {
            return;
        }
        this.active = active;
        if (active) {
            TouchInput.acquire();
        } else {
            cancel();
            TouchInput.release();
        }
    }

    /** Stops any move in progress and lets go of Z and C (e.g. when the app pauses). */
    public void cancel() {
        handler.removeCallbacksAndMessages(null);
        if (running != null) {
            TouchInput.setAxis(running.cAxis, 0f);
            TouchInput.setAxis(TouchInput.AXIS_TRIGGER_LEFT, 0f);
            running = null;
            invalidate();
        }
    }

    private void perform(Action action) {
        if (running != null) {
            return; // One move at a time; the sequence takes ~0.3 s.
        }
        running = action;
        TouchInput.ensure();
        performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        invalidate();

        TouchInput.setAxis(TouchInput.AXIS_TRIGGER_LEFT, 1f);                      // Hold Z
        handler.postDelayed(() -> TouchInput.setAxis(action.cAxis, action.cValue), Z_LEAD_MS); // + C
        handler.postDelayed(() -> TouchInput.setAxis(action.cAxis, 0f), Z_LEAD_MS + C_HOLD_MS);
        handler.postDelayed(() -> {
            TouchInput.setAxis(TouchInput.AXIS_TRIGGER_LEFT, 0f);                  // Release Z
            running = null;
            invalidate();
        }, Z_LEAD_MS + C_HOLD_MS + Z_TAIL_MS);
    }

    // ------------------------------------------------------------------------------
    // Layout and drawing: a 2x2 grid of large buttons.
    // ------------------------------------------------------------------------------

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        float gap = Math.min(w, h) * 0.04f;
        float cellW = (w - gap * 3) / 2f;
        float cellH = (h - gap * 3) / 2f;
        for (int i = 0; i < actions.length; i++) {
            float left = gap + (i % 2) * (cellW + gap);
            float top = gap + (i / 2) * (cellH + gap);
            actions[i].bounds.set(left, top, left + cellW, top + cellH);
        }
        float unit = Math.min(cellW, cellH);
        titlePaint.setTextSize(unit * 0.16f);
        comboPaint.setTextSize(unit * 0.11f);
        stroke.setStrokeWidth(Math.max(2f, unit * 0.015f));
    }

    @Override
    protected void onDraw(Canvas canvas) {
        for (Action a : actions) {
            float radius = Math.min(a.bounds.width(), a.bounds.height()) * 0.12f;
            fill.setColor(a.color);
            fill.setAlpha(a == running ? 255 : 150);
            canvas.drawRoundRect(a.bounds, radius, radius, fill);
            stroke.setAlpha(a == running ? 255 : 120);
            canvas.drawRoundRect(a.bounds, radius, radius, stroke);

            float cx = a.bounds.centerX();
            float cy = a.bounds.centerY();
            canvas.drawText(a.title, cx, cy - titlePaint.descent(), titlePaint);
            canvas.drawText(a.combo, cx, cy - comboPaint.ascent() * 1.2f, comboPaint);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (!active) {
            return false;
        }
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            int index = event.getActionIndex();
            float x = event.getX(index), y = event.getY(index);
            for (Action a : actions) {
                if (a.bounds.contains(x, y)) {
                    perform(a);
                    break;
                }
            }
        }
        return true;
    }
}
