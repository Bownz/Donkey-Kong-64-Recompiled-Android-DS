package io.github.bownz.dk64recomp;

import java.util.Arrays;

/**
 * A snapshot of DK64's state for the second screen, read (never written) from the game's
 * memory by android/jni/game_state.cpp. Layout of the raw array is documented there.
 */
final class GameState {
    static final int DK = 0, DIDDY = 1, LANKY = 2, TINY = 3, CHUNKY = 4;
    static final int KONG_COUNT = 5;

    private static final int COUNT = 8 + KONG_COUNT * 6 + 1;

    /** One Kong's progress. */
    static final class Kong {
        int weapon;            // 0 = not bought yet
        int instrument;        // 0 = not bought yet
        int coins;
        int instrumentEnergy;
        int goldenBananas;
        int colouredBananas;
    }

    int currentKong = -1;
    int health;                // quarter melons
    int melons;
    int ammo;
    int homingAmmo;
    int oranges;
    int crystals;
    int film;
    int freedKongs;            // bit k: Kong k has been freed
    final Kong[] kongs = new Kong[KONG_COUNT];

    private final int[] raw = new int[COUNT];
    private final int[] previous = new int[COUNT];

    GameState() {
        for (int i = 0; i < KONG_COUNT; i++) {
            kongs[i] = new Kong();
        }
    }

    private static native boolean nativeRead(int[] out);

    /** Re-reads the game. Returns true if anything changed since the last call. */
    boolean refresh() {
        boolean ok;
        try {
            ok = nativeRead(raw);
        } catch (UnsatisfiedLinkError e) {
            ok = false;
        }
        if (!ok) {
            Arrays.fill(raw, 0);
            raw[0] = -1;
        }
        if (Arrays.equals(raw, previous)) {
            return false;
        }
        System.arraycopy(raw, 0, previous, 0, COUNT);

        currentKong = raw[0];
        health = raw[1];
        melons = raw[2];
        ammo = raw[3];
        homingAmmo = raw[4];
        oranges = raw[5];
        crystals = raw[6];
        film = raw[7];
        for (int k = 0; k < KONG_COUNT; k++) {
            int base = 8 + k * 6;
            Kong kong = kongs[k];
            kong.weapon = raw[base];
            kong.instrument = raw[base + 1];
            kong.coins = raw[base + 2];
            kong.instrumentEnergy = raw[base + 3];
            kong.goldenBananas = raw[base + 4];
            kong.colouredBananas = raw[base + 5];
        }
        freedKongs = raw[8 + KONG_COUNT * 6];
        return true;
    }

    /** Whether Kong k is playable: freed, or the one being played (DK always is). */
    boolean isFreed(int k) {
        return k == DK || k == currentKong || (freedKongs & (1 << k)) != 0;
    }

    /**
     * Whether a game is in progress, so the values mean something. Before a save is loaded
     * (title screen, launcher) these locations hold zeros or leftovers.
     */
    boolean inGame() {
        return currentKong >= 0 && currentKong < KONG_COUNT && melons >= 1 && melons <= 3
            && health >= 0 && health <= melons * 4;
    }
}
