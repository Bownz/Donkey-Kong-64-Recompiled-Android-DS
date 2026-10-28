package io.github.bownz.dk64recomp;

import java.util.Arrays;

/**
 * A snapshot of DK64's state for the second screen, read (never written) from the game's
 * memory by android/jni/game_state.cpp. Layout of the raw array is documented there.
 */
final class GameState {
    static final int DK = 0, DIDDY = 1, LANKY = 2, TINY = 3, CHUNKY = 4;
    static final int KONG_COUNT = 5;

    /** Level slots: 0-6 the seven levels, 7 DK Isles (with the lobbies), 8 Hideout Helm. */
    static final int LEVELS = 9;
    static final int ISLES = 7, HELM = 8;
    static final String[] LEVEL_NAMES = {
        "Jungle Japes", "Angry Aztec", "Frantic Factory", "Gloomy Galleon", "Fungi Forest",
        "Crystal Caves", "Creepy Castle", "DK Isles", "Hideout Helm",
    };

    private static final int COUNT = 8 + KONG_COUNT * 6 + 2 + KONG_COUNT * LEVELS * 2 + 1;

    // ---- Permanent flags (DK64 Randomizer location lists; US release) ----------------
    // Shared collectibles, per level slot.
    static final int[][] FAIRY_FLAGS = {
        { 589, 590 }, { 600, 601 }, { 591, 602 }, { 592, 603 }, { 595, 596 },
        { 597, 608 }, { 604, 605 }, { 593, 594, 606, 607 }, { 598, 599 },
    };
    static final int[][] CROWN_FLAGS = {
        { 609 }, { 610 }, { 611 }, { 612 }, { 613 }, { 616 }, { 617 }, { 614, 615 }, { 618 },
    };
    static final int[][] KEY_FLAGS = {
        { 26 }, { 74 }, { 138 }, { 168 }, { 236 }, { 292 }, { 317 }, {}, { 380 },
    };
    static final int[][] RAINBOW_COIN_FLAGS = {
        { 0x2A6 }, { 0x2A7, 0x2AC }, { 0x2A8 }, { 0x2AF }, { 0x2AD, 0x2AE }, { 0x2B0 }, { 0x2B1 },
        { 0x2A9, 0x2AA, 0x2AB, 0x2B2, 0x2B3, 0x2B4, 0x2B5 }, {},
    };
    // Kong collectibles: one blueprint per Kong in each of the levels and Isles; one medal
    // per Kong in each level and Helm. Flag = base + slot * 5 + kong.
    private static final int BLUEPRINT_BASE = 0x1D5;
    private static final int MEDAL_BASE = 0x225;

    static int blueprintFlag(int kong, int level) {
        return level <= ISLES ? BLUEPRINT_BASE + level * 5 + kong : -1;
    }

    static int medalFlag(int kong, int level) {
        if (level == ISLES) {
            return -1;
        }
        return MEDAL_BASE + (level == HELM ? 7 : level) * 5 + kong;
    }

    // Every flag the screen shows, read in one call; index of each in flagIds.
    private static final int[] flagIds;
    private static final java.util.HashMap<Integer, Integer> flagIndex = new java.util.HashMap<>();
    static {
        java.util.ArrayList<Integer> ids = new java.util.ArrayList<>();
        for (int level = 0; level < LEVELS; level++) {
            for (int k = 0; k < KONG_COUNT; k++) {
                ids.add(blueprintFlag(k, level));
                ids.add(medalFlag(k, level));
            }
            for (int[][] list : new int[][][] { FAIRY_FLAGS, CROWN_FLAGS, KEY_FLAGS, RAINBOW_COIN_FLAGS }) {
                for (int f : list[level]) {
                    ids.add(f);
                }
            }
        }
        ids.removeIf(f -> f < 0);
        flagIds = new int[ids.size()];
        for (int i = 0; i < flagIds.length; i++) {
            flagIds[i] = ids.get(i);
            flagIndex.put(flagIds[i], i);
        }
    }
    private final int[] flagValues = new int[flagIds.length];
    private final int[] previousFlags = new int[flagIds.length];

    /** One Kong's progress. */
    static final class Kong {
        int weapon;            // 0 = not bought yet
        int moves;             // special moves bought from Cranky (Lanky: 1+ = Orangstand)
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
    int progress;              // bits 0-4: Kong k freed; 5: oranges learned; 6: camera
    final Kong[] kongs = new Kong[KONG_COUNT];

    private final int[] raw = new int[COUNT];
    private final int[] previous = new int[COUNT];

    GameState() {
        for (int i = 0; i < KONG_COUNT; i++) {
            kongs[i] = new Kong();
        }
    }

    private static native boolean nativeRead(int[] out);
    private static native boolean nativeReadFlags(int[] ids, int[] out);
    private static native int nativeKongSwap();
    private static native int nativeFrameCount();

    /** Game frames so far, or -1 while the "Show FPS" setting is off. */
    static int frameCount() {
        try {
            return nativeFrameCount();
        } catch (UnsatisfiedLinkError e) {
            return -1;
        }
    }

    static final int SWAP_SETTING = 1;   // "Touch Kong Swap" is on
    static final int SWAP_MOD = 2;       // the Tag Anywhere mod is enabled
    static final int SWAP_PLAYING = 4;   // normal play: not paused, no cutscene

    /** SWAP_* bits (0 if the game isn't running). */
    static int kongSwap() {
        try {
            return nativeKongSwap();
        } catch (UnsatisfiedLinkError e) {
            return 0;
        }
    }

    /** The level being played (kept through bonus rooms and menus); -1 until known. */
    int level = -1;
    /** The Kong is holding their gun out (after Z + C-Left, until it's put away). */
    boolean gunOut;
    /** In first-person view (C-Up), until it's left. */
    boolean firstPerson;
    /** Z + C moves (weapon, oranges, instrument, camera) work in the current state. */
    boolean canZMoves;
    /** C-Up enters (or leaves) first-person view in the current state. */
    boolean canFirstPerson;
    /** A save file is being played (not the title screen or file select). */
    boolean fileLoaded;
    /** Swimming (on the surface or underwater). */
    boolean swimming;
    /** Hanging on a vine, or climbing a tree or pole. */
    boolean climbing;
    /** The pause menu is open. */
    boolean paused;
    /** The player's control state (DK64's own numbering). */
    int controlState;
    /** The game's input-handler set for the player (0x0C standing, 0x0D walking, 0x3C crouching; 0xFF unknown). */
    int inputSet = 0xFF;
    final int[][] goldenByLevel = new int[KONG_COUNT][LEVELS];
    final int[][] colouredByLevel = new int[KONG_COUNT][LEVELS];

    boolean flag(int id) {
        Integer i = flagIndex.get(id);
        return i != null && flagValues[i] != 0;
    }

    /** How many of these flags are set. */
    int countFlags(int[] ids) {
        int n = 0;
        for (int id : ids) {
            if (flag(id)) {
                n++;
            }
        }
        return n;
    }

    /** Re-reads the game. Returns true if anything changed since the last call. */
    boolean refresh() {
        boolean ok;
        try {
            ok = nativeRead(raw);
            if (!nativeReadFlags(flagIds, flagValues)) {
                Arrays.fill(flagValues, 0);
            }
        } catch (UnsatisfiedLinkError e) {
            ok = false;
        }
        if (!ok) {
            Arrays.fill(raw, 0);
            raw[0] = -1;
        }
        boolean flagsChanged = !Arrays.equals(flagValues, previousFlags);
        System.arraycopy(flagValues, 0, previousFlags, 0, flagValues.length);
        if (Arrays.equals(raw, previous) && !flagsChanged) {
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
            kong.weapon = raw[base] & 0xFF;
            kong.moves = (raw[base] >> 8) & 0xFF;
            kong.instrument = raw[base + 1];
            kong.coins = raw[base + 2];
            kong.instrumentEnergy = raw[base + 3];
            kong.goldenBananas = raw[base + 4];
            kong.colouredBananas = raw[base + 5];
        }
        progress = raw[8 + KONG_COUNT * 6];
        int newLevel = raw[8 + KONG_COUNT * 6 + 1];
        if (newLevel >= 0 && newLevel < LEVELS) {
            level = newLevel;
        }
        gunOut = (raw[COUNT - 1] & 1) != 0;
        firstPerson = (raw[COUNT - 1] & 2) != 0;
        canZMoves = (raw[COUNT - 1] & 4) != 0;
        canFirstPerson = (raw[COUNT - 1] & 8) != 0;
        fileLoaded = (raw[COUNT - 1] & 16) != 0;
        swimming = (raw[COUNT - 1] & 32) != 0;
        paused = (raw[COUNT - 1] & 64) != 0;
        controlState = (raw[COUNT - 1] >> 8) & 0xFF;
        // Control states (code_EFDC0.c, code_EBBE0.c): 0x4E/0x4F swimming, 0x57 on a vine,
        // 0x59 climbing a tree or pole.
        swimming |= controlState == 0x4E || controlState == 0x4F;
        climbing = controlState == 0x57 || controlState == 0x59;
        inputSet = (raw[COUNT - 1] >> 16) & 0xFF;
        for (int k = 0; k < KONG_COUNT; k++) {
            for (int l = 0; l < LEVELS; l++) {
                int base = 8 + KONG_COUNT * 6 + 2 + (k * LEVELS + l) * 2;
                goldenByLevel[k][l] = raw[base];
                colouredByLevel[k][l] = raw[base + 1];
            }
        }
        return true;
    }

    /** Whether Kong k is playable: freed, or the one being played (DK always is). */
    boolean isFreed(int k) {
        return k == DK || k == currentKong || (progress & (1 << k)) != 0;
    }

    /** Orange grenades learned (training barrel). */
    boolean hasOranges() {
        return (progress & (1 << 5)) != 0;
    }

    /** Fairy camera received. */
    boolean hasCamera() {
        return (progress & (1 << 6)) != 0;
    }

    /**
     * Whether a game is in progress, so the values mean something. Before a save is loaded
     * (title screen, launcher) these locations hold zeros or leftovers.
     */
    boolean inGame() {
        return fileLoaded && currentKong >= 0 && currentKong < KONG_COUNT && melons >= 1 && melons <= 3
            && health >= 0 && health <= melons * 4;
    }
}
