package io.github.bownz.dk64recomp;

import android.content.Context;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.HapticFeedbackConstants;
import android.view.View;

/**
 * Touch button vibration, following the Dual Screen tab's "Touch Feedback" and
 * "Feedback Strength" settings (read from the game's config through GameState).
 */
final class Haptics {
    private Haptics() {}

    static void tap(View view) {
        int level = GameState.touchFeedback();
        if (level <= 0) {
            return;
        }
        Vibrator vibrator = (Vibrator) view.getContext().getSystemService(Context.VIBRATOR_SERVICE);
        if (vibrator == null || !vibrator.hasVibrator()) {
            view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
            return;
        }
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                int effect = level == 1 ? VibrationEffect.EFFECT_TICK
                    : level == 2 ? VibrationEffect.EFFECT_CLICK : VibrationEffect.EFFECT_HEAVY_CLICK;
                vibrator.vibrate(VibrationEffect.createPredefined(effect));
            } else {
                long ms = level == 1 ? 10 : level == 2 ? 20 : 35;
                int amplitude = vibrator.hasAmplitudeControl()
                    ? (level == 1 ? 70 : level == 2 ? 150 : 255) : VibrationEffect.DEFAULT_AMPLITUDE;
                vibrator.vibrate(VibrationEffect.createOneShot(ms, amplitude));
            }
        } catch (RuntimeException e) {
            view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        }
    }
}
