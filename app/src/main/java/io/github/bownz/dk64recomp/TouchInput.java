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

    // The natives live in libmain.so, which SDLActivity loads before any view exists.
    // A call that races library loading must not crash the app.
    static void setEnabled(boolean enabled) {
        try { nativeSetEnabled(enabled); } catch (UnsatisfiedLinkError ignored) { }
    }

    static void setButton(int button, boolean pressed) {
        try { nativeSetButton(button, pressed); } catch (UnsatisfiedLinkError ignored) { }
    }

    static void setAxis(int axis, float value) {
        try { nativeSetAxis(axis, value); } catch (UnsatisfiedLinkError ignored) { }
    }
}
