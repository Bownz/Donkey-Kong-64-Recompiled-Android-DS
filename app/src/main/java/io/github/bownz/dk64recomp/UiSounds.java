package io.github.bownz.dk64recomp;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.SoundPool;

/**
 * The menus' sounds: small wooden bongo taps (res/raw/ui_*.wav, made by
 * android/tools/gen_ui_sounds.py). Played from native code when the menus are navigated
 * with a controller (moving, selecting, backing out).
 */
final class UiSounds {
    private static SoundPool pool;
    private static int move, select, back;

    private UiSounds() {}

    static synchronized void init(Context c) {
        if (pool != null) {
            return;
        }
        pool = new SoundPool.Builder()
            .setMaxStreams(3)
            .setAudioAttributes(new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                .build())
            .build();
        move = pool.load(c, R.raw.ui_move, 1);
        select = pool.load(c, R.raw.ui_select, 1);
        back = pool.load(c, R.raw.ui_back, 1);
    }

    /** kind: 0 move, 1 select, 2 back. */
    static synchronized void play(int kind) {
        if (pool == null) {
            return;
        }
        switch (kind) {
            case 0: pool.play(move, 0.35f, 0.35f, 1, 0, 1f); break;
            case 1: pool.play(select, 0.6f, 0.6f, 1, 0, 1f); break;
            case 2: pool.play(back, 0.5f, 0.5f, 1, 0, 1f); break;
            default: break;
        }
    }
}
