package io.github.bownz.dk64recomp;

/**
 * Bridge to android/jni/touch_controller.cpp, which turns touch input into an SDL virtual
 * gamepad. The game then sees an ordinary controller, so its default bindings and its
 * remapping menu apply to touch controls too.
 *
 * Constants are SDL_GameControllerButton / SDL_GameControllerAxis values (SDL 2).
 */
final class TouchInput {
    // SDL_GameControllerButton
    static final int BUTTON_SOUTH = 0;          // N64 A
    static final int BUTTON_EAST = 1;
    static final int BUTTON_WEST = 2;           // N64 B
    static final int BUTTON_NORTH = 3;
    static final int BUTTON_BACK = 4;           // Opens the recomp menu
    static final int BUTTON_START = 6;          // N64 Start
    static final int BUTTON_RIGHT_STICK = 8;    // N64 L (unused by DK64; default mapping)
    static final int BUTTON_LEFT_SHOULDER = 9;
    static final int BUTTON_RIGHT_SHOULDER = 10;
    static final int BUTTON_DPAD_UP = 11;
    static final int BUTTON_DPAD_DOWN = 12;
    static final int BUTTON_DPAD_LEFT = 13;
    static final int BUTTON_DPAD_RIGHT = 14;

    // SDL_GameControllerAxis
    static final int AXIS_LEFT_X = 0;           // N64 analog stick
    static final int AXIS_LEFT_Y = 1;
    static final int AXIS_RIGHT_X = 2;          // N64 C-left / C-right
    static final int AXIS_RIGHT_Y = 3;          // N64 C-up / C-down
    static final int AXIS_TRIGGER_LEFT = 4;     // N64 Z
    static final int AXIS_TRIGGER_RIGHT = 5;    // N64 R

    private TouchInput() {}

    /** Creates or removes the virtual gamepad. Safe to call before the game starts SDL. */
    static native void nativeSetEnabled(boolean enabled);

    static native void nativeSetButton(int button, boolean pressed);

    /** Sticks take -1..1 (down/right positive), triggers take 0..1. */
    static native void nativeSetAxis(int axis, float value);

    // N64 button bits (recompinput values), for buttons that act on the game directly.
    static final int N64_A = 0x8000, N64_Z = 0x2000, N64_B = 0x4000, N64_START = 0x1000;
    static final int N64_L = 0x0020, N64_R = 0x0010;
    static final int N64_C_UP = 0x0008, N64_C_DOWN = 0x0004, N64_C_LEFT = 0x0002, N64_C_RIGHT = 0x0001;
    static final int N64_DPAD_UP = 0x0800, N64_DPAD_DOWN = 0x0400, N64_DPAD_LEFT = 0x0200, N64_DPAD_RIGHT = 0x0100;

    /**
     * N64 buttons that go straight into the game's input for player 1, bypassing the
     * controller mappings, so they work however the physical pad is mapped.
     */
    private static native void nativeSetN64Buttons(int buttons);

    private static int pressedN64, heldN64, overlayN64;

    private static native int nativeTakePhysicalZ();

    /**
     * The player's own Z (controller or keyboard, not the on-screen buttons): bit 1 if it's
     * held now, bit 2 if it was pressed since the last call.
     */
    static int takePhysicalZ() {
        try {
            return nativeTakePhysicalZ();
        } catch (UnsatisfiedLinkError e) {
            return 0;
        }
    }

    /** Buttons pressed for a moment by an on-screen button (replaces the previous ones). */
    static synchronized void setN64Buttons(int buttons) {
        pressedN64 = buttons;
        pushN64();
    }

    /** Buttons kept held until changed (e.g. Z for Lanky's Orangstand), on top of the above. */
    static synchronized void setHeldN64Buttons(int buttons) {
        heldN64 = buttons;
        pushN64();
    }

    /** The on-screen controller's own buttons (held while touched), on top of the above. */
    static synchronized void setOverlayN64Button(int bit, boolean down) {
        overlayN64 = down ? (overlayN64 | bit) : (overlayN64 & ~bit);
        pushN64();
    }

    private static native void nativeSetOverlayActive(boolean active);
    private static native void nativeAddCameraDrag(float dxDp, float dyDp);

    /** The on-screen controller is in use: the game keeps its own N64 button icons. */
    static void setOverlayActive(boolean active) {
        try { nativeSetOverlayActive(active); } catch (UnsatisfiedLinkError ignored) { }
    }

    /** Finger movement (dp) for the touch-drag camera; the game takes it each frame. */
    static void addCameraDrag(float dxDp, float dyDp) {
        try { nativeAddCameraDrag(dxDp, dyDp); } catch (UnsatisfiedLinkError ignored) { }
    }

    private static native int nativeUiState();

    /**
     * Bit 1: a menu is taking input. Bit 2: the Analog Camera is selected. Bit 4: Drag to Move
     * Camera is on. Bit 16: the game has started (not just the launcher).
     */
    static int uiState() {
        try {
            return nativeUiState();
        } catch (UnsatisfiedLinkError e) {
            return 1;
        }
    }

    private static void pushN64() {
        try { nativeSetN64Buttons(pressedN64 | heldN64 | overlayN64); } catch (UnsatisfiedLinkError ignored) { }
    }

    // Several on-screen controls (the overlay controller, the second-screen action pad) share
    // one virtual gamepad. It exists while at least one of them holds it.
    private static int users = 0;

    /** Starts using the virtual gamepad (creating it if needed). Pair with {@link #release()}. */
    static synchronized void acquire() {
        users++;
        setEnabled(true);
    }

    /** Stops using it; the gamepad is removed once nothing uses it. */
    static synchronized void release() {
        if (users > 0) {
            users--;
        }
        if (users == 0) {
            setEnabled(false);
        }
    }

    /** Retries creating the gamepad if it's in use but the game hadn't started SDL yet. */
    static synchronized void ensure() {
        setEnabled(users > 0);
    }

    // The natives live in libmain.so, which SDLActivity loads before any view exists.
    // A call that races library loading must not crash the app.
    private static void setEnabled(boolean enabled) {
        try { nativeSetEnabled(enabled); } catch (UnsatisfiedLinkError ignored) { }
    }

    /** Removes the gamepad regardless of users, e.g. when the activity is destroyed. */
    static synchronized void shutdown() {
        users = 0;
        setEnabled(false);
    }

    static void setButton(int button, boolean pressed) {
        try { nativeSetButton(button, pressed); } catch (UnsatisfiedLinkError ignored) { }
    }

    static void setAxis(int axis, float value) {
        try { nativeSetAxis(axis, value); } catch (UnsatisfiedLinkError ignored) { }
    }
}
