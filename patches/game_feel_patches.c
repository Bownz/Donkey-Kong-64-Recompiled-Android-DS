// Game feel (Android): small modern touches.
//
//  * Haptic Feedback extras (Input > Haptic Feedback): a soft tap for each banana and coin, a
//    strong pulse for a Golden Banana, a tick for each shot and orange (all from
//    changeCollectableCount below, the game's one place for collectable counts), a thump when
//    the Kong is hurt, and a beat while an instrument plays.
//  * Jump Assist (Input): a jump pressed a moment before landing happens on landing (input
//    buffering), and one pressed a moment after running off a ledge still jumps (coyote time).
//  * Smooth Running (Input): speed builds up and runs down over a few frames.
//  * Quick Turn (Input): the skid when reversing while running is cut to ~0.2 s (dust kept).
//
// game_feel_update runs once per frame from the camera's input stage (func_global_asm_8061D1FC,
// sound_options_patches.c), in adventure mode only.

#include "common_structs.h"
#include "debug_config.h"
#include "input.h"
#include "sound.h"
#include "options.h"

extern int gameIsInAdventureMode(void);
extern int gameIsInDKTVMode(void);
extern Actor *gCurrentPlayer;
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);   // the floor below a point
u8 modern_camera_free_state(Actor *kong);   // trees, vines (sound_options_patches.c)
extern s32 D_global_asm_80767CC0;          // the game's frame counter (jump buffer timestamps)
extern f32 D_global_asm_807531FC[];        // per Kong: walking top speed
extern f32 D_global_asm_80753250[];
extern f32 D_global_asm_807535CC[];        // per Kong: jump strength
extern s16 D_global_asm_807FD584;          // the index into those tables

extern u8 is_cutscene_active;
extern u8 current_character_index[];
extern u8 cc_player_index;
extern Maps current_map;
typedef struct {
    u8 pad0[0x2A];
    u16 unk2A;   // buttons held
    u16 unk2C;   // buttons newly pressed
    u8 pad2E[0x34 - 0x2E];
} GFInput;   // Struct807FD610 (sound_options_patches.c)
extern GFInput D_global_asm_807FD610[];
extern void playAnimation(Actor *arg0, s32 arg2);
extern void playSong(MUSIC_E arg0, f32 arg1);
extern void setFlag(s16 flagIndex, u8 newValue, u8 flagType);
extern u8 D_global_asm_807FC950[];   // PlayerProgress[4], 0x306 each (health at 0x2FB)

// The game's HUD counter entry (HUDDisplay, 0x30 bytes): only the fields used here.
typedef struct {
    u16 *actual_count_pointer;
    u8 pad4[0x2C - 0x4];
    u8 unk_2c;
    u8 unk_2d;
    u8 pad2E[2];
} GFHudItem;

#define GF_PROGRESS(player) (&D_global_asm_807FC950[(player) * 0x306])
#define GF_KONG_PROGRESS(player, kong) (&GF_PROGRESS(player)[(kong) * 0x5E])
#define GF_BANANAS(kp, level) (*(u16 *)&(kp)[0x0A + ((level) * 2)])
#define GF_BANANAS_FED(kp, level) (*(u16 *)&(kp)[0x26 + ((level) * 2)])

#define GF_JUMP_ASSIST 1
#define GF_QUICK_TURN 2
#define GF_SMOOTH_RUN 4
#define GF_POLISH 8
#define GF_MOVE_CHAIN 32
#define GF_AIR_CONTROL 64
#define GF_HIT_FEEL 128
#define GF_FAST_TRANSITIONS 256

// ---- Collectables ---------------------------------------------------------------------------

s32 func_global_asm_805FEF10(s32 *arg0);
u8 getLevelIndex(u8 map, u8 arg1);
void func_global_asm_80687C48(void);
s32 func_global_asm_806C8DE0(s32 playerIndex);
GFHudItem *func_global_asm_806F7FD0(u8 playerIndex);
u16 func_global_asm_806F8EDC(s32 HUDItemIndex, s32 playerIndex);
void func_global_asm_806F8278(u8 playerIndex);
s32 func_global_asm_807319D8(s32 arg0, s32 arg1, s32 arg2);

static s32 gf_instrument_frames = 0;   // frames left of the instrument beat
static s32 gf_instrument_kong = 0;

static void gf_collect_haptics(s32 item, s16 amount) {
    if ((amount == 0) || (amount > 50) || (amount < -50) || !gameIsInAdventureMode() || (is_cutscene_active != 0)) {
        return;   // refills, file loads, cutscene rewards
    }
    switch (item) {
        case 0:   // coloured bananas (a bunch is 5): a soft tap
        case 1:   // coins
            if (amount > 0) {
                recomp_haptic_pulse((amount >= 5) ? 0.22f : 0.12f, (amount >= 5) ? 45 : 25);
            }
            break;
        case 8:   // Golden Banana
            if (amount > 0) {
                recomp_haptic_pulse(0.75f, 260);
            }
            break;
        case 2:   // gun ammo (standard, homing): a tick per shot
        case 3:
            if (amount < 0) {
                recomp_haptic_pulse(0.20f, 30);
            }
            break;
        case 4:   // oranges: a slightly heavier tick per throw
            if (amount < 0) {
                recomp_haptic_pulse(0.25f, 40);
            }
            break;
        case 7:   // instrument energy: an instrument is being played
            if (amount < 0) {
                gf_instrument_frames = 75;   // ~2.5 s of beat
                gf_instrument_kong = current_character_index[0];
            }
            break;
    }
}

// @recomp: the game's own function, unchanged, plus the haptics above.
RECOMP_PATCH void changeCollectableCount(s32 HUDItemIndex, u8 playerIndex, s16 amount) {
    GFHudItem *HUDItem;
    u8 *sp48;
    s32 maxItemCapacity;
    s32 levelIndex;
    s32 flagIndex;
    s32 amountBefore;

    sp48 = GF_KONG_PROGRESS(playerIndex, func_global_asm_806C8DE0(playerIndex));
    HUDItem = func_global_asm_806F7FD0(playerIndex);
    if (((HUDItem[HUDItemIndex].unk_2d == 0) || (amount >= 0)) && (HUDItem[HUDItemIndex].unk_2c == 0)) {
        if ((HUDItemIndex != 0xA) && (HUDItemIndex != 0xC)) {
            if (playerIndex == 0) {
                gf_collect_haptics(HUDItemIndex, amount);
            }
            if (((*HUDItem[HUDItemIndex].actual_count_pointer) + amount) < 0) {
                *HUDItem[HUDItemIndex].actual_count_pointer = 0;
            } else {
                *HUDItem[HUDItemIndex].actual_count_pointer += amount;
                maxItemCapacity = func_global_asm_806F8EDC(HUDItemIndex, playerIndex);
                if (maxItemCapacity < (*HUDItem[HUDItemIndex].actual_count_pointer)) {
                    *HUDItem[HUDItemIndex].actual_count_pointer = maxItemCapacity;
                }
                if ((HUDItemIndex == 8) && (func_global_asm_805FEF10(&flagIndex) != 0)) {
                    setFlag(flagIndex, TRUE, FLAG_TYPE_PERMANENT);
                }
                if ((amount > 0) && (HUDItemIndex == 0)) {
                    levelIndex = getLevelIndex(current_map, 1);
                    amountBefore = GF_BANANAS(sp48, levelIndex) + GF_BANANAS_FED(sp48, levelIndex);
                    if ((amountBefore >= 75) && ((amountBefore - amount) < 75)) {
                        // Award banana medal
                        setFlag(func_global_asm_807319D8(PERMFLAG_ITEM_MEDAL_JAPES_DK, levelIndex, current_character_index[playerIndex]), TRUE, FLAG_TYPE_PERMANENT);
                        func_global_asm_80687C48();
                    }
                    if ((amountBefore == 100) && ((amountBefore - amount) < 100)) {
                        // Play 100 bananas collected jingle
                        playSong(0xAC, 1.0f);
                    }
                }
            }
        }
        if (HUDItemIndex == 8) {
            func_global_asm_806F8278(playerIndex);
        }
    }
}

// ---- Per frame ------------------------------------------------------------------------------

static s32 gf_last_health = -1000;
static s32 gf_air_frames = 0;           // frames since the Kong last stood on the ground
static s32 gf_last_state = -1;
static u8 gf_left_by_walking = FALSE;   // left the ground without jumping (walked off a ledge)

// Each Kong's instrument gets its own beat (frames within a 16-frame bar: strength).
static void gf_instrument_beat(void) {
    s32 t;
    f32 s = 0.0f;
    if (gf_instrument_frames <= 0) {
        return;
    }
    t = (75 - gf_instrument_frames) & 15;
    switch (gf_instrument_kong) {
        case 0:   // DK, bongos: two quick hits
            s = ((t == 0) || (t == 4)) ? 0.35f : 0.0f;
            break;
        case 1:   // Diddy, guitar: a strum every half bar
            s = ((t == 0) || (t == 8)) ? 0.25f : 0.0f;
            break;
        case 2:   // Lanky, trombone: one long swell
            s = (t == 0) ? 0.30f : 0.0f;
            break;
        case 3:   // Tiny, saxophone: off-beat
            s = ((t == 2) || (t == 6) || (t == 10)) ? 0.20f : 0.0f;
            break;
        default:  // Chunky, triangle: a light ding
            s = (t == 0) ? 0.18f : 0.0f;
            break;
    }
    if (s > 0.0f) {
        recomp_haptic_pulse(s, (gf_instrument_kong == 2) ? 220 : 50);
    }
    gf_instrument_frames--;
}

// Kong states where walking off a ledge or landing count (idle, running, skid, crouch).
static u8 gf_grounded_state(s32 state) {
    return (state == 0x0C) || (state == 0x0D) || (state == 0x0E) || (state == 0x3C);
}

// Starts a jump just as the game does from standing or running (func_global_asm_806E186C).
static void gf_start_jump(Actor *kong, PlayerAdditionalActorData *paad) {
    paad->unk38 = D_global_asm_807531FC[D_global_asm_807FD584];
    paad->unk30 = D_global_asm_80753250[D_global_asm_807FD584];
    paad->unk3C = 0;
    paad->unk54 = D_global_asm_807535CC[D_global_asm_807FD584];
    paad->unk50 = 0;
    kong->control_state = 0x17;
    kong->control_state_progress = 0;
    playAnimation(kong, 0x10);
    paad->unk58 = D_global_asm_80767CC0 - 0x1E;
}

// ---- Quick Turn ------------------------------------------------------------------------------
// The game's skid (Kong state 0x0E: the dust and its rumble when you reverse while running) is
// kept, but shorter: the Kong turns round during it, and after 9 frames (~0.3 s) it keeps a
// little speed, and the game's own "back on the ground" step (func_global_asm_806CFF9C, which
// also picks the right animation) puts it back into running.
void func_global_asm_806CFF9C(Actor *arg0);
static s32 gf_skid_frames = 0;
static u8 gf_pivoted = FALSE;   // a Quick Turn happened (Smooth Running lets the speed drop)

static s32 gf_quick_turn(Actor *kong, PlayerAdditionalActorData *paad, s32 state, s32 lastState) {
    f32 top;
    s32 diff;
    if (state != 0x0E) {
        gf_skid_frames = 0;
        return state;
    }
    if (lastState != 0x0E) {
        gf_skid_frames = 0;
    }
    gf_skid_frames++;
    // During the skid the Kong turns round towards the stick, a little more each frame (eased),
    // so it's already facing the new way when the skid ends, and its speed bleeds off.
    diff = ((((kong->unkEE & 0xFFF) - (kong->y_rotation & 0xFFF)) + 0x800) & 0xFFF) - 0x800;
    kong->y_rotation = (kong->y_rotation + (s32)((f32)diff * 0.3f)) & 0xFFF;
    kong->unkB8 *= 0.85f;
    if (gf_skid_frames < 9) {
        return state;
    }
    // ~0.3 s in: off the new way, from the speed the skid has left.
    top = D_global_asm_807531FC[D_global_asm_807FD584];
    if (top < 50.0f) {
        top = 150.0f;
    }
    kong->y_rotation = kong->unkEE & 0xFFF;
    paad->unkA = kong->y_rotation;
    paad->unkC = -1;
    paad->unkE = -1;
    if (kong->unkB8 < top * 0.25f) {
        kong->unkB8 = top * 0.25f;
    }
    func_global_asm_806CFF9C(kong);
    gf_pivoted = TRUE;
    gf_skid_frames = 0;
    return kong->control_state;
}

// ---- Movement Polish -------------------------------------------------------------------------
// Run lean: the Kong tilts into tight turns while running (its roll, z_rotation; up to ~11
// degrees, eased). Landing squash: a quick, subtle squash and stretch back after a big fall
// (the model's scale, put back exactly). Momentum jumps: a jump from a full run carries ~12%
// further than from standing.
static f32 gf_lean = 0.0f;
static u8 gf_leaning = FALSE;
static s32 gf_last_yaw = -1;
static s32 gf_squash_frames = 0;
static f32 gf_base_scale[3];

static void gf_squash_restore(Actor *kong) {
    if ((gf_squash_frames > 0) && (kong->animation_state != NULL)) {
        kong->animation_state->scale_x = gf_base_scale[0];
        kong->animation_state->scale_y = gf_base_scale[1];
        kong->animation_state->scale_z = gf_base_scale[2];
    }
    gf_squash_frames = 0;
}

static void gf_polish_off(Actor *kong) {
    if (gf_leaning) {
        kong->z_rotation = 0;
        gf_leaning = FALSE;
    }
    gf_lean = 0.0f;
    gf_squash_restore(kong);
}

static void gf_polish(Actor *kong, PlayerAdditionalActorData *paad, s32 state, u8 onGround, s32 landedAfter) {
    s32 yaw = kong->y_rotation & 0xFFF;
    s32 turn = 0;
    f32 top = D_global_asm_807531FC[D_global_asm_807FD584];
    f32 want = 0.0f;
    if (top < 50.0f) {
        top = 150.0f;
    }

    // Lean into turns while running.
    if (gf_last_yaw >= 0) {
        turn = ((yaw - gf_last_yaw + 0x800) & 0xFFF) - 0x800;
    }
    gf_last_yaw = yaw;
    if ((state == 0x0D) && onGround) {
        f32 speed = kong->unkB8 / top;
        if (speed > 1.0f) {
            speed = 1.0f;
        }
        want = (f32)turn * 3.0f * speed;
        if (want > 128.0f) {
            want = 128.0f;
        } else if (want < -128.0f) {
            want = -128.0f;
        }
    }
    gf_lean += (want - gf_lean) * 0.2f;
    if ((state == 0x0D) || (gf_lean > 2.0f) || (gf_lean < -2.0f)) {
        if ((state == 0x0D) || (state == 0x0C)) {
            kong->z_rotation = (s16)(((s32)gf_lean) & 0xFFF);
            gf_leaning = TRUE;
        } else if (gf_leaning) {
            kong->z_rotation = 0;   // jumping, attacking...: upright at once
            gf_leaning = FALSE;
            gf_lean = 0.0f;
        }
    } else if (gf_leaning) {
        kong->z_rotation = 0;
        gf_leaning = FALSE;
    }

    // Landing squash after a fall of half a second or more (not the Simian Slam, which has its
    // own landing).
    if (kong->animation_state != NULL) {
        if ((landedAfter >= 15) && (gf_squash_frames == 0) && (state != 0x1C) && !modern_camera_free_state(kong)) {
            gf_base_scale[0] = kong->animation_state->scale_x;
            gf_base_scale[1] = kong->animation_state->scale_y;
            gf_base_scale[2] = kong->animation_state->scale_z;
            gf_squash_frames = 1;
        }
        if (gf_squash_frames > 0) {
            // 8 frames: squashed to 85% at once, back to normal with a slight overshoot.
            static const f32 curve[8] = { 0.85f, 0.87f, 0.92f, 0.98f, 1.03f, 1.02f, 1.0f, 1.0f };
            f32 y = curve[gf_squash_frames - 1];
            f32 xz = 1.0f + ((1.0f - y) * 0.5f);
            kong->animation_state->scale_x = gf_base_scale[0] * xz;
            kong->animation_state->scale_y = gf_base_scale[1] * y;
            kong->animation_state->scale_z = gf_base_scale[2] * xz;
            gf_squash_frames++;
            if (gf_squash_frames > 8) {
                gf_squash_frames = 1;   // so restore writes the base back
                gf_squash_restore(kong);
            }
        }
    }

    // Momentum jump: the jump has just started from a run at (nearly) full speed.
    if ((state == 0x17) && (gf_last_state == 0x0D) && (kong->unkB8 > top * 0.8f)) {
        paad->unk38 *= 1.12f;   // the jump's top speed
        kong->unkB8 *= 1.12f;
    }
}

// ---- Wall bump ------------------------------------------------------------------------------
// Running into a wall at (nearly) full speed: a small rumble (Haptic Feedback). Detected by the
// Kong suddenly covering far less ground than its running speed would.
static f32 gf_last_x = 0.0f, gf_last_z = 0.0f;
static f32 gf_moved_ratio = 0.0f;   // usual ground covered per unit of speed while running
static s32 gf_bump_cooldown = 0;
static s32 gf_stuck_frames = 0;

static void gf_wall_bump(Actor *kong, s32 state, u8 onGround) {
    f32 dx = kong->x_position - gf_last_x;
    f32 dz = kong->z_position - gf_last_z;
    f32 moved = _sqrtf((dx * dx) + (dz * dz));
    f32 top = D_global_asm_807531FC[D_global_asm_807FD584];
    f32 speed = kong->unkB8;
    gf_last_x = kong->x_position;
    gf_last_z = kong->z_position;
    if (top < 50.0f) {
        top = 150.0f;
    }
    if (gf_bump_cooldown > 0) {
        gf_bump_cooldown--;
    }
    if ((state != 0x0D) || !onGround || (speed < top * 0.7f) || (moved > 100.0f)) {
        gf_stuck_frames = 0;
        return;
    }
    if (moved > 0.01f) {
        f32 ratio = moved / speed;
        if (ratio > gf_moved_ratio * 0.6f) {
            gf_moved_ratio += (ratio - gf_moved_ratio) * 0.1f;   // learn the normal rate
        }
    }
    if ((gf_moved_ratio > 0.0f) && (moved < speed * gf_moved_ratio * 0.25f)) {
        gf_stuck_frames++;
        if ((gf_stuck_frames == 2) && (gf_bump_cooldown == 0)) {
            recomp_haptic_pulse(0.35f, 60);
            gf_bump_cooldown = 30;
        }
    } else {
        gf_stuck_frames = 0;
    }
}

static f32 gf_prev_x = 0.0f, gf_prev_z = 0.0f;

static u8 gf_air_state(s32 state) {
    switch (state) {
        case 0x17:   // jump
        case 0x18:
        case 0x1D:   // backflip
        case 0x1E:   // falling
        case 0x3D:
        case 0x3E:   // high jump
            return TRUE;
    }
    return FALSE;
}

// ---- Move Chaining -------------------------------------------------------------------------
// A jump can cut short a punch or a running attack/roll on the ground, and keeps its speed
// (so a roll into a jump carries you further), like chaining moves in Odyssey.
static void gf_move_chain(Actor *kong, PlayerAdditionalActorData *paad, s32 state, u8 onGround, u16 pressed) {
    static s32 attackFrames = 0;
    f32 speed;
    if ((state == 0x26) || (state == 0x29) || (state == 0x2A)) {
        attackFrames++;
    } else {
        attackFrames = 0;
        return;
    }
    if (!(pressed & A_BUTTON) || !onGround || (attackFrames < 3)) {
        return;
    }
    speed = kong->unkB8;
    gf_start_jump(kong, paad);
    if (speed > paad->unk38) {
        paad->unk38 = speed;   // the jump's top speed: keep the attack's
    }
    kong->unkB8 = speed;
    attackFrames = 0;
}

// ---- Air Control ---------------------------------------------------------------------------
// In the air the Kong turns towards the stick much more readily (the N64 mostly commits you to
// the direction you jumped in); pulling back brakes instead of flipping round.
static void gf_air_control(Actor *kong, PlayerAdditionalActorData *paad, s32 state, u8 onGround) {
    s32 diff;
    if (onGround || !gf_air_state(state) || (paad->unk4 < 5.0f)) {
        return;
    }
    diff = ((((kong->unkEE & 0xFFF) - (kong->y_rotation & 0xFFF)) + 0x800) & 0xFFF) - 0x800;
    if ((diff > 0x500) || (diff < -0x500)) {
        kong->unkB8 *= 0.94f;   // pulling back: brake
        kong->y_rotation = (kong->y_rotation + (diff / 24)) & 0xFFF;
    } else {
        kong->y_rotation = (kong->y_rotation + (diff / 8)) & 0xFFF;
    }
}

// ---- Hit Feel -------------------------------------------------------------------------------
// When something near the Kong loses health (a punch, a slam, a shot landing): a short kick of
// the camera (game_feel_camera_kick, used by the Modern Camera) and a rumble.
extern u16 D_global_asm_807FBB34;
typedef struct {
    Actor *actor;
    s32 unk4;
} GFLoadedActor;
extern GFLoadedActor D_global_asm_807FB930[];

#define GF_TRACKED 64
static Actor *gf_tracked[GF_TRACKED];
static s16 gf_tracked_health[GF_TRACKED];
static s32 gf_stop_frames = 0;
static Actor *gf_stop_target = NULL;
static f32 gf_stop_kong[3], gf_stop_enemy[3];
static f32 gf_stop_kong_anim = 1.0f, gf_stop_enemy_anim = 1.0f;
static f32 gf_kick = 0.0f;

f32 game_feel_camera_kick(void) {
    return gf_kick;
}

static void gf_hit_feel(Actor *kong) {
    s32 i, j, n, count;
    Actor *a;
    static s32 recent = 0;   // frames left to watch for a hit after an attack
    gf_kick *= 0.75f;
    if (gf_kick < 0.01f) {
        gf_kick = 0.0f;
    }
    // Only watch while an attack can land: punches, kicks, rolls, the slam, an orange thrown,
    // or the gun out (its shots); the rest of the time, nothing's scanned.
    switch (kong->control_state) {
        case 0x1C: case 0x26: case 0x27: case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C:
            recent = 45;
            break;
    }
    if ((kong->PaaD != NULL) && (kong->PaaD->unk8C != 0)) {
        recent = 45;
    }
    if ((recent <= 0) && (gf_stop_frames <= 0)) {
        for (i = 0; i < GF_TRACKED; i++) {
            gf_tracked[i] = NULL;   // starts afresh next time (no false hits)
        }
        return;
    }
    recent--;
    if (gf_stop_frames > 0) {
        gf_stop_frames--;
        kong->x_position = gf_stop_kong[0];
        kong->y_position = gf_stop_kong[1];
        kong->z_position = gf_stop_kong[2];
        if (gf_stop_target != NULL) {
            gf_stop_target->x_position = gf_stop_enemy[0];
            gf_stop_target->y_position = gf_stop_enemy[1];
            gf_stop_target->z_position = gf_stop_enemy[2];
        }
        if (gf_stop_frames == 0) {
            if (kong->animation_state != NULL) {
                kong->animation_state->unk48 = gf_stop_kong_anim;
            }
            if ((gf_stop_target != NULL) && (gf_stop_target->animation_state != NULL)) {
                gf_stop_target->animation_state->unk48 = gf_stop_enemy_anim;
            }
            gf_stop_target = NULL;
        }
        return;
    }
    count = D_global_asm_807FBB34;
    if (count > 255) {
        count = 255;
    }
    n = 0;
    for (i = 0; i < count; i++) {
        f32 dx, dz, dy;
        s16 hp;
        a = D_global_asm_807FB930[i].actor;
        if ((a == NULL) || (a == kong)) {
            continue;
        }
        dx = a->x_position - kong->x_position;
        dz = a->z_position - kong->z_position;
        dy = a->y_position - kong->y_position;
        if (((dx * dx) + (dz * dz)) > (150.0f * 150.0f) || (dy > 120.0f) || (dy < -120.0f)) {
            continue;
        }
        hp = a->health;
        for (j = 0; j < GF_TRACKED; j++) {
            if (gf_tracked[j] == a) {
                break;
            }
        }
        if ((j < GF_TRACKED) && (hp < gf_tracked_health[j]) && (gf_tracked_health[j] > 0) &&
            (gf_tracked_health[j] - hp) < 100) {
            // A hit: a camera punch and a rumble (no freeze).
            gf_kick = 1.0f;
            recomp_haptic_pulse(0.45f, 70);
        }
        if (n < GF_TRACKED) {
            gf_tracked[n] = a;
            gf_tracked_health[n] = hp;
            n++;
        }
    }
    for (; n < GF_TRACKED; n++) {
        gf_tracked[n] = NULL;
    }
}

static void gf_hit_feel_off(Actor *kong) {
    if (gf_stop_frames > 0) {
        gf_stop_frames = 1;   // finish (restores the animation speeds)
        gf_hit_feel(kong);
    }
    gf_kick = 0.0f;
}

// ---- Fast Transitions ------------------------------------------------------------------------
// The fade to and from black when going through a loading zone (and other scripted fades) runs
// at 1.8x speed.
extern f32 loading_zone_transition_speed;

static void gf_fast_transitions(void) {
    static f32 lastSet = 0.0f;
    f32 v = loading_zone_transition_speed;
    if (v == 0.0f) {
        lastSet = 0.0f;
        return;
    }
    if ((v != lastSet) && (v > -1.6f) && (v < 1.6f)) {
        loading_zone_transition_speed = v * 1.8f;
        lastSet = loading_zone_transition_speed;
    }
}

void game_feel_update(void) {
    Actor *kong = gCurrentPlayer;
    PlayerAdditionalActorData *paad;
    s32 flags, state, health;
    u16 pressed;
    u8 onGround;
    s32 landedAfter;

    if (gameIsInAdventureMode() && !gameIsInDKTVMode() && (recomp_get_game_feel() & GF_FAST_TRANSITIONS)) {
        gf_fast_transitions();
    }
    if ((kong == NULL) || (is_cutscene_active != 0) || gameIsInDKTVMode() || !gameIsInAdventureMode()) {
        if (kong != NULL) {
            gf_polish_off(kong);
            gf_hit_feel_off(kong);
        }
        gf_last_health = -1000;
        gf_last_state = -1;
        gf_instrument_frames = 0;
        return;
    }
    paad = kong->PaaD;
    if (paad == NULL) {
        return;
    }
    flags = recomp_get_game_feel();
    state = kong->control_state;
    pressed = D_global_asm_807FD610[cc_player_index].unk2C;

    // Hurt: a thump, stronger for bigger hits.
    health = (s8)GF_PROGRESS(0)[0x2FB];
    if ((gf_last_health != -1000) && (health < gf_last_health)) {
        s32 lost = gf_last_health - health;
        recomp_haptic_pulse((lost >= 4) ? 0.8f : 0.55f, (lost >= 4) ? 220 : 150);
    }
    gf_last_health = health;

    gf_instrument_beat();

    // On the ground: the game's own test (the Kong within 4 units of the floor).
    onGround = (kong->y_position - kong->floor) < 4.0f;
    landedAfter = 0;
    if (onGround) {
        landedAfter = gf_air_frames;
        gf_air_frames = 0;
        gf_left_by_walking = FALSE;
    } else {
        if ((gf_air_frames == 0) && gf_grounded_state(gf_last_state) && (kong->y_velocity <= 0.0f)) {
            gf_left_by_walking = TRUE;   // dropped off without a jump
        }
        gf_air_frames++;
    }

    if (flags & GF_JUMP_ASSIST) {
        // Coyote time: up to 6 frames after walking off a ledge, A still jumps.
        if ((pressed & A_BUTTON) && !onGround && gf_left_by_walking && (gf_air_frames <= 6) &&
            (kong->y_velocity <= 0.0f) && (state != 0x17)) {
            gf_start_jump(kong, paad);
            gf_left_by_walking = FALSE;
        // Input buffer: A pressed while coming down (within ~8 frames of landing) is kept, so the
        // game's own jump check (which allows 15 frames from the press) jumps on landing.
        } else if ((pressed & A_BUTTON) && !onGround && (kong->y_velocity < 0.0f) &&
                   ((kong->y_position - kong->floor) < 60.0f) && !modern_camera_free_state(kong)) {
            paad->unk58 = D_global_asm_80767CC0 - 7;
        }
    }

    if (flags & GF_QUICK_TURN) {
        state = gf_quick_turn(kong, paad, state, gf_last_state);
        if (gf_pivoted) {
            gf_last_yaw = -1;   // no lean from the sudden turn
        }
    }
    // Smooth Running: the speed builds up over about a third of a second from standing (instead
    // of almost at once) and runs down over a few frames when you let go, so running has a bit
    // of weight. Only between standing and running (attacks, rolls and jumps set their own speed).
    if (flags & GF_SMOOTH_RUN) {
        static f32 lastSpeed = 0.0f;
        f32 top = D_global_asm_807531FC[D_global_asm_807FD584];
        f32 up, down;
        if (top < 50.0f) {
            top = 150.0f;
        }
        up = top * 0.09f;     // ~11 frames to full speed
        down = top * 0.2f;    // ~5 frames to a stop
        if (((state == 0x0C) || (state == 0x0D)) && ((gf_last_state == 0x0C) || (gf_last_state == 0x0D)) && onGround) {
            if (gf_pivoted) {
                // a Quick Turn: keep its speed drop
            } else if (kong->unkB8 > lastSpeed + up) {
                kong->unkB8 = lastSpeed + up;
            } else if (kong->unkB8 < lastSpeed - down) {
                kong->unkB8 = lastSpeed - down;
            }
        }
        lastSpeed = kong->unkB8;
        gf_pivoted = FALSE;
    }
    if (flags & GF_POLISH) {
        gf_polish(kong, paad, state, onGround, landedAfter);
    } else {
        gf_polish_off(kong);
    }
    gf_wall_bump(kong, state, onGround);
    {
        gf_prev_x = kong->x_position;
        gf_prev_z = kong->z_position;
    }
    if (flags & GF_MOVE_CHAIN) {
        gf_move_chain(kong, paad, state, onGround, pressed);
    }
    if (flags & GF_AIR_CONTROL) {
        gf_air_control(kong, paad, state, onGround);
    }
    if (flags & GF_HIT_FEEL) {
        gf_hit_feel(kong);
    } else {
        gf_hit_feel_off(kong);
    }
    gf_last_state = state;
}
