package io.github.bownz.dk64recomp;

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * The touch "moves" shared by the bottom screen (BottomScreenView) and the on-screen
 * controller (TouchControllerView): the action buttons (oranges, gun, instrument, camera),
 * first person, and the dynamic button (Lanky's Orangstand, Diddy's Charge and Chunky's
 * Punch, or Cancel while a Kong barrel ability runs).
 *
 * Each move presses N64 buttons directly (Z, then C or B while Z is held), so it works
 * whatever the controller bindings are. There's one instance, so both screens see the same
 * state: one move runs at a time, and Lanky's Orangstand is one toggle wherever it's tapped.
 *
 * It reads the game's state while anything uses it ({@link #acquire}); views listen for
 * changes ({@link #addListener}) to redraw.
 */
final class TouchMoves {

    enum Move { WEAPON, ORANGE, INSTRUMENT, CAMERA, FIRST_PERSON }

    // Input timing. DK64 reads input once per game frame (~33 ms at 30 fps), so each step
    // lasts a few frames: hold Z, press C while Z is still held, then let go.
    static final long Z_LEAD_MS = 100;
    static final long C_HOLD_MS = 150;
    static final long Z_TAIL_MS = 60;
    private static final long POLL_MS = 150;

    private static TouchMoves instance;

    static TouchMoves get() {
        if (instance == null) {
            instance = new TouchMoves();
        }
        return instance;
    }

    final GameState state = new GameState();
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final List<Runnable> listeners = new ArrayList<>();
    private int users;

    /** The move being performed, or null. */
    Move running;
    /** Lanky is on his hands (entered with the Orangstand button); Z is held to keep him up. */
    boolean orangstand;
    boolean orangstandBusy;
    boolean zbBusy;
    boolean cancelBusy;
    /** When crystal coconuts last went down (a Kong barrel ability is running). */
    long crystalsDrainedAt;

    private TouchMoves() {}

    // ------------------------------------------------------------------------------
    // Users, polling and listeners
    // ------------------------------------------------------------------------------

    private final Runnable poll = new Runnable() {
        @Override
        public void run() {
            if (refresh()) {
                notifyListeners();
            }
            if (users > 0) {
                handler.postDelayed(this, POLL_MS);
            }
        }
    };

    /** Starts reading the game's state (pair with {@link #release}). */
    void acquire() {
        if (users++ == 0) {
            handler.removeCallbacks(poll);
            handler.post(poll);
        }
    }

    void release() {
        if (users > 0 && --users == 0) {
            handler.removeCallbacks(poll);
        }
    }

    void addListener(Runnable r) {
        if (!listeners.contains(r)) {
            listeners.add(r);
        }
    }

    void removeListener(Runnable r) {
        listeners.remove(r);
    }

    private void notifyListeners() {
        for (Runnable r : new ArrayList<>(listeners)) {
            r.run();
        }
    }

    /**
     * Reads the game's state and keeps the moves' own state in step with it. Returns true if
     * anything changed.
     */
    boolean refresh() {
        int crystalsBefore = state.crystals;
        boolean changed = state.refresh();
        if (state.inGame() && state.crystals < crystalsBefore) {
            crystalsDrainedAt = SystemClock.uptimeMillis();
        }
        if (updateOrangstand()) {
            changed = true;
        }
        return changed;
    }

    /** Stops any move in progress and lets go of every input (e.g. when the app pauses). */
    void cancel() {
        handler.removeCallbacksAndMessages(null);
        if (running != null || zbBusy || cancelBusy) {
            TouchInput.setN64Buttons(0);
            running = null;
            zbBusy = false;
            cancelBusy = false;
        }
        if (orangstandBusy || orangstand) {
            TouchInput.setN64Buttons(0);
            TouchInput.setHeldN64Buttons(0);
            orangstandBusy = false;
            orangstand = false;
        }
        if (users > 0) {
            handler.post(poll);
        }
        notifyListeners();
    }

    /** Anything in progress: a move, the Orangstand toggle, the Z + B move or Cancel. */
    boolean busy() {
        return running != null || orangstandBusy || zbBusy || cancelBusy;
    }

    // ------------------------------------------------------------------------------
    // Action buttons and first person
    // ------------------------------------------------------------------------------

    void perform(Move move, View view) {
        if (running != null) {
            return; // One move at a time; a sequence takes ~0.3 s.
        }
        running = move;
        if (view != null) {
            Haptics.tap(view);
        }
        notifyListeners();

        // The buttons press N64 Z and C directly (not through the controller mappings).
        final int c;
        switch (move) {
            case WEAPON:     c = TouchInput.N64_C_LEFT; break;
            case ORANGE:     c = TouchInput.N64_C_RIGHT; break;
            case INSTRUMENT: c = TouchInput.N64_C_UP; break;
            case CAMERA:     c = TouchInput.N64_C_DOWN; break;
            default:         c = TouchInput.N64_C_UP; break;
        }

        if (move == Move.CAMERA && state.camera) {
            // Leaving the camera: C-Down on its own (code_EBBE0.c), then C-Up if that didn't.
            TouchInput.setN64Buttons(TouchInput.N64_C_DOWN);
            handler.postDelayed(() -> TouchInput.setN64Buttons(0), C_HOLD_MS);
            handler.postDelayed(() -> {
                state.refresh();
                if (!state.camera) {
                    running = null;
                    notifyListeners();
                    return;
                }
                TouchInput.setN64Buttons(TouchInput.N64_C_UP);
                handler.postDelayed(() -> {
                    TouchInput.setN64Buttons(0);
                    running = null;
                    notifyListeners();
                }, C_HOLD_MS);
            }, C_HOLD_MS + 250);
            return;
        }

        if (move == Move.FIRST_PERSON) {
            // C-Up on its own enters first-person view, and pressing it again leaves it.
            TouchInput.setN64Buttons(c);
            handler.postDelayed(() -> {
                TouchInput.setN64Buttons(0);
                running = null;
                notifyListeners();
            }, C_HOLD_MS);
            return;
        }

        final int z = TouchInput.N64_Z;
        // Putting the gun away from first-person aiming: leave first person first (C-Up), then
        // Z + C-Left as usual, so the view doesn't stay in first person without the gun.
        final long lead = (move == Move.WEAPON && state.gunOut && state.firstPerson) ? C_HOLD_MS + 250 : 0;
        if (lead > 0) {
            TouchInput.setN64Buttons(TouchInput.N64_C_UP);
            handler.postDelayed(() -> TouchInput.setN64Buttons(0), C_HOLD_MS);
        }
        handler.postDelayed(() -> TouchInput.setN64Buttons(z), lead);                       // Z
        handler.postDelayed(() -> TouchInput.setN64Buttons(z | c), lead + Z_LEAD_MS);       // + C
        handler.postDelayed(() -> TouchInput.setN64Buttons(z), lead + Z_LEAD_MS + C_HOLD_MS);
        handler.postDelayed(() -> {
            TouchInput.setN64Buttons(0);
            running = null;
            notifyListeners();
        }, lead + Z_LEAD_MS + C_HOLD_MS + Z_TAIL_MS);
    }

    /**
     * Whether Kong {@code kong} has this move at all; buttons for moves not bought yet are
     * hidden. Before a game is running everything is shown.
     */
    boolean owned(Move move, int kong) {
        if (!state.inGame()) {
            return true;
        }
        if (kong < 0 || kong >= GameState.KONG_COUNT) {
            kong = state.currentKong;
        }
        GameState.Kong k = state.kongs[kong];
        switch (move) {
            case WEAPON: return k.weapon > 0;
            case ORANGE: return state.hasOranges() || state.oranges > 0;
            case INSTRUMENT: return k.instrument > 0;
            case CAMERA: return state.hasCamera() || state.film > 0;
            default: return true;
        }
    }

    /**
     * Whether the game takes this move right now. When it doesn't only because of what the
     * Kong is doing for a moment (jumping, attacking, rolling...), the button looks normal
     * but does nothing; see {@link #greyed} for when it's greyed out.
     */
    boolean usableNow(Move move) {
        if (running == move) {
            return true;
        }
        if (move == Move.CAMERA && state.camera) {
            return true;   // tapping again puts the camera away
        }
        switch (move) {
            case FIRST_PERSON: return state.canFirstPerson || state.firstPerson || state.gunOut;
            case WEAPON: return state.canZMoves || state.gunOut;
            default: return state.canZMoves && !state.gunOut;
        }
    }

    /**
     * Greyed out: nothing to use it with (no ammo, oranges, film or instrument energy),
     * swimming, or (for the other moves) while the gun is out. The gun button stays lit while
     * the gun is out so it can always be put away, even with no ammo left.
     */
    boolean greyed(Move move) {
        if (!state.inGame() || running == move) {
            return false;
        }
        if (state.paused) {
            return true;   // nothing works while the pause menu is open
        }
        if (orangstand || orangstandBusy) {
            return true;   // Lanky is on his hands: none of these work until he's back up
        }
        GameState.Kong k = state.kongs[state.currentKong];
        switch (move) {
            case FIRST_PERSON: return state.climbing;
            case WEAPON:
                if (state.gunOut) {
                    return false;
                }
                return state.swimming || state.climbing || (state.ammo <= 0 && state.homingAmmo <= 0);
            case ORANGE: return state.swimming || state.climbing || state.gunOut || state.oranges <= 0;
            // In first person (or the camera) Z + C-Up would just leave the view.
            case INSTRUMENT: return state.swimming || state.climbing || state.gunOut || state.firstPerson || state.camera
                || k.instrumentEnergy <= 0;
            case CAMERA: return !state.camera && (state.swimming || state.climbing || state.gunOut || state.film <= 0);
            default: return false;
        }
    }

    /** A tap on this button does something now. */
    boolean available(Move move) {
        return !state.inGame() || (!greyed(move) && usableNow(move));
    }

    /** Lit: the move is running, or its mode is on (gun out, camera up, first person). */
    boolean lit(Move move) {
        return running == move
            || (move == Move.WEAPON && state.gunOut)
            || (move == Move.CAMERA && state.camera)
            || (move == Move.FIRST_PERSON && state.firstPerson);
    }

    // ------------------------------------------------------------------------------
    // Lanky's Orangstand toggle
    // ------------------------------------------------------------------------------
    //
    // A toggle: Z then B (as on a controller) puts Lanky on his hands, and Z stays held to
    // keep him there, as the game needs. Tapping again lets go of Z. Pressing Z on the
    // controller takes over (he stays up while it's held). It also lets go when he ends up
    // back on his feet or in water (Z stays held through the pause menu).

    boolean showOrangstand() {
        return state.inGame() && state.currentKong == GameState.LANKY
            && state.kongs[GameState.LANKY].moves >= 1;
    }

    /** Called on each refresh: notice when Lanky is back on his feet (or isn't Lanky). */
    private boolean updateOrangstand() {
        int physicalZ = TouchInput.takePhysicalZ();
        if (orangstand && !orangstandBusy && (physicalZ & 2) != 0) {
            // The player pressed Z on the controller: hand over to it. While they hold Z,
            // Lanky stays on his hands; letting go puts him back on his feet.
            stopOrangstand();
            return true;
        }
        if (orangstand && !orangstandBusy && (!showOrangstand() || (!state.paused && (state.onFeet || state.swimming)))) {
            stopOrangstand();
            return true;
        }
        return false;
    }

    void toggleOrangstand(View view) {
        if (orangstandBusy || running != null || state.paused) {
            return;
        }
        if (view != null) {
            Haptics.tap(view);
        }
        if (orangstand) {
            // Letting go of Z puts Lanky back on his feet.
            stopOrangstand();
            return;
        }
        if (!state.canZMoves || state.gunOut) {
            return;
        }
        // Z, then B while Z is held; Z then stays held to keep him on his hands.
        orangstandBusy = true;
        notifyListeners();
        final int z = TouchInput.N64_Z, b = TouchInput.N64_B;
        TouchInput.setHeldN64Buttons(z);
        handler.postDelayed(() -> TouchInput.setN64Buttons(b), Z_LEAD_MS);
        handler.postDelayed(() -> TouchInput.setN64Buttons(0), Z_LEAD_MS + C_HOLD_MS);
        handler.postDelayed(() -> {
            state.refresh();
            TouchInput.takePhysicalZ();   // forget Z presses from before he got on his hands
            orangstandBusy = false;
            orangstand = !state.onFeet && !state.swimming;
            if (!orangstand) {
                TouchInput.setHeldN64Buttons(0);   // it didn't take: don't leave Z held
            }
            notifyListeners();
        }, Z_LEAD_MS + C_HOLD_MS + 350);
    }

    void stopOrangstand() {
        orangstand = false;
        TouchInput.setHeldN64Buttons(0);
        notifyListeners();
    }

    // ------------------------------------------------------------------------------
    // Other Z + B moves (same place): Diddy's Chimpy Charge, Chunky's Primate Punch
    // ------------------------------------------------------------------------------
    //
    // Crouching then B (code_E4090.c): Diddy charges once he has his first Cranky move,
    // Chunky punches once he has his second.

    /** The current Kong's Z + B move, if bought; null if none (Lanky's is the toggle above). */
    String zbMoveName() {
        if (!state.inGame()) {
            return null;
        }
        int k = state.currentKong;
        if (k == GameState.DIDDY && state.kongs[k].moves >= 1) {
            return "Charge";
        }
        if (k == GameState.CHUNKY && state.kongs[k].moves >= 2) {
            return "Punch";
        }
        return null;
    }

    boolean zbMoveUsable() {
        return !state.paused && !state.swimming && !state.climbing && !state.gunOut;
    }

    void performZbMove(View view) {
        if (zbBusy || running != null || !zbMoveUsable() || !state.canZMoves) {
            return;
        }
        if (view != null) {
            Haptics.tap(view);
        }
        zbBusy = true;
        notifyListeners();
        final int z = TouchInput.N64_Z, b = TouchInput.N64_B;
        TouchInput.setN64Buttons(z);
        handler.postDelayed(() -> TouchInput.setN64Buttons(z | b), Z_LEAD_MS);
        handler.postDelayed(() -> TouchInput.setN64Buttons(z), Z_LEAD_MS + C_HOLD_MS);
        handler.postDelayed(() -> {
            TouchInput.setN64Buttons(0);
            zbBusy = false;
            notifyListeners();
        }, Z_LEAD_MS + C_HOLD_MS + Z_TAIL_MS);
    }

    // ------------------------------------------------------------------------------
    // Ending a Kong barrel ability (same place as the Orangstand button)
    // ------------------------------------------------------------------------------
    //
    // Mini Monkey, Hunky Chunky, Strong Kong, Rocketbarrel, Baboon Balloon and the like use
    // up crystal coconuts while they last, and Z + C-Left ends them (code_CEAE0.c). The
    // button shows while the game reports one, or while crystals are running down.

    boolean showCancel() {
        return state.inGame() && (state.transformed || SystemClock.uptimeMillis() - crystalsDrainedAt < 1500);
    }

    void cancelAbility(View view) {
        if (cancelBusy || running != null || state.paused) {
            return;
        }
        if (view != null) {
            Haptics.tap(view);
        }
        cancelBusy = true;
        notifyListeners();
        final int z = TouchInput.N64_Z, c = TouchInput.N64_C_LEFT;
        TouchInput.setN64Buttons(z);
        handler.postDelayed(() -> TouchInput.setN64Buttons(z | c), Z_LEAD_MS);
        handler.postDelayed(() -> TouchInput.setN64Buttons(z), Z_LEAD_MS + C_HOLD_MS);
        handler.postDelayed(() -> {
            TouchInput.setN64Buttons(0);
            crystalsDrainedAt = 0;
        }, Z_LEAD_MS + C_HOLD_MS + Z_TAIL_MS);
        handler.postDelayed(() -> {
            cancelBusy = false;
            notifyListeners();
        }, Z_LEAD_MS + C_HOLD_MS + Z_TAIL_MS + 400);
    }

    // ------------------------------------------------------------------------------
    // The dynamic button: whichever of the three above fits right now
    // ------------------------------------------------------------------------------

    enum Dynamic { NONE, CANCEL, ORANGSTAND, ZB_MOVE }

    Dynamic dynamic() {
        if (showCancel()) {
            return Dynamic.CANCEL;
        }
        if (showOrangstand()) {
            return Dynamic.ORANGSTAND;
        }
        if (zbMoveName() != null) {
            return Dynamic.ZB_MOVE;
        }
        return Dynamic.NONE;
    }

    /** Taps the dynamic button. Returns false if there's nothing for it to do. */
    boolean performDynamic(View view) {
        switch (dynamic()) {
            case CANCEL: cancelAbility(view); return true;
            case ORANGSTAND: toggleOrangstand(view); return true;
            case ZB_MOVE: performZbMove(view); return true;
            default: return false;
        }
    }
}
