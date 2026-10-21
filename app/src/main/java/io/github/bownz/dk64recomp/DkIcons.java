package io.github.bownz.dk64recomp;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;

/**
 * Simple vector icons for the second screen, drawn in code: each Kong's fruit weapon and
 * instrument, the orange, the camera, first-person view, and watermelon health. Original
 * drawings of everyday objects, not the game's artwork.
 *
 * Every method draws into a square centred on (cx, cy) with half-size r.
 */
final class DkIcons {
    private DkIcons() {}

    private static final Paint P = new Paint(Paint.ANTI_ALIAS_FLAG);
    private static final Path PATH = new Path();
    private static final RectF R = new RectF();

    private static Paint fill(int color) {
        P.setStyle(Paint.Style.FILL);
        P.setColor(color);
        return P;
    }

    private static Paint stroke(int color, float width) {
        P.setStyle(Paint.Style.STROKE);
        P.setStrokeCap(Paint.Cap.ROUND);
        P.setStrokeJoin(Paint.Join.ROUND);
        P.setColor(color);
        P.setStrokeWidth(width);
        return P;
    }

    // ---- Weapons -------------------------------------------------------------------

    static void weapon(Canvas c, int kong, float cx, float cy, float r) {
        switch (kong) {
            case GameState.DIDDY: peanut(c, cx, cy, r); break;
            case GameState.LANKY: grapes(c, cx, cy, r); break;
            case GameState.TINY: feather(c, cx, cy, r); break;
            case GameState.CHUNKY: pineapple(c, cx, cy, r); break;
            default: coconut(c, cx, cy, r); break;
        }
    }

    static String weaponName(int kong) {
        switch (kong) {
            case GameState.DIDDY: return "Peanut Popguns";
            case GameState.LANKY: return "Grape Shooter";
            case GameState.TINY: return "Feather Bow";
            case GameState.CHUNKY: return "Pineapple Launcher";
            default: return "Coconut Shooter";
        }
    }

    static void coconut(Canvas c, float cx, float cy, float r) {
        c.drawCircle(cx, cy, r * 0.78f, fill(Color.rgb(122, 74, 38)));
        c.drawCircle(cx - r * 0.22f, cy - r * 0.22f, r * 0.2f, fill(Color.rgb(160, 104, 60)));
        float d = r * 0.11f;
        c.drawCircle(cx - r * 0.18f, cy - r * 0.05f, d, fill(Color.rgb(58, 34, 16)));
        c.drawCircle(cx + r * 0.18f, cy - r * 0.05f, d, fill(Color.rgb(58, 34, 16)));
        c.drawCircle(cx, cy + r * 0.22f, d, fill(Color.rgb(58, 34, 16)));
    }

    static void peanut(Canvas c, float cx, float cy, float r) {
        int shell = Color.rgb(214, 172, 104);
        c.drawCircle(cx - r * 0.28f, cy + r * 0.28f, r * 0.42f, fill(shell));
        c.drawCircle(cx + r * 0.28f, cy - r * 0.28f, r * 0.42f, fill(shell));
        c.drawCircle(cx, cy, r * 0.3f, fill(shell));
        Paint s = stroke(Color.rgb(160, 120, 64), r * 0.06f);
        c.drawLine(cx - r * 0.5f, cy + r * 0.12f, cx - r * 0.12f, cy + r * 0.5f, s);
        c.drawLine(cx + r * 0.12f, cy - r * 0.5f, cx + r * 0.5f, cy - r * 0.12f, s);
    }

    static void grapes(Canvas c, float cx, float cy, float r) {
        int grape = Color.rgb(122, 60, 168);
        int light = Color.rgb(160, 104, 204);
        float g = r * 0.24f;
        float[][] at = { {-0.3f, -0.25f}, {0f, -0.25f}, {0.3f, -0.25f}, {-0.15f, 0.08f}, {0.15f, 0.08f}, {0f, 0.4f} };
        for (float[] p : at) {
            c.drawCircle(cx + p[0] * r, cy + p[1] * r, g, fill(grape));
            c.drawCircle(cx + p[0] * r - g * 0.3f, cy + p[1] * r - g * 0.3f, g * 0.3f, fill(light));
        }
        c.drawLine(cx, cy - r * 0.5f, cx + r * 0.15f, cy - r * 0.8f, stroke(Color.rgb(90, 140, 60), r * 0.09f));
    }

    static void feather(Canvas c, float cx, float cy, float r) {
        PATH.reset();
        PATH.moveTo(cx - r * 0.6f, cy + r * 0.7f);
        PATH.quadTo(cx - r * 0.55f, cy - r * 0.4f, cx + r * 0.6f, cy - r * 0.75f);
        PATH.quadTo(cx + r * 0.3f, cy + r * 0.3f, cx - r * 0.6f, cy + r * 0.7f);
        PATH.close();
        c.drawPath(PATH, fill(Color.rgb(236, 232, 220)));
        c.drawLine(cx - r * 0.7f, cy + r * 0.8f, cx + r * 0.45f, cy - r * 0.55f, stroke(Color.rgb(150, 120, 90), r * 0.07f));
    }

    static void pineapple(Canvas c, float cx, float cy, float r) {
        Paint leaf = fill(Color.rgb(70, 150, 60));
        PATH.reset();
        PATH.moveTo(cx, cy - r * 0.9f);
        PATH.lineTo(cx - r * 0.3f, cy - r * 0.35f);
        PATH.lineTo(cx + r * 0.3f, cy - r * 0.35f);
        PATH.close();
        c.drawPath(PATH, leaf);
        R.set(cx - r * 0.45f, cy - r * 0.45f, cx + r * 0.45f, cy + r * 0.85f);
        c.drawOval(R, fill(Color.rgb(222, 170, 40)));
        Paint s = stroke(Color.rgb(160, 110, 20), r * 0.05f);
        c.save();
        c.clipPath(ovalPath(R));
        for (int i = -3; i <= 3; i++) {
            float o = i * r * 0.28f;
            c.drawLine(cx - r + o, cy - r, cx + r + o, cy + r * 1.2f, s);
            c.drawLine(cx + r + o, cy - r, cx - r + o, cy + r * 1.2f, s);
        }
        c.restore();
    }

    private static Path ovalPath(RectF oval) {
        Path p = new Path();
        p.addOval(oval, Path.Direction.CW);
        return p;
    }

    // ---- Instruments ---------------------------------------------------------------

    static void instrument(Canvas c, int kong, float cx, float cy, float r) {
        switch (kong) {
            case GameState.DIDDY: guitar(c, cx, cy, r); break;
            case GameState.LANKY: trombone(c, cx, cy, r); break;
            case GameState.TINY: saxophone(c, cx, cy, r); break;
            case GameState.CHUNKY: triangle(c, cx, cy, r); break;
            default: bongos(c, cx, cy, r); break;
        }
    }

    static String instrumentName(int kong) {
        switch (kong) {
            case GameState.DIDDY: return "Guitar";
            case GameState.LANKY: return "Trombone";
            case GameState.TINY: return "Saxophone";
            case GameState.CHUNKY: return "Triangle";
            default: return "Bongos";
        }
    }

    static void bongos(Canvas c, float cx, float cy, float r) {
        drum(c, cx - r * 0.36f, cy + r * 0.05f, r * 0.42f);
        drum(c, cx + r * 0.36f, cy + r * 0.12f, r * 0.36f);
    }

    private static void drum(Canvas c, float x, float y, float s) {
        R.set(x - s, y - s * 0.3f, x + s, y + s * 1.1f);
        c.drawRect(R, fill(Color.rgb(176, 96, 48)));
        R.set(x - s, y - s * 0.6f, x + s, y);
        c.drawOval(R, fill(Color.rgb(240, 226, 196)));
        c.drawOval(R, stroke(Color.rgb(120, 60, 30), s * 0.12f));
    }

    static void guitar(Canvas c, float cx, float cy, float r) {
        c.drawLine(cx + r * 0.05f, cy - r * 0.05f, cx + r * 0.7f, cy - r * 0.7f, stroke(Color.rgb(90, 60, 40), r * 0.14f));
        int body = Color.rgb(210, 60, 50);
        c.drawCircle(cx - r * 0.2f, cy + r * 0.2f, r * 0.42f, fill(body));
        c.drawCircle(cx + r * 0.08f, cy - r * 0.08f, r * 0.3f, fill(body));
        c.drawCircle(cx - r * 0.12f, cy + r * 0.12f, r * 0.11f, fill(Color.rgb(40, 20, 20)));
    }

    static void trombone(Canvas c, float cx, float cy, float r) {
        Paint brass = stroke(Color.rgb(222, 184, 70), r * 0.1f);
        c.drawLine(cx - r * 0.75f, cy - r * 0.2f, cx + r * 0.4f, cy - r * 0.2f, brass);
        c.drawLine(cx - r * 0.75f, cy + r * 0.2f, cx + r * 0.4f, cy + r * 0.2f, brass);
        R.set(cx - r * 0.95f, cy - r * 0.2f, cx - r * 0.55f, cy + r * 0.2f);
        c.drawArc(R, 90, 180, false, brass);
        PATH.reset();
        PATH.moveTo(cx + r * 0.35f, cy - r * 0.2f);
        PATH.lineTo(cx + r * 0.85f, cy - r * 0.5f);
        PATH.lineTo(cx + r * 0.85f, cy + r * 0.1f);
        PATH.close();
        c.drawPath(PATH, fill(Color.rgb(222, 184, 70)));
    }

    static void saxophone(Canvas c, float cx, float cy, float r) {
        Paint brass = stroke(Color.rgb(222, 184, 70), r * 0.2f);
        PATH.reset();
        PATH.moveTo(cx - r * 0.2f, cy - r * 0.8f);
        PATH.lineTo(cx - r * 0.2f, cy + r * 0.3f);
        PATH.quadTo(cx - r * 0.2f, cy + r * 0.75f, cx + r * 0.25f, cy + r * 0.6f);
        PATH.lineTo(cx + r * 0.45f, cy + r * 0.1f);
        c.drawPath(PATH, brass);
        c.drawCircle(cx + r * 0.5f, cy, r * 0.22f, fill(Color.rgb(222, 184, 70)));
        c.drawCircle(cx + r * 0.5f, cy, r * 0.12f, fill(Color.rgb(90, 60, 20)));
    }

    static void triangle(Canvas c, float cx, float cy, float r) {
        PATH.reset();
        PATH.moveTo(cx, cy - r * 0.7f);
        PATH.lineTo(cx + r * 0.65f, cy + r * 0.55f);
        PATH.lineTo(cx - r * 0.65f, cy + r * 0.55f);
        PATH.close();
        c.drawPath(PATH, stroke(Color.rgb(200, 205, 215), r * 0.11f));
        c.drawLine(cx + r * 0.2f, cy - r * 0.2f, cx + r * 0.75f, cy - r * 0.75f, stroke(Color.rgb(150, 155, 165), r * 0.07f));
    }

    // ---- Shared items ----------------------------------------------------------------

    static void orange(Canvas c, float cx, float cy, float r) {
        c.drawCircle(cx, cy + r * 0.08f, r * 0.7f, fill(Color.rgb(240, 140, 30)));
        c.drawCircle(cx - r * 0.22f, cy - r * 0.15f, r * 0.16f, fill(Color.rgb(250, 185, 90)));
        R.set(cx, cy - r * 0.85f, cx + r * 0.45f, cy - r * 0.55f);
        c.drawOval(R, fill(Color.rgb(70, 150, 60)));
    }

    static void camera(Canvas c, float cx, float cy, float r) {
        R.set(cx - r * 0.75f, cy - r * 0.4f, cx + r * 0.75f, cy + r * 0.55f);
        c.drawRoundRect(R, r * 0.12f, r * 0.12f, fill(Color.rgb(60, 62, 70)));
        R.set(cx - r * 0.3f, cy - r * 0.6f, cx + r * 0.15f, cy - r * 0.4f);
        c.drawRect(R, fill(Color.rgb(60, 62, 70)));
        c.drawCircle(cx, cy + r * 0.07f, r * 0.3f, fill(Color.rgb(200, 210, 230)));
        c.drawCircle(cx, cy + r * 0.07f, r * 0.17f, fill(Color.rgb(40, 50, 80)));
        c.drawCircle(cx + r * 0.5f, cy - r * 0.22f, r * 0.08f, fill(Color.rgb(250, 230, 120)));
    }

    /** A single yellow banana. */
    static void banana(Canvas c, float cx, float cy, float r) {
        PATH.reset();
        PATH.moveTo(cx - r * 0.85f, cy - r * 0.45f);
        PATH.quadTo(cx - r * 0.2f, cy + r * 0.95f, cx + r * 0.85f, cy - r * 0.15f);
        PATH.quadTo(cx - r * 0.05f, cy + r * 0.35f, cx - r * 0.85f, cy - r * 0.45f);
        PATH.close();
        c.drawPath(PATH, fill(Color.rgb(255, 214, 40)));
        c.drawPath(PATH, stroke(Color.rgb(120, 80, 10), r * 0.1f));
        c.drawLine(cx + r * 0.85f, cy - r * 0.15f, cx + r * 0.95f, cy - r * 0.35f, stroke(Color.rgb(90, 60, 20), r * 0.14f));
    }

    /** A five-pointed star (generic collectible). */
    static void star(Canvas c, float cx, float cy, float r) {
        PATH.reset();
        for (int i = 0; i < 10; i++) {
            double a = Math.PI / 2 + i * Math.PI / 5;
            float rr = i % 2 == 0 ? r : r * 0.45f;
            float px = cx + (float) Math.cos(a) * rr, py = cy - (float) Math.sin(a) * rr;
            if (i == 0) PATH.moveTo(px, py); else PATH.lineTo(px, py);
        }
        PATH.close();
        c.drawPath(PATH, fill(Color.rgb(255, 214, 40)));
        c.drawPath(PATH, stroke(Color.rgb(120, 80, 10), r * 0.1f));
    }

    /** A gold coin. */
    static void coin(Canvas c, float cx, float cy, float r) {
        c.drawCircle(cx, cy, r, fill(Color.rgb(196, 140, 30)));
        c.drawCircle(cx, cy, r * 0.8f, fill(Color.rgb(248, 200, 60)));
        c.drawCircle(cx, cy, r * 0.5f, stroke(Color.rgb(196, 140, 30), r * 0.12f));
    }

    static void eye(Canvas c, float cx, float cy, float r) {
        PATH.reset();
        PATH.moveTo(cx - r * 0.85f, cy);
        PATH.quadTo(cx, cy - r * 0.75f, cx + r * 0.85f, cy);
        PATH.quadTo(cx, cy + r * 0.75f, cx - r * 0.85f, cy);
        PATH.close();
        c.drawPath(PATH, fill(Color.rgb(240, 240, 235)));
        c.drawCircle(cx, cy, r * 0.32f, fill(Color.rgb(70, 120, 200)));
        c.drawCircle(cx, cy, r * 0.15f, fill(Color.rgb(15, 15, 20)));
        c.drawCircle(cx - r * 0.1f, cy - r * 0.1f, r * 0.06f, fill(Color.WHITE));
    }

    /** One watermelon slice for health; {@code quarters} of 4 are filled. */
    static void melon(Canvas c, float cx, float cy, float r, int quarters) {
        c.drawCircle(cx, cy, r, fill(Color.rgb(40, 120, 50)));                  // rind
        R.set(cx - r * 0.82f, cy - r * 0.82f, cx + r * 0.82f, cy + r * 0.82f);
        c.drawOval(R, fill(Color.rgb(60, 36, 36)));                             // empty flesh
        for (int q = 0; q < quarters; q++) {
            c.drawArc(R, -90 + q * 90, 90, true, fill(Color.rgb(230, 60, 70)));
        }
        Paint line = stroke(Color.rgb(40, 120, 50), r * 0.08f);
        c.drawLine(cx, cy - r * 0.82f, cx, cy + r * 0.82f, line);
        c.drawLine(cx - r * 0.82f, cy, cx + r * 0.82f, cy, line);
        for (int q = 0; q < quarters; q++) {
            double a = Math.toRadians(-45 + q * 90);
            c.drawCircle(cx + (float) Math.cos(a) * r * 0.45f, cy + (float) Math.sin(a) * r * 0.45f, r * 0.07f, fill(Color.rgb(25, 20, 20)));
        }
    }
}
