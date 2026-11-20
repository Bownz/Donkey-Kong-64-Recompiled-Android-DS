#include "common_structs.h"
#include "debug_config.h"
#include "input.h"
#include "sound.h"
#include "options.h"

extern s8 D_global_asm_80745840;
extern s8 D_global_asm_8074583C;
extern void func_global_asm_8060A398(s32 arg0);
extern void func_global_asm_80737B58(u8 arg0, u16 arg1);
RECOMP_DECLARE_EVENT(recomp_on_file_start());
RECOMP_DECLARE_EVENT(recomp_on_new_file_start());
RECOMP_DECLARE_EVENT(recomp_on_dirty_file_start());
RECOMP_DECLARE_EVENT(recomp_on_flag_check(s16 *flag, u8 *flag_type));
extern s32 dk64_android_tag_target; // android_hooks.c
extern f32 loading_zone_transition_speed;
extern u8 is_cutscene_active;
RECOMP_DECLARE_EVENT(recomp_on_flag_change(s16 *flag, u8 *target_state, u8 *flag_type));

void AlterVolumes(void) {
    s32 i;
    s8 new_bgm_vol;
    s8 new_sfx_vol;

    //@recomp: Music Volume
    new_bgm_vol = recomp_get_bgm_volume();
    if (D_global_asm_80745840 != new_bgm_vol) {
        D_global_asm_80745840 = new_bgm_vol;
        func_global_asm_8060A398(0);
        func_global_asm_8060A398(2);
    }
    new_sfx_vol = recomp_get_sfx_volume();
    if (D_global_asm_8074583C != new_sfx_vol) {
        D_global_asm_8074583C = new_sfx_vol;
        for (i = 0; i < 4; i++) {
            func_global_asm_80737B58(i, new_sfx_vol * 625);
        }
    }
}

extern u16 D_global_asm_80744734[];
extern u8 isFlagSet(s16 flagIndex, u8 flagType);
extern void setFlag(s16 flagIndex, u8 newValue, u8 flagType);
extern void func_global_asm_805FF378(Maps nextMap, s32 nextExit);
extern void func_global_asm_80712524(Maps newMap, s32 cutsceneIndex);
extern Maps current_map;
extern u8 game_mode;

// @recomp: Level Intro Story Skip reads
RECOMP_PATCH s32 func_global_asm_8068ABE0(s16 arg0) {
    s16 cutsceneIndex;
    s32 sp20;
    s32 found;
    s32 i;
    Maps map;
    s32 exit; // 20

    cutsceneIndex = -1;
    i = 0, found = 0;
    while (!found && i < 8) {
        if (current_map == D_global_asm_80744734[i]) {
            found = 1;
        } else {
            i++;
        }
    }
    if (found) {
        if (!isFlagSet(PERMFLAG_LEVEL_ENTERED_JAPES + i, FLAG_TYPE_PERMANENT)) {
            exit = 0;
            switch (arg0) {
                case 0x7:
                    map = MAP_HELM_LEVEL_INTROS_GAME_OVER;
                    cutsceneIndex = 0xF;
                    break;
                case 0x26:
                    map = MAP_HELM_LEVEL_INTROS_GAME_OVER;
                    cutsceneIndex = 0x10;
                    break;
                case 0x1A:
                    map = MAP_HELM_LEVEL_INTROS_GAME_OVER;
                    cutsceneIndex = 0x11;
                    break;
                case 0x1E:
                    map = MAP_HELM_LEVEL_INTROS_GAME_OVER;
                    cutsceneIndex = 0x12;
                    break;
                case 0x30:
                    map = MAP_HELM_LEVEL_INTROS_GAME_OVER;
                    cutsceneIndex = 0x13;
                    break;
                case 0x48:
                    map = MAP_HELM_LEVEL_INTROS_GAME_OVER;
                    cutsceneIndex = 0x14;
                    break;
                case 0x57:
                    map = MAP_HELM_LEVEL_INTROS_GAME_OVER;
                    cutsceneIndex = 0x15;
                    exit = 0x15;
                    break;
                case 0x11:
                    map = MAP_HELM_LEVEL_INTROS_GAME_OVER;
                    cutsceneIndex = 0x16;
                    break;
            }
            if (cutsceneIndex != -1) {
                setFlag(PERMFLAG_LEVEL_ENTERED_JAPES + i, TRUE, FLAG_TYPE_PERMANENT);
                //@recomp: change story_skip to the function read
                if (recomp_get_story_skip() == 2) {
                    func_global_asm_805FF378(arg0, exit);
                    return TRUE;
                } else {
                    func_global_asm_80712524(map, cutsceneIndex);
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

extern OSTime D_global_asm_807445B8;
extern u8 D_global_asm_80755350;
extern void func_global_asm_806C9AE0(void);
extern void func_global_asm_80731030(void);
extern void func_global_asm_8060DC3C(u8 fileIndex, s32 arg1);
extern u8 current_character_index[];
extern s32 func_global_asm_8060C6B8(s32 arg0, u8 arg1, u8 arg2, u8 fileIndex);
extern u8 current_file;
extern s32 D_global_asm_80755338;
extern s32 D_global_asm_8075533C;
extern void func_global_asm_805FF4D8(Maps map, s32 exit);

void fixHelmMedalsBug(void) {
    s32 i;
    
    if (isFlagSet(0x302, FLAG_TYPE_PERMANENT)) { // BoM Shut Down
        for (i = 0; i < 5; i++) {
            setFlag(0x4B + i, TRUE, FLAG_TYPE_TEMPORARY);
        }
    }
}

// @recomp: Intro Story Story Skip Read
RECOMP_PATCH void func_global_asm_807144B8(s8 arg0) {
    Maps map;
    s32 exit;

    D_global_asm_807445B8 = osGetTime();
    D_global_asm_80755350 = 0;
    func_global_asm_806C9AE0();
    func_global_asm_80731030(); // clearTemporaryFlags()
    func_global_asm_8060DC3C(arg0, 1);
    current_character_index[0] = 0; // DK
    //@recomp: change story_skip to the function read
    if (func_global_asm_8060C6B8(0xD, 0, 0, current_file) || (recomp_get_story_skip() > 0)) {
        // Flag: Isles: Escape Cutscene
        if (isFlagSet(PERMFLAG_CUTSCENE_ISLES_FTCS, FLAG_TYPE_PERMANENT)) {
            map = MAP_DK_ISLES_OVERWORLD;
            exit = 0;
        } else {
            map = MAP_TRAINING_GROUNDS;
            exit = 1;
        }
    } else {
        D_global_asm_80755338 = 1;
        map = MAP_DK_ISLES_DK_THEATRE;
        D_global_asm_8075533C = 0;
        exit = 0;
    }
    recomp_on_file_start();
    if (func_global_asm_8060C6B8(0xD, 0, 0, current_file)) {
        recomp_on_dirty_file_start();
        fixHelmMedalsBug();
    } else {
        recomp_on_new_file_start();
    }
    func_global_asm_805FF4D8(map, exit); // initMapChange()
    game_mode = GAME_MODE_ADVENTURE;
}

extern s32 D_global_asm_807FBB64;
extern int gameIsInDKTVMode(void);
extern int gameIsInAdventureMode(void);

u8 modern_cam_enabled(void);
u8 modern_camera_free_state(Actor *kong);

// @recomp: Camera Type
RECOMP_PATCH u8 func_global_asm_80621174(s32 arg0, PlayerAdditionalActorData *arg1, Actor *arg2) {
    // @recomp: Modern Camera: the game never pushes the camera behind the Kong (during attacks,
    // on trees and vines...); the Modern Camera's own gentle follow does that job.
    if (modern_cam_enabled()) {
        return FALSE;
    }
    // Formatting is off here for better manual formatting. Whitespace doesn't affect the match here.
    // clang-format off
    return (arg2->control_state == 0x5B) ||
           ((D_global_asm_807FBB64 & 0x04000004) && (arg2->unkB8 != 0.0f)) ||
           (D_global_asm_807FBB64 & 0x08000000) ||
           (arg1->unkAC & 4) ||
           ((arg1->unkAC & 8) && (arg2->unkB8 != 0.0f)) ||
           ((recomp_get_camera_type() == 1) && (arg2->unkB8 != 0.0f) && (gameIsInDKTVMode() == 0)) ||
           (arg1->unkF0_u8[3] == 0xA) ||
           (arg1->unkF0_u8[3] == 0xD) ||
           (arg1->unkF0_u8[3] == 5) ||
           (arg1->unkF0_u8[3] == 0xC) ||
           (arg1->unkFC && (arg2->unkB8 != 0.0f)) ||
           (arg2->control_state == 0x6E) ||
           (arg2->unk58 == 8);
    // clang-format on
}

extern Actor *gCurrentPlayer;
const u8 default_scheme_maps[] = {
    MAP_KROOL_BARREL_LANKY_MAZE,
    MAP_MAD_MAZE_MAUL_HARD,
    MAP_MAD_MAZE_MAUL_EASY,
    MAP_MAD_MAZE_MAUL_NORMAL,
    MAP_STASH_SNATCH_EASY,
    MAP_STASH_SNATCH_NORMAL,
    MAP_STASH_SNATCH_INSANE,
    MAP_STEALTHY_SNOOP_VERY_EASY,
    MAP_STEALTHY_SNOOP_EASY,
    MAP_STEALTHY_SNOOP_NORMAL,
    MAP_STEALTHY_SNOOP_HARD,
};

u8 analog_cam_enabled(void) {
    // Whether to use the analog camera control scheme or otherwise
    u32 i;
    
    for (i = 0; i < sizeof(default_scheme_maps); i++) {
        if (current_map == (default_scheme_maps[i])) {
            return FALSE;
        }
    }
    // Analog Camera (3) and Modern Camera (4) both put the camera on the right stick. Modern
    // Camera only in actual play (adventure mode): elsewhere (the intro and its "press start"
    // scenes, title, menus, demos) the game's own camera handling is left alone, since the
    // right-stick handling skips the scripted camera turns those scenes rely on.
    if (recomp_get_camera_type() == 4) {
        return gameIsInAdventureMode() != 0;
    }
    return recomp_get_camera_type() == 3;
}

// @recomp: Modern Camera: the Analog Camera's controls with smoothing, tilt instead of zoom,
// L to recenter and a gentle follow (modern_camera_input and modern_camera_frame below).
u8 modern_cam_enabled(void) {
    return analog_cam_enabled() && (recomp_get_camera_type() == 4);
}

const u8 banned_analog_states[] = {
    // First person
    2,
    3,
    4,
    5,
    0x2C, // Throwing Orange
    0x3C, // Crouching
    0x5F, // Putting away gun
    0x60, // Pulling out gun
    0x62, // Aiming Gun
    0x64, // Taking photo
    0x65, // Taking photo (underwater)
    0x67, // Instrument
};

// @recomp (Android): whether the camera may be turned by hand right now (not in DK TV mode or
// one of the states above). Used by touch drag, which works with every camera type.
static u8 camera_turn_allowed(void) {
    u32 i;

    if (gameIsInDKTVMode()) {
        return FALSE;
    }
    for (i = 0; i < sizeof(banned_analog_states); i++) {
        if (banned_analog_states[i] == gCurrentPlayer->control_state) {
            return FALSE;
        }
    }
    return TRUE;
}

u8 get_analog_allowed(void) {
    // When using the analog control scheme, whether the stick does anything
    u32 i;

    if (!analog_cam_enabled()) {
        return FALSE;
    }
    if (gameIsInDKTVMode()) {
        return FALSE;
    }
    for (i = 0; i < sizeof(banned_analog_states); i++) {
        if (banned_analog_states[i] == gCurrentPlayer->control_state) {
            return FALSE;
        }
    }
    return TRUE;
}

f32 recomp_apply_per_axis_deadzone(f32 value) {
    const f32 per_axis_deadzone = 0.2f;
    if (value > per_axis_deadzone) {
        return (value - per_axis_deadzone) / (1.0f - per_axis_deadzone);
    }
    else if (value < -per_axis_deadzone) {
        return (value + per_axis_deadzone) / (1.0f - per_axis_deadzone);
    }
    else {
        return 0.0f;
    }
}

void recomp_analog_camera_get(f32 *x, f32 *y) {
    float input_x, input_y;
    s32 inverted_x, inverted_y;
    recomp_get_right_analog_inputs(&input_x, &input_y);
    recomp_get_analog_inverted_axes(&inverted_x, &inverted_y);
    *x = recomp_apply_per_axis_deadzone(input_x) * (inverted_x ? 1.0f : -1.0f);
    *y = recomp_apply_per_axis_deadzone(input_y) * (inverted_y ? -1.0f : 1.0f);
}

typedef struct Struct807FD610 {
    s32 unk0; // Timer that ticks up once per frame
    f32 unk4; // Probably float
    f32 unk8; // Probably float
    f32 unkC; // Probably float
    f32 unk10[4];
    s16 unk20[4];
    s16 unk28; // Used
    u16 unk2A; // Used, controller button bitfield
    u16 unk2C; // Used, controller button bitfield
    s8 unk2E; // Used
    s8 unk2F; // Used
    u8 unk30; // Used
    u8 unk31;
    s16 unk32;
} Struct807FD610;

extern Struct807FD610 D_global_asm_807FD610[];
extern PlayerAdditionalActorData *extra_player_info_pointer;
extern u8 cc_player_index;

// @recomp: Orbit Camera
RECOMP_PATCH void func_global_asm_806EA200(void) {
    s8 change = 0x2D;
    s8 require_new_input = 1;
    s8 cooldown = 0xB;
    s32 invX = 0;
    s32 invY = 0;
    s32 cam_type;

    if (analog_cam_enabled()) {
        return;
    }
    cam_type = recomp_get_camera_type();
    if (cam_type == 2) {
        cooldown = 5;
        change = 5 * (cooldown - 2);
        require_new_input = 0;
    }
    recomp_get_analog_inverted_axes(&invX, &invY);
    if (invX) {
        change = -change;
    }
    CameraPaad *CaaD = extra_player_info_pointer->unk104->AAD_as_array[0];
    if (((D_global_asm_807FD610[cc_player_index].unk2C & L_CBUTTONS) || (!require_new_input)) && (CaaD->unkF1 < 3)) {
        CaaD->unkB0 -= change;
        CaaD->unkF1 = cooldown;
    }
}

RECOMP_PATCH void func_global_asm_806EA26C(void) {
    s8 change = 0x2D;
    s8 require_new_input = 1;
    s8 cooldown = 0xB;
    s32 invX = 0;
    s32 invY = 0;
    s32 cam_type;

    if (analog_cam_enabled()) {
        return;
    }
    cam_type = recomp_get_camera_type();
    if (cam_type == 2) {
        cooldown = 5;
        change = 5 * (cooldown - 2);
        require_new_input = 0;
    }
    recomp_get_analog_inverted_axes(&invX, &invY);
    if (invX) {
        change = -change;
    }
    CameraPaad *CaaD = extra_player_info_pointer->unk104->AAD_as_array[0];
    if (((D_global_asm_807FD610[cc_player_index].unk2C & R_CBUTTONS) || (!require_new_input)) && (CaaD->unkF1 < 3)) {
        CaaD->unkB0 += change;
        CaaD->unkF1 = cooldown;
    }
}

void func_global_asm_8062217C(Actor*, s16);
s16 playSound(s16 arg0, s32 arg1, f32 arg2, f32 arg3, u8 arg4, u8 arg5);
extern s32 D_global_asm_807FBB68;

// @recomp: Zoom out
RECOMP_PATCH void func_global_asm_806EA0A4(void) {
    PlayerAdditionalActorData *PaaD; // TODO: Probably not actually PaaD
    u16 temp;
    u8 phi_a0;

    if (analog_cam_enabled()) {
        return;
    }
    PaaD = extra_player_info_pointer->unk104->PaaD;
    if ((D_global_asm_807FBB64 & 1) && !(D_global_asm_807FBB68 & 2)) {
        phi_a0 = 2;
    } else {
        phi_a0 = 3;
    }
    if ((PaaD->unkEF) && ((D_global_asm_807FD610[cc_player_index].unk2C & (U_CBUTTONS | D_CBUTTONS)) == D_CBUTTONS) && ((PaaD->unkEE == phi_a0) || (PaaD->unkFC == 0))) {
        PaaD->unkF0_u8[0] = PaaD->unkEF;
        if (PaaD->unkEF < phi_a0) {
            PaaD->unkEF++;
        } else {
            PaaD->unkEF = 1;
        }
        func_global_asm_8062217C(extra_player_info_pointer->unk104, PaaD->unkEF);
        PaaD->unkEE = PaaD->unkEF;
        if (PaaD->unkEF != PaaD->unkF0) {
            temp = PaaD->unk8;
            if ((((temp == 0)) || (temp >= 0x96)) && (PaaD->unkF0_u8[3] != 2)) {
                playSound(0x27, 0x7FFF, 63.0f, 1.0f, 0, 0);
            }
        }
        PaaD->unkF0_u8[1] = 0xB;
        PaaD->unkF4_u8[2] = 0xF;
    }
}

void func_global_asm_8062217C(Actor*, s16);
int getFrameDelta(void);

// ---- Modern Camera ------------------------------------------------------------------------
// Feel aimed at Super Mario Odyssey / Donkey Kong Bananza. In normal play (and on trees and
// vines) the Modern Camera places the camera itself every frame, instead of steering the game's
// camera code (most of which is still assembly and kept re-adding its own behaviour):
//  - it orbits a focus point just above the Kong, which trails the Kong slightly and holds its
//    height while the Kong jumps (only following once it lands higher, drops below, rises well
//    above or stays in the air);
//  - the right stick turns it with a short acceleration and an easing response curve; up/down
//    tilts it lower/higher; L swings it behind the Kong; after ~1 s without input it drifts
//    gently back behind the Kong while running (not when running towards the camera);
//  - it keeps a set distance (a little closer when low, further when high), traced against the
//    level's collision from the focus out: blocked, it moves right up to the obstacle at once
//    (no minimum: the Kong fades when the camera is right up against it), and eases back out;
//  - on vines it stays steady on the middle of the swing, further out, facing the way the Kong
//    was heading.
// Input is read in func_global_asm_8061D1FC (modern_camera_input); the camera is placed from
// the camera update's outputs (modern_camera_frame, called from func_global_asm_8061DBD4 in
// patches_interpolation.c). Cutscenes, bosses and the game's other special cameras are left to
// the game, blended in and out.
#define MC_ANGLE_WRAP(a) (((((s32)(a)) + 0x800) & 0xFFF) - 0x800)

f32 func_global_asm_80612790(s16 arg0);
f32 func_global_asm_80612794(s16 arg0);
s16 func_global_asm_80665DE0(f32 arg0, f32 arg1, f32 arg2, f32 arg3);
u8 func_global_asm_80671E00(f32 arg0, f32 arg1, f32 arg2, f32 arg3, s16 *arg4, s16 *arg5, u8 arg6, u16 arg7);
s32 func_global_asm_80667110(f32 arg0, f32 arg1, f32 *arg2);
extern u8 is_cutscene_active;

#define MC_LOWEST 6.0f              // the lowest the camera goes: just above the ground

static f32 mc_yaw = 0.0f;           // the camera's direction from the Kong (game angle units)
static u8 mc_yaw_valid = FALSE;
static f32 mc_yaw_velocity = 0.0f;
static f32 mc_tilt_velocity = 0.0f;
static f32 mc_height = 55.0f;       // camera height above the framed ground (below 0: under the Kong)
static u8 mc_height_valid = FALSE;
static f32 mc_distance = 0.0f;      // the usual distance (0: not set yet)
static f32 mc_reach = 1.0f;         // how far out along the line the camera is (1 = all the way)
static f32 mc_idle = 0.0f;
static u8 mc_recentering = FALSE;
static u8 mc_follow_strong = FALSE;   // after L: keep behind the Kong until you turn the camera
static s32 mc_recenter_frame = 0;     // L: frames into the swing
static f32 mc_recenter_yaw = 0.0f;    // L: where the swing started
static f32 mc_recenter_turn = 0.0f;   // L: how far it turns
static f32 mc_recenter_height = 0.0f;
static s32 mc_blocked_frames = 0;     // frames since the camera was last pushed in by a wall
static u8 mc_active_now = FALSE;      // the Modern Camera placed the camera last frame
static u8 mc_floating = FALSE;        // not standing on the ground (swimming, climbing, vines, long falls)
static f32 mc_eye_x = 0.0f, mc_eye_y = 0.0f, mc_eye_z = 0.0f;
static u8 mc_eye_valid = FALSE;
static f32 mc_vine_offset = 0.0f;     // vines: your turn from the game's vine view
static f32 mc_vine_hoff = 0.0f;       // vines: your height change from the game's vine view
static u8 mc_on_vine = FALSE;
static f32 mc_vine_blend = 0.0f;      // 0 = our usual view, 1 = the game's vine view

static f32 mc_weight = 0.0f;        // 0 = the game's camera, 1 = the Modern Camera
static f32 mc_focus_x = 0.0f, mc_focus_y = 0.0f, mc_focus_z = 0.0f;
static u8 mc_focus_valid = FALSE;
static f32 mc_ground_y = 0.0f;     // the height the camera should frame (the floor the Kong stands on)
static f32 mc_frame_y = 0.0f;      // the height it frames now: eases to mc_ground_y
static f32 mc_frame_speed = 0.0f;
static s32 mc_air_frames = 0;
static u8 mc_faded = FALSE;

// How the game's angles map to directions (checked once against func_global_asm_80665DE0,
// which the game itself uses to turn a camera position into its angle): 1-8, or -1 if no match.
static s32 mc_convention = 0;

static f32 mc_abs(f32 v) {
    return v < 0.0f ? -v : v;
}

static f32 mc_clamp(f32 v, f32 lo, f32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Fine control near the centre of the stick, full speed at the edge.
static f32 mc_curve(f32 v) {
    return (0.35f * v) + (0.65f * v * mc_abs(v));
}

static void mc_direction_with(s32 convention, s16 angle, f32 *dx, f32 *dz) {
    f32 a = func_global_asm_80612794(angle);
    f32 b = func_global_asm_80612790(angle);
    switch (convention) {
        case 1: *dx = a; *dz = b; break;
        case 2: *dx = b; *dz = a; break;
        case 3: *dx = -a; *dz = b; break;
        case 4: *dx = a; *dz = -b; break;
        case 5: *dx = -b; *dz = a; break;
        case 6: *dx = b; *dz = -a; break;
        case 7: *dx = -a; *dz = -b; break;
        default: *dx = -b; *dz = -a; break;
    }
}

static u8 mc_convention_matches(s32 convention, s16 angle) {
    f32 dx, dz;
    s32 diff;
    mc_direction_with(convention, angle, &dx, &dz);
    diff = MC_ANGLE_WRAP(func_global_asm_80665DE0(0.0f, 0.0f, dx * 100.0f, dz * 100.0f) - angle);
    return (diff > -12) && (diff < 12);
}

static u8 mc_ready(void) {
    s32 c;
    if (mc_convention == 0) {
        mc_convention = -1;
        for (c = 1; c <= 8; c++) {
            if (mc_convention_matches(c, 0x155) && mc_convention_matches(c, 0x9AB) && mc_convention_matches(c, 0xE38)) {
                mc_convention = c;
                break;
            }
        }
        recomp_printf("Modern Camera: angle convention %d%s\n", mc_convention,
            mc_convention < 0 ? " (none matched: Modern Camera off)" : "");
    }
    return mc_convention > 0;
}

static u8 mc_vine_state(Actor *kong) {
    return (kong != NULL) && ((kong->control_state == 0x57) || (kong->control_state == 0x58));
}

// Like get_analog_allowed, but crouching (and throwing an orange) doesn't hand the camera back
// to the game: it kept the right-stick camera off there, which turned the view.
static u8 mc_allowed(void) {
    u32 i;

    if (!modern_cam_enabled() || gameIsInDKTVMode() || (is_cutscene_active != 0)) {
        return FALSE;
    }
    // Not in the intro, menus, story scenes and endings, nor while the game has taken the
    // controls away (scripted scenes in normal levels, e.g. during the intro).
    switch (current_map) {
        case 0x1C: // Helm level intros / game over
        case 0x28: // Nintendo logo
        case 0x4C: // DK Rap
        case 0x50: // main menu
        case 0x98: // Helm intro story
        case 0x99: // DK Theatre
        case 0xAC: // K. Rool's rock intro story
        case 0xC6: // Training Grounds end sequence
        case 0xD0: // bloopers ending
        case 0xD5: // K. Lumsy ending
            return FALSE;
    }
    if ((extra_player_info_pointer != NULL) && (extra_player_info_pointer->unk1F4 & 0x20)) {
        return FALSE;
    }
    for (i = 0; i < sizeof(banned_analog_states); i++) {
        if ((banned_analog_states[i] == gCurrentPlayer->control_state) &&
            (banned_analog_states[i] != 0x3C) && (banned_analog_states[i] != 0x2C)) {
            return FALSE;
        }
    }
    return TRUE;
}

// Kong states where the game normally locks the camera, but the Modern Camera stays free:
// swinging on a vine (0x57, 0x58) and climbing a tree or pole (0x59, 0x5A).
u8 modern_camera_free_state(Actor *kong) {
    if (kong == NULL) {
        return FALSE;
    }
    switch (kong->control_state) {
        case 0x57:
        case 0x58:
        case 0x59:
        case 0x5A:
            return TRUE;
    }
    return FALSE;
}

// The camera's input, once per frame (from func_global_asm_8061D1FC).
static void modern_camera_input(Actor *camera, CameraPaad *CaaD, s32 touchMoved, f32 touchX, f32 touchY) {
    f32 stickX, stickY, dt, sensitivity, blend, yawStep, tilt, speed, follow;
    s32 behind, diff;
    u8 onTree;

    if (!mc_yaw_valid) {
        mc_yaw = (f32)(CaaD->unkB2 & 0xFFF);
        mc_yaw_valid = TRUE;
    }
    recomp_analog_camera_get(&stickX, &stickY);
    dt = (f32)getFrameDelta();
    sensitivity = (f32)recomp_get_analog_cam_sensitivity();
    blend = mc_clamp(0.3f * dt, 0.0f, 1.0f);

    // Turning: ease the speed towards the stick's target speed (a short acceleration).
    mc_yaw_velocity += ((mc_curve(stickX) * sensitivity * -5.7f) - mc_yaw_velocity) * blend;
    if ((stickX == 0.0f) && (mc_abs(mc_yaw_velocity) < 0.05f)) {
        mc_yaw_velocity = 0.0f;
    }
    yawStep = mc_yaw_velocity * dt;
    if (touchMoved) {
        yawStep += touchX;   // touch drags stay 1:1
    }

    // L: swing back to the normal view behind the Kong (and the normal height), like Odyssey's
    // recenter: a fixed, short ease-in-out (~0.4 s), gentle at both ends. Turning the camera
    // yourself cancels it.
    if (D_global_asm_807FD610[cc_player_index].unk2C & L_TRIG) {
        mc_vine_offset = 0.0f;           // L also forgets your vine view
        mc_vine_hoff = 0.0f;
        mc_recentering = TRUE;
        mc_follow_strong = TRUE;
        mc_recenter_frame = 0;
        mc_recenter_yaw = mc_yaw;
        mc_recenter_turn = (f32)MC_ANGLE_WRAP((gCurrentPlayer->y_rotation & 0xFFF) - (s32)mc_yaw);
        mc_recenter_height = mc_height;
    }
    if ((stickX != 0.0f) || (touchMoved && (touchX != 0.0f))) {
        mc_recentering = FALSE;
        mc_follow_strong = FALSE;
        mc_idle = 0.0f;
    } else {
        mc_idle += dt;
    }

    // Vines: the game's own vine view (modern_camera_frame), turned and raised by however much
    // you've changed it on a vine, remembered for every vine until you press L.
    mc_on_vine = mc_vine_state(gCurrentPlayer);
    // Climbing a tree or pole: no automatic turning (the Kong turns as it moves round the trunk).
    onTree = (gCurrentPlayer->control_state == 0x59) || (gCurrentPlayer->control_state == 0x5A);
    if (mc_on_vine) {
        mc_recentering = FALSE;
    }

    // The camera's angle equals the Kong's facing when it's right behind (func_global_asm_8061DB64).
    behind = gCurrentPlayer->y_rotation & 0xFFF;
    diff = MC_ANGLE_WRAP(behind - (s32)mc_yaw);
    if (mc_recentering) {
        f32 t, e;
        mc_recenter_frame++;
        t = mc_clamp((f32)mc_recenter_frame / 12.0f, 0.0f, 1.0f);
        e = t * t * (3.0f - (2.0f * t));
        mc_yaw = mc_recenter_yaw + (mc_recenter_turn * e);
        mc_height = mc_recenter_height + ((55.0f - mc_recenter_height) * e);
        mc_yaw_velocity = 0.0f;
        mc_tilt_velocity = 0.0f;
        if (t >= 1.0f) {
            mc_recentering = FALSE;
        }
    } else if (mc_follow_strong && !mc_on_vine && !onTree) {
        // After L: stays behind the Kong as it runs and turns (about 4x the normal follow, no
        // waiting), until you turn the camera yourself. Running back towards the camera (more
        // than ~135 degrees off) doesn't spin it round.
        speed = gCurrentPlayer->unkB8;
        follow = mc_clamp(speed / 100.0f, 0.0f, 1.0f);
        if ((follow > 0.02f) && (diff > -0x600) && (diff < 0x600)) {
            yawStep += diff * 0.05f * follow * dt;
        }
    } else if ((mc_idle > 50.0f) && !mc_on_vine && !onTree) {
        // Gentle follow while running; never while running towards the camera (more than
        // ~120 degrees off), so turning round to face it doesn't spin the view.
        speed = gCurrentPlayer->unkB8;
        follow = mc_clamp(speed / 150.0f, 0.0f, 1.0f);
        if ((follow > 0.05f) && (diff > -0x555) && (diff < 0x555)) {
            yawStep += diff * 0.012f * follow * dt;
        }
        // Like Odyssey, while you run without touching the camera it also drifts back to the
        // usual height, slowly (standing on the ground only; vines keep your vine height).
        if ((mc_idle > 90.0f) && !mc_floating && !mc_on_vine && (follow > 0.2f)) {
            mc_height += (55.0f - mc_height) * 0.008f * follow * dt;
        }
    }
    if (mc_on_vine) {
        mc_vine_offset = (f32)MC_ANGLE_WRAP((s32)(mc_vine_offset + yawStep));
        yawStep = 0.0f;
    }
    mc_yaw += yawStep;
    while (mc_yaw < 0.0f) {
        mc_yaw += 4096.0f;
    }
    while (mc_yaw >= 4096.0f) {
        mc_yaw -= 4096.0f;
    }

    // Tilting: up/down moves the camera lower/higher around the Kong.
    mc_tilt_velocity += ((mc_curve(stickY) * sensitivity * 1.0f) - mc_tilt_velocity) * (blend * 0.7f);
    if ((stickY == 0.0f) && (mc_abs(mc_tilt_velocity) < 0.05f)) {
        mc_tilt_velocity = 0.0f;
    }
    tilt = mc_tilt_velocity * dt;
    if (touchMoved) {
        tilt += touchY;
    }
    // Resistance near the ends, like Odyssey: going down it slows more and more as it nears the
    // ground (and the Kong); going up it slows near the top.
    if ((tilt < 0.0f) && (mc_height < 70.0f) && !mc_floating) {
        f32 r = mc_clamp((mc_height - MC_LOWEST) / (70.0f - MC_LOWEST), 0.0f, 1.0f);
        tilt *= 0.12f + (0.88f * r * r * (3.0f - (2.0f * r)));
    } else if ((tilt > 0.0f) && (mc_height > 150.0f)) {
        f32 r = mc_clamp((200.0f - mc_height) / 50.0f, 0.0f, 1.0f);
        tilt *= 0.12f + (0.88f * r * r * (3.0f - (2.0f * r)));
    }
    if (mc_on_vine) {
        mc_vine_hoff = mc_clamp(mc_vine_hoff + tilt, -120.0f, 150.0f);
    } else if (!mc_recentering) {
        // Standing, the camera stops just above the ground; swimming, climbing or hanging, it can
        // go well below the Kong to look up at it.
        if (tilt < 0.0f) {
            mc_height = (mc_height + tilt < (mc_floating ? -150.0f : MC_LOWEST)) ? (mc_floating ? -150.0f : (mc_height < MC_LOWEST ? mc_height : MC_LOWEST)) : (mc_height + tilt);
        } else {
            mc_height = mc_clamp(mc_height + tilt, -150.0f, 200.0f);
        }
    }

    // The game's own camera state follows ours (the Kong's movement is relative to it, and the
    // game starts from it again when it takes over).
    CaaD->unkB2 = ((s32)mc_yaw) & 0xFFF;
    CaaD->unkB0 = 0;
}

// Whether something solid is at this point: a wall or object (the level's collision, as the
// game's own camera search tests it; small, so the camera slips through fences and railings),
// or the ground (the point is under a floor: slopes, hills, ramps, ledges; the wall test
// doesn't see floors). The game's floor lookup finds the first floor below a point, so it's
// asked from a little above: a floor found within 4 units under the point or anywhere above
// it means the point is in (or just at) the ground.
static u8 mc_blocked(f32 x, f32 y, f32 z) {
    s16 hitA = 0, hitB = 0xFFF;
    f32 floorY = y + 40.0f;
    if (func_global_asm_80671E00(x, y, z, 5.0f, &hitA, &hitB, 1U, 0xBCU) != 0) {
        return TRUE;
    }
    if (func_global_asm_80667110(x, z, &floorY) && (floorY <= y + 40.0f) && (floorY > y - 4.0f)) {
        return TRUE;
    }
    return FALSE;
}

// The game's own camera visibility test, for a camera at this angle, distance and height: true
// if the Kong can't be seen from there. It works from the camera's own state, so that's set to
// the values to test and put back afterwards.
s32 func_global_asm_8062133C(Actor*, Actor*, f32*, f32*, f32*, f32);

static u8 mc_hidden_at(Actor *camera, CameraPaad *AAD, s16 yaw, f32 distance, f32 height) {
    f32 savedA0 = AAD->unkA0;
    s16 savedB2 = AAD->unkB2;
    f32 savedHeight = camera->distance_from_floor;
    f32 x, y, z;
    s32 hidden;

    AAD->unkA0 = distance;
    AAD->unkB2 = yaw;
    camera->distance_from_floor = height;
    hidden = func_global_asm_8062133C(camera, AAD->unk0, &x, &y, &z, -1.0f) == 1;
    AAD->unkA0 = savedA0;
    AAD->unkB2 = savedB2;
    camera->distance_from_floor = savedHeight;
    return hidden;
}

// The Kong fades when the camera is right up against it (Odyssey does the same with Mario):
// solid from 45 units out, a quarter at the closest. Actor opacity (0x128) is only touched while
// fading, and put back to full afterwards.
static void mc_fade(f32 distance) {
    f32 t;
    if (gCurrentPlayer == NULL) {
        return;
    }
    if (distance < 45.0f) {
        t = mc_clamp((distance - 8.0f) / 37.0f, 0.0f, 1.0f);
        gCurrentPlayer->shadow_opacity = (s16)(64.0f + (191.0f * t));
        mc_faded = TRUE;
    } else if (mc_faded) {
        gCurrentPlayer->shadow_opacity = 0xFF;
        mc_faded = FALSE;
    }
}

// Tunnels and corridors: a tunnel's ceiling is usually only scenery (no collision for the floor
// lookup or the wall test to find), so it's recognised by its walls instead: walls close on
// both sides of the Kong (within 100 units each way), along any of four directions (checked one
// direction per frame, at about head-and-a-half height so low fences and railings don't count).
// In a tight space the camera stays low and a little closer, under any ceiling, like the
// game's own camera there; it eases in and out (mc_tight).
static f32 mc_tight = 0.0f;
static f32 mc_run_tilt = 0.0f;    // height change running up or down slopes
static u8 mc_tight_pairs = 0;     // bit per direction: walls both ways
static u8 mc_tight_dir = 0;
static u8 mc_tight_logged = FALSE;

static f32 mc_wall_within(f32 x, f32 y, f32 z, f32 dx, f32 dz, f32 range) {
    s16 hitA = 0, hitB = 0xFFF;
    f32 d;
    for (d = 10.0f; d <= range; d += 10.0f) {
        if (func_global_asm_80671E00(x + (dx * d), y, z + (dz * d), 5.0f, &hitA, &hitB, 1U, 0xBCU) != 0) {
            return d;
        }
    }
    return -1.0f;
}

static f32 mc_tight_a[4], mc_tight_b[4];   // wall distances each way, per direction (-1: none)
static s32 mc_tight_log_frames = 0;

static void mc_update_tight(Actor *target) {
    f32 dx, dz, a, b;
    s32 angle = (mc_tight_dir * 0x200) & 0xFFF;   // 0, 45, 90, 135 degrees
    f32 y = target->y_position + 40.0f;
    s32 k, best;
    mc_direction_with(mc_convention, angle, &dx, &dz);
    // Measured out to 200 each way (for the log); tight is walls within 100 on both sides.
    a = mc_wall_within(target->x_position, y, target->z_position, dx, dz, 200.0f);
    b = mc_wall_within(target->x_position, y, target->z_position, -dx, -dz, 200.0f);
    mc_tight_a[mc_tight_dir] = a;
    mc_tight_b[mc_tight_dir] = b;
    if ((a >= 0.0f) && (b >= 0.0f) && (a <= 100.0f) && (b <= 100.0f)) {
        mc_tight_pairs |= (1 << mc_tight_dir);
    } else {
        mc_tight_pairs &= ~(1 << mc_tight_dir);
    }
    mc_tight_dir = (mc_tight_dir + 1) & 3;
    // Held for a second and a half after the walls were last seen both sides, so a tunnel whose
    // width varies doesn't flick the camera up and down.
    {
        static s32 hold = 0;
        if (mc_tight_pairs != 0) {
            hold = 45;
        } else if (hold > 0) {
            hold--;
        }
        mc_tight += (((hold > 0) ? 1.0f : 0.0f) - mc_tight) * 0.08f;
    }
    if ((mc_tight_pairs != 0) != mc_tight_logged) {
        mc_tight_logged = (mc_tight_pairs != 0);
        recomp_printf("Modern Camera: tight space %s (map 0x%02X)\n", mc_tight_logged ? "on" : "off", current_map);
    }
    // Every 2 seconds: the narrowest walls-on-both-sides gap around the Kong, for tuning.
    if (++mc_tight_log_frames >= 60) {
        mc_tight_log_frames = 0;
        best = -1;
        for (k = 0; k < 4; k++) {
            if ((mc_tight_a[k] >= 0.0f) && (mc_tight_b[k] >= 0.0f) &&
                ((best < 0) || ((mc_tight_a[k] + mc_tight_b[k]) < (mc_tight_a[best] + mc_tight_b[best])))) {
                best = k;
            }
        }
        if (best >= 0) {
            recomp_printf("Modern Camera: walls %d / %d (dir %d, map 0x%02X, Kong state 0x%02X)\n",
                (s32)mc_tight_a[best], (s32)mc_tight_b[best], best, current_map, target->control_state);
        }
    }
}

// The game's own locked-camera spots (where it takes the camera and frames the scene itself):
// CameraPaad 0xAC bit 4, or a map whose camera always stays behind the Kong (map flags 0x08000000) or can't be
// turned (0x40). The Modern Camera hands over to the game there. Changes are logged.
static u8 mc_game_locked(CameraPaad *AAD) {
    static s32 lastLocked = -1;
    // (Bit 8, the game's "camera zone", isn't used: DK Isles' tunnels are such zones, and the
    // camera should stay free there.)
    u8 locked = ((AAD->unkAC & 0x4) != 0) || ((D_global_asm_807FBB64 & 0x08000040) != 0);
    if (locked != lastLocked) {
        if ((lastLocked != -1) || locked) {
            recomp_printf("Modern Camera: game camera %s (map 0x%02X, zone %d, camera flags 0x%08X, map flags 0x%08X)\n",
                locked ? "locked" : "free", current_map, AAD->unk8, AAD->unkAC, D_global_asm_807FBB64);
        }
        lastLocked = locked;
    }
    return locked;
}

// Places the camera: called from the camera update with its eye and look-at outputs.
void modern_camera_frame(Actor *camera, CameraPaad *AAD, f32 *eyeX, f32 *eyeY, f32 *eyeZ, f32 *atX, f32 *atY, f32 *atZ) {
    Actor *target = AAD->unk0;
    f32 kongY, wantY, dx, dz, distance, wantX, wantYEye, wantZ, lineX, lineY, lineZ, length, t, lo, hi, mid, reach;
    f32 camX, camY, camZ, w, frameTarget, startX, startY, startZ;
    s32 i;
    u8 active;

    active = (is_cutscene_active == 0) && mc_allowed() && (target != NULL) &&
        (target == gCurrentPlayer) && !(AAD->unkAC & 0x2000) && !mc_game_locked(AAD) &&
        ((AAD->unkF3 == 1) || (AAD->unkF3 == 5) || (AAD->unkF3 == 12)) && mc_ready();
    // (Camera mode 2 is the game's fixed camera for tight spots such as tunnel entrances: left
    // to the game, blended in and out.)
    if (active != mc_active_now) {
        recomp_printf("Modern Camera: %s (map 0x%02X, camera mode %d, Kong state 0x%02X, cutscene %d)\n",
            active ? "on" : "off", current_map, AAD->unkF3, (target != NULL) ? target->control_state : 0xFF,
            is_cutscene_active);
    }
    // Taking over (entering an area, after a cutscene or after the game's own camera was in
    // charge): start from where the game put the camera, so walking forward goes where the game
    // intended (not straight back out through the door you came in by).
    {
        static s32 lastMap = -1;
        if (active && (!mc_active_now || (current_map != lastMap)) && (target != NULL) && mc_ready()) {
            mc_yaw = (f32)(func_global_asm_80665DE0(target->x_position, target->z_position, *eyeX, *eyeZ) & 0xFFF);
            mc_yaw_valid = TRUE;
            mc_yaw_velocity = 0.0f;
            mc_recentering = FALSE;
            mc_idle = 0.0f;
            mc_focus_valid = FALSE;
            mc_eye_valid = FALSE;
            mc_reach = 1.0f;
        }
        lastMap = current_map;
    }
    mc_active_now = active;
    if (active) {
        if (!mc_yaw_valid) {
            mc_yaw = (f32)(AAD->unkB2 & 0xFFF);
            mc_yaw_valid = TRUE;
        }
        if (!mc_height_valid) {
            mc_height = 55.0f;
            mc_height_valid = TRUE;
        }
        if (mc_distance <= 0.0f) {
            mc_distance = AAD->unkA4;
            if ((mc_distance < 110.0f) || (mc_distance > 200.0f)) {
                mc_distance = 150.0f;
            }
        }
    }
    mc_weight += ((active ? 1.0f : 0.0f) - mc_weight) * 0.2f;
    if (!active) {
        mc_fade(1000.0f);
    }
    if (mc_weight < 0.01f) {
        mc_weight = 0.0f;
        mc_focus_valid = FALSE;
        mc_eye_valid = FALSE;
        return;
    }
    if (!mc_height_valid || (mc_distance <= 0.0f)) {
        return;   // not set up yet (only blending out)
    }
    if ((target == NULL) || (mc_convention <= 0)) {
        return;
    }

    // The framed height: where the Kong last stood. Jumping doesn't move the camera; it only
    // follows in the air if the Kong rises well above (60), drops below, or stays off the
    // ground (~1.5 s: swimming, falling, flying). Trees and vines follow the Kong.
    // Standing, it's the floor under the Kong (which doesn't change when it jumps, so neither
    // taking off nor landing moves the camera; tiny bumps under 3 units are ignored).
    kongY = target->y_position;
    if ((kongY - target->floor) < 4.0f) {
        mc_air_frames = 0;
    } else {
        mc_air_frames++;
    }
    mc_floating = modern_camera_free_state(target) || (mc_air_frames > 45);
    if (!mc_floating && (mc_height < MC_LOWEST)) {
        mc_height += (MC_LOWEST - mc_height) * 0.15f;   // back on the ground: ease back up
    }
    if (!mc_focus_valid || mc_floating) {
        mc_ground_y = kongY;
    } else if (mc_air_frames == 0) {
        if (mc_abs(target->floor - mc_ground_y) >= 3.0f) {
            mc_ground_y = target->floor;
        }
    } else if (kongY < mc_ground_y) {
        mc_ground_y = kongY;
    }

    // In the air, like Odyssey: a normal jump (up to ~30 units) doesn't move the camera; higher
    // ones (Z jumps, backflips...) lift it by half the extra height, so the Kong stays nicely in
    // frame, and it settles back down after landing (the same easing as for ledges).
    frameTarget = mc_ground_y;
    if ((mc_air_frames > 0) && (mc_air_frames <= 45) && !modern_camera_free_state(target) &&
        (kongY > mc_ground_y + 30.0f)) {
        frameTarget = mc_ground_y + ((kongY - mc_ground_y - 30.0f) * 0.5f);
    }
    // Jumping up onto a higher ledge: once the Kong is over it (the floor under it is the
    // ledge), the view already starts rising towards it, so most of the climb happens in the air
    // and the landing has little left to do.
    if ((mc_air_frames > 0) && (mc_air_frames <= 45) && !modern_camera_free_state(target) &&
        (target->floor > mc_ground_y + 3.0f) && (target->floor < mc_ground_y + 300.0f)) {
        f32 ledge = mc_ground_y + ((target->floor - mc_ground_y) * 0.7f);
        if (ledge > frameTarget) {
            frameTarget = ledge;
        }
    }

    // Landing on a higher (or lower) ledge, the whole view, camera and focus together, eases to
    // the new height (speeding up gently, then settling over about two thirds of a second), like
    // Odyssey, instead of snapping. Trees, vines, long falls and swimming follow more directly.
    if (!mc_focus_valid) {
        mc_frame_y = mc_ground_y;
        mc_frame_speed = 0.0f;
    } else if ((target->control_state == 0x59) || (target->control_state == 0x5A)) {
        // Climbing a tree or pole: follow the climb gently, not each pull up the trunk.
        mc_frame_y += (mc_ground_y - mc_frame_y) * 0.06f;
        mc_frame_speed = 0.0f;
    } else if (modern_camera_free_state(target) || (mc_air_frames > 45)) {
        mc_frame_y += (mc_ground_y - mc_frame_y) * 0.3f;
        mc_frame_speed = 0.0f;
    } else {
        f32 gap = frameTarget - mc_frame_y;
        mc_frame_speed += ((gap * 0.16f) - mc_frame_speed) * 0.25f;
        mc_frame_speed = mc_clamp(mc_frame_speed, -8.0f, 8.0f);
        mc_frame_y += mc_frame_speed;
        if ((mc_abs(gap) < 0.5f) && (mc_abs(mc_frame_speed) < 0.5f)) {
            mc_frame_y = frameTarget;
            mc_frame_speed = 0.0f;
        }
        // Landed well above the view (a big step): catch up faster, but still smoothly, so the
        // view never sits inside the ledge for long (the collision keeps it out of the ledge).
        if ((mc_air_frames == 0) && (mc_frame_y < mc_ground_y - 40.0f)) {
            mc_frame_y += ((mc_ground_y - 40.0f) - mc_frame_y) * 0.35f;
        }
    }

    // The focus trails the Kong slightly (lunges and attacks don't jolt the view); on a vine it
    // barely follows the swing, so it settles on the middle of the arc. Warps snap.
    wantY = mc_frame_y + 18.0f;
    if (!mc_focus_valid || (!mc_vine_state(target) && ((mc_abs(mc_ground_y - mc_frame_y) > 300.0f) ||
        (mc_abs(target->x_position - mc_focus_x) > 150.0f) || (mc_abs(target->z_position - mc_focus_z) > 150.0f)))) {
        mc_frame_y = mc_ground_y;
        mc_frame_speed = 0.0f;
        mc_focus_x = target->x_position;
        mc_focus_y = mc_frame_y + 18.0f;
        mc_focus_z = target->z_position;
        mc_focus_valid = TRUE;
        mc_reach = 1.0f;
    } else if ((target->control_state == 0x59) || (target->control_state == 0x5A)) {
        // Climbing: the Kong shifts round the trunk and bobs as it climbs; the view stays put
        // and only drifts after it, so it doesn't sway with every move.
        mc_focus_x += (target->x_position - mc_focus_x) * 0.03f;
        mc_focus_y += (wantY - mc_focus_y) * 0.06f;
        mc_focus_z += (target->z_position - mc_focus_z) * 0.03f;
    } else if (mc_vine_state(target)) {
        mc_focus_x += (target->x_position - mc_focus_x) * 0.04f;
        mc_focus_y += (wantY - mc_focus_y) * 0.05f;
        mc_focus_z += (target->z_position - mc_focus_z) * 0.04f;
    } else {
        // Look-ahead, a light one like Odyssey's: while running, the view leans a little (up
        // to 14 units) the way the Kong is running, easing in and out over about a second, so
        // there's a bit more room ahead. Standing, attacking or jumping, it's centred on the Kong.
        static f32 lead = 0.0f;
        f32 want = 0.0f, fx, fz;
        if ((target->control_state == 0x0D) && (mc_air_frames == 0)) {
            want = 14.0f * mc_clamp(target->unkB8 / 150.0f, 0.0f, 1.0f);
        }
        lead += (want - lead) * 0.035f;
        mc_direction_with(mc_convention, target->y_rotation & 0xFFF, &fx, &fz);   // points behind
        mc_focus_x += ((target->x_position - (fx * lead)) - mc_focus_x) * 0.35f;
        mc_focus_y = wantY;   // already eased (mc_frame_y)
        mc_focus_z += ((target->z_position - (fz * lead)) - mc_focus_z) * 0.35f;
    }

    // Where the camera wants to be: out along its angle at the usual distance (a little closer
    // when low, further when high, further on vines), at its height above the framed ground.
    // Like Odyssey: high, it's a little further out; tilted down towards the ground it comes in
    // closer and closer, down to just above the ground near the Kong, looking up.
    if (mc_height >= 40.0f) {
        distance = mc_distance * (0.85f + (0.3f * mc_clamp((mc_height - 40.0f) / 160.0f, 0.0f, 1.0f)));
    } else if (mc_floating) {
        distance = mc_distance * 0.85f;   // swimming/climbing: orbiting below the Kong too
    } else {
        f32 r = mc_clamp((mc_height - MC_LOWEST) / (40.0f - MC_LOWEST), 0.0f, 1.0f);
        distance = mc_distance * (0.45f + (0.40f * r * r * (3.0f - (2.0f * r))));
    }
    if (mc_on_vine) {
        distance *= 1.2f;
    }
    // Tunnels and corridors (mc_update_tight): low (at most 30 above the ground, so ~level with
    // the Kong's head) and at most 110 out. Tilting down still works.
    if (mc_on_vine) {
        mc_tight += (0.0f - mc_tight) * 0.08f;
    } else {
        mc_update_tight(target);
    }
    // Running feel (Odyssey-like, all eased):
    //  - at speed the camera pulls out a little (up to 8%) and comes back in as you slow;
    //  - running downhill it rises a little (up to 25) to show the way down; uphill it drops a
    //    little (up to 12) to look up the slope.
    {
        static f32 pull = 0.0f, tilt = 0.0f, slope = 0.0f, lastFloor = 0.0f, lastX = 0.0f, lastZ = 0.0f;
        f32 wantPull = 0.0f, wantTilt = 0.0f;
        f32 mx = target->x_position - lastX;
        f32 mz = target->z_position - lastZ;
        f32 moved = _sqrtf((mx * mx) + (mz * mz));
        if ((target->control_state == 0x0D) && (mc_air_frames == 0)) {
            wantPull = 0.08f * mc_clamp((target->unkB8 - 100.0f) / 100.0f, 0.0f, 1.0f);
            if ((moved > 0.5f) && (moved < 40.0f)) {
                slope += (mc_clamp((target->floor - lastFloor) / moved, -1.0f, 1.0f) - slope) * 0.1f;
            }
            wantTilt = (slope < 0.0f) ? (-slope * 50.0f) : (-slope * 24.0f);
            wantTilt = mc_clamp(wantTilt, -12.0f, 25.0f);
        } else {
            slope *= 0.9f;
        }
        lastFloor = target->floor;
        lastX = target->x_position;
        lastZ = target->z_position;
        pull += (wantPull - pull) * 0.03f;
        tilt += (wantTilt - tilt) * 0.03f;
        distance *= 1.0f + pull;
        mc_run_tilt = tilt;
    }
    {
        f32 height = mc_height + mc_run_tilt;
        if (mc_tight > 0.01f) {
            if (height > 30.0f) {
                height += (30.0f - height) * mc_tight;
            }
            if (distance > 110.0f) {
                distance += (110.0f - distance) * mc_tight;
            }
        }
        mc_direction_with(mc_convention, ((s32)mc_yaw) & 0xFFF, &dx, &dz);
        wantX = mc_focus_x + (dx * distance);
        wantYEye = mc_frame_y + height;
    }
    wantZ = mc_focus_z + (dz * distance);

    // Vines: the game's own vine view (which it has just placed: the eye and look-at passed in),
    // turned and raised by your remembered vine changes; blended in over ~half a second.
    if (mc_vine_state(target)) {
        f32 gx = *eyeX - *atX;
        f32 gz = *eyeZ - *atZ;
        f32 gDist = _sqrtf((gx * gx) + (gz * gz));
        s32 vineYaw = (func_global_asm_80665DE0(*atX, *atZ, *eyeX, *eyeZ) + (s32)mc_vine_offset) & 0xFFF;
        f32 vx, vz, b;
        mc_direction_with(mc_convention, vineYaw, &vx, &vz);
        mc_vine_blend += (1.0f - mc_vine_blend) * 0.12f;
        b = mc_vine_blend;
        wantX += ((*atX + (vx * gDist)) - wantX) * b;
        wantYEye += ((*eyeY + mc_vine_hoff) - wantYEye) * b;
        wantZ += ((*atZ + (vz * gDist)) - wantZ) * b;
        mc_focus_x += (*atX - mc_focus_x) * b;
        mc_focus_y += (*atY - mc_focus_y) * b;
        mc_focus_z += (*atZ - mc_focus_z) * b;
        distance += (gDist - distance) * b;
        mc_yaw = (f32)vineYaw;   // letting go carries on from the vine view
    } else {
        mc_vine_blend = 0.0f;
    }

    // Traced from the Kong's head out (not from the focus, which can trail inside a ledge just
    // climbed): the first blocked point (narrowed to ~1 unit) limits how far out the camera goes.
    startX = target->x_position;
    startY = kongY + 18.0f;
    startZ = target->z_position;
    lineX = wantX - startX;
    lineY = wantYEye - startY;
    lineZ = wantZ - startZ;
    length = distance + 1.0f;
    reach = 1.0f;
    for (t = 10.0f / length; ; t += 10.0f / length) {
        if (t > 1.0f) {
            t = 1.0f;
        }
        if (mc_blocked(startX + lineX * t, startY + lineY * t, startZ + lineZ * t)) {
            lo = t - (10.0f / length);
            hi = t;
            if (lo <= 0.0f) {
                reach = 0.0f;
            } else {
                for (i = 0; i < 4; i++) {
                    mid = (lo + hi) * 0.5f;
                    if (mc_blocked(startX + lineX * mid, startY + lineY * mid, startZ + lineZ * mid)) {
                        hi = mid;
                    } else {
                        lo = mid;
                    }
                }
                reach = lo;
            }
            break;
        }
        if (t >= 1.0f) {
            break;
        }
    }
    // Then the game's own visibility test (func_global_asm_8062133C, which its camera uses to
    // keep the Kong in view; it knows about ceilings, which the floor lookup doesn't): if the
    // Kong can't be seen from there, the camera comes in, 12 units at a time.
    {
        s16 yawNow = ((s32)mc_yaw) & 0xFFF;
        f32 height = wantYEye - mc_frame_y;
        f32 d = distance * reach;
        s32 tries = 0;
        while ((d > 12.0f) && (tries < 10) && mc_hidden_at(camera, AAD, yawNow, d, height)) {
            d -= 12.0f;
            tries++;
        }
        // Hidden all the way in means the test doesn't apply here: ignore it rather than glue
        // the camera to the Kong.
        if ((tries > 0) && (d > 12.0f)) {
            reach = mc_clamp(d / length, 8.0f / length, reach);
        }
    }

    // Blocked, the camera slides in over a couple of frames (never more than a few units past
    // the obstacle); once it's been clear for a moment, it eases back out. No flicker along walls.
    reach = mc_clamp(reach, 8.0f / length, 1.0f);
    if (reach < mc_reach - 0.001f) {
        mc_blocked_frames = 0;
        mc_reach += (reach - mc_reach) * 0.5f;
        if (mc_reach > reach + (6.0f / length)) {
            mc_reach = reach + (6.0f / length);
        }
    } else {
        mc_blocked_frames++;
        if (mc_blocked_frames > 8) {
            mc_reach += (reach - mc_reach) * 0.06f;
        }
    }

    camX = startX + lineX * mc_reach;
    camY = startY + lineY * mc_reach;
    camZ = startZ + lineZ * mc_reach;

    // Floors and ceilings (the wall test above doesn't see them), with the game's own floor
    // lookup (the first floor below a point):
    //  - a floor under the camera keeps it at least 6 units above (climbing, swimming, low
    //    angles; slopes are also caught by the trace, mc_blocked);
    //  - a surface directly above the Kong, below the camera, is a ceiling (or an upper floor):
    //    the camera comes down under it.
    {
        f32 floorY = camY + 1.0f;
        f32 aboveY = camY + 1.0f;
        if (func_global_asm_80667110(camX, camZ, &floorY) && (floorY <= camY + 1.0f) && (camY < floorY + 6.0f)) {
            camY = floorY + 6.0f;
        }
        if (func_global_asm_80667110(target->x_position, target->z_position, &aboveY) && (aboveY <= camY + 1.0f) &&
            (aboveY > kongY + 30.0f)) {
            camY = aboveY - 15.0f;
            if (camY < kongY + 18.0f) {
                camY = kongY + 18.0f;
            }
        }
        // Under cover (a bridge, an overhang, a roof): the lowest solid surface found above the
        // Kong, the camera and halfway between (looked for from well above) keeps the camera
        // under it. Away from the Kong, a surface only counts as cover if there's ground below
        // it too (a bridge or an overhang); with nothing below, it's the ground itself (a hill or
        // slope rising behind the Kong), so the camera is kept above it instead, not under.
        {
            f32 cover = 100000.0f;
            f32 ground = -100000.0f;
            f32 probe, below;
            f32 px[3], pz[3];
            s32 k;
            px[0] = target->x_position;               pz[0] = target->z_position;
            px[1] = (target->x_position + camX) * 0.5f; pz[1] = (target->z_position + camZ) * 0.5f;
            px[2] = camX;                              pz[2] = camZ;
            for (k = 0; k < 3; k++) {
                probe = kongY + 400.0f;
                if (!func_global_asm_80667110(px[k], pz[k], &probe) || (probe > kongY + 400.0f)) {
                    continue;
                }
                if (k == 0) {
                    if ((probe > kongY + 30.0f) && (probe < cover)) {
                        cover = probe;   // above the Kong's head: a roof
                    }
                    continue;
                }
                below = probe - 5.0f;
                if (func_global_asm_80667110(px[k], pz[k], &below) && (below < probe - 20.0f)) {
                    // Ground under it: a bridge or an overhang (if it's above the Kong's head).
                    if ((probe > kongY + 30.0f) && (probe < cover)) {
                        cover = probe;
                    }
                } else if (k == 2) {
                    ground = probe;   // the ground under the camera
                }
            }
            if ((cover < 100000.0f) && (camY > cover - 20.0f)) {
                camY = cover - 20.0f;
                if (camY < kongY + 18.0f) {
                    camY = kongY + 18.0f;
                }
            }
            // Only a little ground above the camera (a slope it has sunk into); a surface far
            // above is the top of a wall or cliff the camera is passing behind, which the wall
            // trace deals with: jumping up onto it would make the camera pop over walls.
            if ((camY < ground + 8.0f) && (ground < camY + 40.0f)) {
                camY = ground + 8.0f;   // a slope behind the Kong: stay above it
            }
        }
    }
    mc_fade(distance * mc_reach);

    // A final light smoothing, so small corrections blend instead of stepping; off the moment
    // a wall pushes the camera in (so it can't lag into it) and for big jumps (warps).
    if (!mc_eye_valid || (mc_blocked_frames == 0) || (mc_abs(camX - mc_eye_x) > 100.0f) ||
        (mc_abs(camY - mc_eye_y) > 100.0f) || (mc_abs(camZ - mc_eye_z) > 100.0f)) {
        mc_eye_x = camX;
        mc_eye_y = camY;
        mc_eye_z = camZ;
        mc_eye_valid = TRUE;
    } else {
        mc_eye_x += (camX - mc_eye_x) * 0.55f;
        mc_eye_y += (camY - mc_eye_y) * 0.55f;
        mc_eye_z += (camZ - mc_eye_z) * 0.55f;
    }
    camX = mc_eye_x;
    camY = mc_eye_y;
    camZ = mc_eye_z;

    // Blended with the game's camera while handing over.
    w = mc_weight;
    *eyeX += (camX - *eyeX) * w;
    *eyeY += (camY - *eyeY) * w;
    *eyeZ += (camZ - *eyeZ) * w;
    *atX += (mc_focus_x - *atX) * w;
    *atY += (mc_focus_y - *atY) * w;
    *atZ += (mc_focus_z - *atZ) * w;
    // The game rolls its camera with the Kong; the Kong's lean into turns (game_feel_patches.c)
    // shouldn't roll the view.
    if ((w >= 0.99f) && ((target->control_state == 0x0C) || (target->control_state == 0x0D))) {
        camera->z_rotation = 0;
    }
    if (w >= 0.99f) {
        camera->position.f[0] = *eyeX;
        camera->position.f[1] = *eyeY;
        camera->position.f[2] = *eyeZ;
        AAD->unkA0 = distance * mc_reach;
        AAD->unkA4 = AAD->unkA0;
    }
}

// ---- Shoot Gun With (Input) ---------------------------------------------------------------
// The game turns the controller's buttons into what the Kong reads (both held and newly
// pressed) with this function, once per frame. While the gun is out (in hand, or aiming), ZR (the N64's R) can fire too, or
// instead of B: 0 B (as the N64), 1 ZR only, 2 both.
RECOMP_PATCH u16 func_global_asm_806DFF34(u16 arg0, u16 *arg1) {
    u16 i;
    u16 j;
    u16 result;
    s32 mode;

    j = 0;
    i = 0;
    while (*arg1) {
        j |= *arg1;
        if (*arg1++ & arg0) {
            i |= *arg1;
        }
        arg1++;
    }
    result = (i | (arg0 & ~j));

    {
        static s32 gunWas = -1;
        s32 gunNow = (gCurrentPlayer != NULL) && (gCurrentPlayer->PaaD != NULL) && (gCurrentPlayer->PaaD->unk8C != 0);
        if (gunNow != gunWas) {
            if (gunWas != -1) {
                recomp_printf("Gun: %s\n", gunNow ? "out" : "away");
            }
            gunWas = gunNow;
        }
    }
    // The gun is out: the Kong's "holding the gun" flag (PlayerAdditionalActorData 0x8C, what
    // the game checks to pull the gun out or put it away), or one of the gun states (aiming...).
    if ((gCurrentPlayer != NULL) && (((gCurrentPlayer->PaaD != NULL) && (gCurrentPlayer->PaaD->unk8C != 0)) ||
        ((gCurrentPlayer->control_state >= 0x60) && (gCurrentPlayer->control_state <= 0x63)))) {
        mode = recomp_get_gun_button();
        if (mode != 0) {
            u8 r = (result & R_TRIG) != 0;
            if (mode == 1) {
                result &= ~B_BUTTON;
            }
            if (r) {
                result |= B_BUTTON;
            }
        }
    }
    return result;
}

// ---- Haptic Feedback -----------------------------------------------------------------------
// Small rumbles for game feel, like Odyssey (Input > Haptic Feedback; recomp_haptic_pulse checks
// the setting, scales by Rumble Strength, and uses the phone's vibrator with touch controls):
//  - landing after a jump or fall: a light tap, a little more after a long fall;
//  - the Simian Slam (Kong state 0x1C) hitting the ground: clearly stronger;
//  - skidding round (Kong state 0x0E, when the dust kicks up).
// States from the session log (Camera: ... Kong state lines). Checked once per frame from the
// camera update, with any camera type, in actual play only.
#define HF_STATE_SKID 0x0E
#define HF_STATE_SLAM 0x1C

static s32 hf_air = 0;           // frames in the air (-1: too long to count, e.g. swimming)
static s32 hf_last_state = -1;

void game_feel_update(void);   // game_feel_patches.c

static void modern_haptics_update(void) {
    Actor *kong = gCurrentPlayer;
    s32 state, ms;
    f32 strength;

    if ((kong == NULL) || (is_cutscene_active != 0) || gameIsInDKTVMode() || !gameIsInAdventureMode()) {
        hf_air = 0;
        hf_last_state = -1;
        return;
    }
    state = kong->control_state;

    // Skid: as the game starts it (the dust).
    if ((state == HF_STATE_SKID) && (hf_last_state != HF_STATE_SKID) && (hf_last_state >= 0)) {
        recomp_haptic_pulse(0.25f, 70);
    }

    // Simian Slam: when it hits the ground (the slam state ends).
    if ((hf_last_state == HF_STATE_SLAM) && (state != HF_STATE_SLAM)) {
        recomp_haptic_pulse(0.55f, 120);
        hf_air = 0;
    }

    // Landing.
    if (((kong->y_position - kong->floor) > 4.0f) && !modern_camera_free_state(kong)) {
        if (hf_air >= 0) {
            hf_air++;
            if (hf_air > 150) {
                hf_air = -1;   // 5 s: swimming or similar, not a jump
            }
        }
    } else {
        if ((hf_air >= 8) && (state != HF_STATE_SLAM)) {
            strength = 0.10f + ((f32)hf_air * 0.005f);
            if (strength > 0.35f) {
                strength = 0.35f;
            }
            ms = 40 + hf_air;
            if (ms > 100) {
                ms = 100;
            }
            recomp_haptic_pulse(strength, ms);
        }
        hf_air = 0;
    }
    if (state == HF_STATE_SLAM) {
        hf_air = 0;   // the slam's own pulse covers its landing
    }
    hf_last_state = state;
}

RECOMP_PATCH void func_global_asm_8061D1FC(Actor* arg0) {
    CameraPaad* CaaD;
    f32 temp_f2;
    f32 var_f0;
    f32 var_f14;
    f32 dRStickX, dRStickY, ratio;

    CaaD = arg0->CaaD;
    modern_haptics_update();
    game_feel_update();
    temp_f2 = D_global_asm_807FD610[CaaD->unkFB].unk2F * 0.5;
    if (!(CaaD->unkAC & 0x2000) && !(D_global_asm_807FBB64 & 0x40)) {
        if ((CaaD->unk8) && ((s32) CaaD->unk8 < 0x9B) && (CaaD->unkEF)) {
            if (( CaaD->unkA == 0) || ((s32)  CaaD->unkA >= 0x9B)) {
                CaaD->unkF0 = CaaD->unkEE;
            }
            func_global_asm_8062217C(arg0, 1);
            CaaD->unkAC |= 8;
        } else {
            if ((CaaD->unkA) && ((s32) CaaD->unkA < 0x9B)) {
                if (CaaD->unkEF) {
                    if ((CaaD->unkF0) && (CaaD->unkEF != CaaD->unkF0)) {
                        func_global_asm_8062217C(arg0, CaaD->unkF0);
                    }
                    CaaD->unkAC &= ~8;
                }
            }
        }
    }
    if (temp_f2 > 20.0f) {
        var_f14 = temp_f2;
    } else {
        if (temp_f2 < -20.0f) {
            var_f0 = temp_f2;
        } else {
            var_f0 = 0.0f;
        }
        var_f14 = var_f0;
    }
    CaaD->unk98 = ((var_f14 - CaaD->unk98) * 0.05) + CaaD->unk98;
    CaaD->unkA0 += ((CaaD->unkA4 - CaaD->unkA0) * 0.2);
    if (CaaD->unkAC & 0x10) {
        arg0->distance_from_floor += ((300.0f - arg0->distance_from_floor) * 0.2);
    } else {
        arg0->distance_from_floor += ((CaaD->unkB8 - arg0->distance_from_floor) * 0.2);
    }
    // @recomp (Android): dragging a finger on the touch screen turns the camera (and moves it up
    // and down), with any camera type. The app returns the drag since the last frame, already
    // in the camera's units (recomp_touch_camera_get, src/game/recomp_api.cpp).
    f32 touchCamX = 0.0f, touchCamY = 0.0f;
    s32 touchCamMoved = recomp_touch_camera_get(&touchCamX, &touchCamY) && camera_turn_allowed();
    // Modern Camera also stays in charge in the game's fixed camera mode (2: trees, vines...).
    // Each change of camera mode or Kong state is logged, to find any place this misbehaves.
    {
        static s32 lastMode = -1, lastState = -1;
        if ((CaaD->unkF3 != lastMode) || (gCurrentPlayer->control_state != lastState)) {
            lastMode = CaaD->unkF3;
            lastState = gCurrentPlayer->control_state;
            recomp_printf("Camera: mode %d, Kong state 0x%02X, map 0x%02X\n", lastMode, lastState, current_map);
        }
    }
    if (mc_allowed() && mc_active_now) {
        modern_camera_input(arg0, CaaD, touchCamMoved, touchCamX, touchCamY);
        return;
    }
    if ((analog_cam_enabled() && get_analog_allowed()) || touchCamMoved) {
        if (analog_cam_enabled() && get_analog_allowed()) {
            recomp_analog_camera_get(&dRStickX, &dRStickY);
            dRStickX *= recomp_get_analog_cam_sensitivity() * -5.7f * getFrameDelta();
            dRStickY *= recomp_get_analog_cam_sensitivity() * 1.0f * getFrameDelta();
        } else {
            dRStickX = 0.0f;
            dRStickY = 0.0f;
        }
        if (touchCamMoved) {
            dRStickX += touchCamX;
            dRStickY += touchCamY;
        }
        if (CaaD->unkF3 != 2) {
            if (dRStickX != 0.0f) {
                CaaD->unkB0 = 0;
                CaaD->unkB2 += dRStickX;
                CaaD->unkF1 = 1; // Makes things snappy
            }
            if (dRStickY != 0.0f) {
                f32 ratio = (f32)CaaD->unkB8 / (f32)CaaD->unkA4;
                CaaD->unkA4 += dRStickY;
                CaaD->unkA4 = MAX(MIN(CaaD->unkA4, 300), 50);
                CaaD->unkB8 = MIN(CaaD->unkA4 * ratio, 300);
                CaaD->unkA0 = CaaD->unkA4;
                arg0->distance_from_floor = CaaD->unkB8;
                CaaD->unkF1 = 1; // Makes things snappy
            }
            return;
        }
    }
    if ((CaaD->unkB0) && (CaaD->unkF3 != 2)) {
        if (CaaD->unkB0 > 0) {
            CaaD->unkB0 -= 9;
            CaaD->unkB2 += 0x66;
            if (CaaD->unkB0 < 0) {
                CaaD->unkB0 = 0;
            }
        } else {
            CaaD->unkB0 += 9;
            CaaD->unkB2 -= 0x66;
            if (CaaD->unkB0 > 0) {
                CaaD->unkB0 = 0;
            }
        }
    } else if (CaaD->unkF3 == 2) {
        CaaD->unkB0 = 0;
        CaaD->unkF1 = 0;
    }
}

s32 func_global_asm_8062133C(Actor*, Actor*, f32*, f32*, f32*, f32);
void func_global_asm_80622334(Actor*, s16);
u8 func_global_asm_80671E00(f32 arg0, f32 arg1, f32 arg2, f32 arg3, s16 *arg4, s16 *arg5, u8 arg6, u16 arg7);
extern s32 D_global_asm_807FBB68;
extern u32 global_properties_bitfield;

RECOMP_PATCH void func_global_asm_80620628(Actor* arg0, s32 arg1, s16 arg2, u8 arg3) {
    CameraPaad* temp_s1;
    f32 temp_f24;
    s16 spBE;
    s16 temp_s0;
    s16 spBA;
    s16 spB8;
    f32 spB4;
    f32 spB0;
    f32 spAC;
    s16 spAA;
    u8 var_s3;
    u8 spA8;
    u8 var_s4;
    u8 spA6;
    u8 spA5;
    s16 spA2;
    f32 var_f30;
    Actor *temp_v1;

    temp_s1 = arg0->CaaD;
    temp_v1 = temp_s1->unk0;
    spBE = 0;
    spB8 = 0xFFF;
    spBA = 0;
    temp_s1->unkB2 = arg2;
    spAA = temp_s1->unkB2;
    spA5 = 0;
    spA6 = 0;
    spA2 = 0x80;
    var_f30 = 1.0f;
    if (temp_s1->unkAC & 0x80000) return;
    if (temp_s1->unkAC & 0x12000) return;
    if ((temp_s1->unkF3 != 1) && (temp_s1->unkF3 != 5)) return;
    if (D_global_asm_807FBB68 & 2) return;
    if (temp_s1->unkFF >= 4) return;
    if ((temp_v1->unk58 == 5) && (character_change_array[temp_s1->unkFB].unk2C0 != 1)) {
        var_f30 = 0.333f;
    }
    if (!analog_cam_enabled() || !get_analog_allowed()) {
        func_global_asm_8062217C(arg0, 1);
        temp_s1->unkB8 = 10.0f;
        arg0->distance_from_floor = 10.0f;
        temp_s1->unkA0 = 5.0f;
    }
    temp_f24 = var_f30 * 100.0f;
    temp_s1->unkAC |= 0x80000;
    do {
        spA8 = FALSE;
        var_s3 = func_global_asm_8062133C(arg0, temp_s1->unk0, &spB4, &spB0, &spAC, -1.0f) == 1;
        while (
            (temp_s1->unkA0 < temp_f24) &&
            (!var_s3) &&
            (
                var_s4 = func_global_asm_80671E00(spB4, spB0, spAC, 10.0f * var_f30, &spBA, &spB8, 1U, 0xBCU),
                !var_s4
            )
        ) {
            temp_s0 = temp_s1->unkB2;
            temp_s1->unkA0 += 10.0f * var_f30;
            arg0->distance_from_floor += var_f30 * 3.0f;
            var_s3 = func_global_asm_8062133C(arg0, temp_s1->unk0, &spB4, &spB0, &spAC, -1.0f) == 1;
            temp_s1->unkB2 = temp_s0;
        }
        if (var_s4) {
            temp_s1->unkA0 -= (var_f30 * 10.0f);
            arg0->distance_from_floor -= var_f30 * 3.0f;
        }
        if (((var_f30 * 60.0f) < temp_s1->unkA0) && (!var_s3)) {
            spA5 = 1;
        } else if (((var_s4) || (var_s3)) && (spBE < 0x1000)) {
            spA8 = TRUE;
            temp_s1->unkB2 = (((spA6 ? -1 : 1) * spA2) + spAA) & 0xFFF;
            spA6 ^= 1;
            if (!spA6) {
                spA2 += 0x80;
            }
            temp_s1->unkA0 = 5.0f;
            temp_s1->unkB8 = 10.0f;
            arg0->distance_from_floor = 10.0f;
            spBE += 0x80;
        }
    } while (spA8);
    if ((spA5) || ((spBE < 0x1000) && (!var_s3))) {
        temp_s1->unkA4 = temp_s1->unkA0;
        func_global_asm_80622334(arg0, temp_s1->unkA0);
    } else {
        temp_s1->unkFF += arg3;
        spB4 = character_change_array->unk21C;
        spB0 = character_change_array->unk220;
        spAC = character_change_array->unk224;
        if (!analog_cam_enabled() || !get_analog_allowed()) {
            func_global_asm_8062217C(arg0, 1);
        }
        temp_s1->unkA0 = temp_s1->unkA4;
        arg0->distance_from_floor = temp_s1->unkB8;
        temp_s1->unkB2 = spAA;
    }
    temp_s1->unkB0 = 0;
    temp_s1->unkAC |= 0x100000;
    temp_s1->unk38 = spB4;
    temp_s1->unk3C = spB0;
    temp_s1->unk40 = spAC;
    temp_s1->unkEE = temp_s1->unkEF;
    temp_s1->unkAC &= ~0x80000;
    temp_s1->unkC = -32768.0f;
    global_properties_bitfield |= 0x2000;
    arg0->unkB8 = 0.0f;
}

extern Actor *gCurrentActorPointer;
extern s16 D_global_asm_80753B34[];
extern s16 D_global_asm_80753B44[];
extern s16 D_global_asm_807FD584;
void func_global_asm_806DF494(s16 *arg0, s16 arg1, s16 arg2);
// @recomp: Swimming controls
RECOMP_PATCH void func_global_asm_806E6B98(void) {
    s16 phi_v0;
    s8 pad;
    s16 rot;
    s16 phi_a2;
    s8 stick_x, stick_y;
    s32 invX = 0;
    s32 invY = 0;

    phi_v0 = gCurrentActorPointer->y_rotation;
    phi_a2 = 0x10;
    stick_x = D_global_asm_807FD610[cc_player_index].unk2E;
    stick_y = D_global_asm_807FD610[cc_player_index].unk2F;
    recomp_get_swimming_inverted_axes(&invX, &invY);
    if (invX) stick_x = -stick_x;
    if (!invY) stick_y = -stick_y;
    if (D_global_asm_807FD610[cc_player_index].unk30 != 0) {
        if (current_character_index[cc_player_index] == 7) {
            phi_v0 -= (stick_x * 0.8);
        } else {
            phi_v0 -= (stick_x * 0.5);
        }
        gCurrentActorPointer->y_rotation = phi_v0 & 0xFFF;
        if (stick_x >= 0) {
            phi_v0 = 1;
        } else {
            phi_v0 = -1;
        }
        rot = phi_v0;
        rot *= D_global_asm_80753B34[D_global_asm_807FD584];
        rot &= 0xFFF;
    } else {
        rot = 0;
    }
    func_global_asm_806DF494(&gCurrentActorPointer->x_rotation, rot, phi_a2);
    rot = ((D_global_asm_80753B44[D_global_asm_807FD584] * stick_y) / 70);
    rot &= 0xFFF;
    if (current_character_index[cc_player_index] == 7) {
        phi_a2 = (ABS(0.5 * stick_y)) + 16.0;
    }
    func_global_asm_806DF494(&gCurrentActorPointer->z_rotation, rot, phi_a2);
}

extern s16 D_global_asm_80753B44[];
extern s16 D_global_asm_80753B54[];
extern s16 D_global_asm_80753B64[];

// @recomp: Stationary Swimming
RECOMP_PATCH void func_global_asm_806E65BC(void) {
    s32 phi_v0;
    s16 phi_a1;
    s16 phi_t1;
    s16 temp;
    s8 stick_x, stick_y;
    s32 invX = 0;
    s32 invY = 0;

    phi_v0 = gCurrentActorPointer->y_rotation;

    stick_x = D_global_asm_807FD610[cc_player_index].unk2E;
    stick_y = D_global_asm_807FD610[cc_player_index].unk2F;
    recomp_get_swimming_inverted_axes(&invX, &invY);
    if (invX) stick_x = -stick_x;
    if (!invY) stick_y = -stick_y;
    phi_t1 = 0x10;
    if (current_character_index[cc_player_index] == 7) {
        phi_t1 = 0x19;
    }
    if (D_global_asm_807FD610[cc_player_index].unk30) {
        gCurrentActorPointer->y_rotation = ((phi_v0 - (stick_x / 2)) & 0xFFF) & 0xFFF;
        phi_v0 = stick_x >= 0 ? 1 : -1; \
        phi_a1 = ((phi_v0 * D_global_asm_80753B54[D_global_asm_807FD584]) & 0xFFF & 0xFFF & 0xFFF);
    } else {
        phi_a1 = 0;
    }
    func_global_asm_806DF494(&gCurrentActorPointer->x_rotation, phi_a1, 0x10);
    if (stick_y) {
        temp = ((stick_y * D_global_asm_80753B44[D_global_asm_807FD584]) / 70) & 0xFFF & 0xFFF & 0xFFF;
        func_global_asm_806DF494(
            &gCurrentActorPointer->z_rotation, 
            temp,
            ABS(stick_y * 0.125) + phi_t1);
        return;
    }
    func_global_asm_806DF494(&gCurrentActorPointer->z_rotation, D_global_asm_80753B64[D_global_asm_807FD584], 0x10);
}

f32 func_global_asm_806EA2D8(void);

s32 invertAxis(s32 original_value, f32 dGyro, f32 dMouse, u8 invNormal, u8 invGyro, u8 invMouse) {
    if (original_value == 0) {
        if (dGyro != 0.0f) {
            if (invGyro) {
                return -dGyro;
            }
            return dGyro;
        } else if (dMouse != 0.0f) {
            if (invMouse) {
                return -dMouse;
            }
            return dMouse;
        }
    }
    if (invNormal) {
        return -original_value;
    }
    return original_value;
}

void getStickPosition(s32 *x, s32 *y) {
    s32 invX = 0;
    s32 invY = 0;
    s32 invGyroX = 0;
    s32 invGyroY = 0;
    s32 invMouseX = 0;
    s32 invMouseY = 0;
    f32 dGyroX, dGyroY, dMouseX, dMouseY;

    recomp_get_first_person_inverted_axes(&invX, &invY);
    recomp_get_gyro_inverted_axes(&invGyroX, &invGyroY);
    recomp_get_mouse_inverted_axes(&invMouseX, &invMouseY);
    recomp_get_mouse_deltas(&dMouseX, &dMouseY);
    recomp_get_gyro_deltas(&dGyroY, &dGyroX);
    *x = invertAxis(*x, dGyroX, dMouseX, invX, invGyroX, invMouseX);
    *y = invertAxis(*y, dGyroY, dMouseY, !invY, !invGyroY, !invMouseY);
}

// @recomp: First person controls
RECOMP_PATCH void func_global_asm_806EA628(void) {
    PlayerAdditionalActorData *temp_a0;
    s32 pad;
    f32 *temp_v0;
    s16 *temp_v1;
    s32 stick_x, stick_y;

    if (!(extra_player_info_pointer->unk1F0 & 0x8000)) {
        stick_x = D_global_asm_807FD610[cc_player_index].unk2E;
        stick_y = D_global_asm_807FD610[cc_player_index].unk2F;
        getStickPosition(&stick_x, &stick_y);
        temp_a0 = extra_player_info_pointer->unk104->additional_actor_data;
        temp_v1 = &temp_a0->unkB2;
        *temp_v1 -= (stick_x * 0.08 * func_global_asm_806EA2D8() * 4096.0) / 360.0;
        temp_v0 = &temp_a0->unkB8;
        *temp_v0 += stick_y * 0.04 * func_global_asm_806EA2D8();
        *temp_v1 &= 0xFFF;
        if (temp_a0->unkBC + 0x32 < *temp_v0) {
            *temp_v0 = temp_a0->unkBC + 0x32;
        } else if (*temp_v0 < temp_a0->unkBC - 0x50) {
            *temp_v0 = temp_a0->unkBC - 0x50;
        }
        extra_player_info_pointer->unk104->distance_from_floor = *temp_v0;
        gCurrentActorPointer->y_rotation = (temp_a0->unkB2 + 0x800) & 0xFFF;
    }
}

void func_global_asm_806A2A10(s32, s32, s32);
f32 func_global_asm_80612790(s16 arg0);
f32 func_global_asm_80612794(s16 arg0);
void func_global_asm_8061C2F0(Actor *camera, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, u8 arg7);
u8 func_global_asm_8061CB50(void);
void addActorToTextOverlayRenderArray(void *arg0, void *arg1, u8 arg2);
Gfx *func_global_asm_806FF01C(Gfx *dl, Actor *arg1);
Gfx *func_global_asm_806FEF7C(Gfx *dl, Actor *arg1);
void func_global_asm_8066E6C8(Actor *arg0, s32 arg1, u8 arg2);
Gfx *func_global_asm_8068DAF4(Gfx *dl, u8 *arg1);
s32 deleteActor(Actor*);
void func_global_asm_8063DA40(s16 arg0, s16 arg1);
s32 func_global_asm_8067ACDC(Actor *arg0, u16 arg1, s32 (*arg2)(Actor *));
void func_global_asm_806907F0(f32 x, f32 y, f32 z);
f32 func_global_asm_80611BB4(f32 arg0, f32 arg1);
void func_global_asm_806A2B08(Actor *arg0);
f32 func_global_asm_80612D10(f32 arg0);
f32 func_global_asm_80612D1C(f32 arg0);
void func_global_asm_8069084C(s16 arg0, s16 arg1, f32 arg2, s16 arg3, f32 arg4, f32 arg5, Actor *arg6);
void playSoundAtActorPosition(Actor *arg0, s16 arg1, u8 arg2, s16 arg3, u8 arg4);
void func_global_asm_80679200(Actor *arg0, Actor *arg1, s32 arg2, u8 arg3, s32 arg4, void *arg5);
void func_global_asm_8067A70C(Actor *arg0, Actor *arg1, f32 arg2, f32 arg3, f32 arg4, u8 arg5, u8 arg6);
void func_global_asm_806CFF9C(Actor *arg0);
void renderActor(Actor *arg0, u8 arg1);
extern GlobalASMStruct35 D_global_asm_807FBB70;
extern f32 D_global_asm_8074E7F4;
extern f32 D_global_asm_8074E7F8;
extern f32 D_global_asm_8074E7FC;
extern f32 D_global_asm_8074E800;
extern f32 D_global_asm_8074E804;
extern f32 D_global_asm_8074E808;
extern u8 D_global_asm_8074E810;
extern Actor *gPlayerPointer;
extern Actor *gLastSpawnedActor;

typedef struct CannonGameAAD {
    u8 unk0;
    u8 unk1;
    u8 unk2;
    u8 unk3;
    Actor *unk4;
    s16 unk8;
} CannonGameAAD;

RECOMP_PATCH void func_global_asm_806822FC(void) {
    CannonGameAAD *temp_s1;
    Actor* sp58;
    f32 sp54;
    s32 i;
    f32 temp_f18;
    u8 sp4B;
    u8 sp4A;
    s32 pad[3];
    s32 stick_x, stick_y;

    temp_s1 = gCurrentActorPointer->AAD_as_array[0];
    sp4B = 0;
    sp4A = 0;
    if (!(gCurrentActorPointer->object_properties_bitfield & 0x10)) {
        gCurrentActorPointer->unk132 = 1;
        temp_s1->unk2 = 0U;
        temp_s1->unk8 = 0;
        temp_s1->unk1 = 0U;
        temp_s1->unk3 = 0U;
    }
    if ((gCurrentActorPointer->control_state == 2) || (gCurrentActorPointer->control_state == 3)) {
        stick_x = D_global_asm_807FD610[0].unk2E;
        stick_y = D_global_asm_807FD610[0].unk2F;
        getStickPosition(&stick_x, &stick_y);
        sp58 = temp_s1->unk4->PaaD->unk104;
        temp_s1->unk8 -= ((stick_x * 0.08 * D_global_asm_8074E7F8 * 4096.0) / 360.0);
        temp_s1->unk8 &= 0xFFF;
        sp54 = func_global_asm_80612794(temp_s1->unk8) * D_global_asm_8074E7F4;
        temp_f18 = func_global_asm_80612790(temp_s1->unk8) * D_global_asm_8074E7F4;
        D_global_asm_8074E804 -= (stick_y * 0.04 * D_global_asm_8074E800);
        if (D_global_asm_8074E804 > 50.0) {
            D_global_asm_8074E804 = 50.0f;
        }
        if (D_global_asm_8074E804 < -50.0) {
            D_global_asm_8074E804 = -50.0f;
        }
        func_global_asm_8061C2F0(sp58,
            gCurrentActorPointer->x_position,
            gCurrentActorPointer->y_position + D_global_asm_8074E7FC,
            gCurrentActorPointer->z_position,
            gCurrentActorPointer->x_position + sp54,
            gCurrentActorPointer->y_position + D_global_asm_8074E7FC + D_global_asm_8074E804,
            gCurrentActorPointer->z_position + temp_f18,
            0U
        );
        if (func_global_asm_8061CB50() == 0) {
            addActorToTextOverlayRenderArray(func_global_asm_806FF01C, gPlayerPointer, 3U);
            addActorToTextOverlayRenderArray(func_global_asm_806FEF7C, gPlayerPointer, 3U);
        }
        func_global_asm_8066E6C8(gCurrentActorPointer, 0, 0U);
    } else {
        func_global_asm_8066E6C8(gCurrentActorPointer, 0, 1U);
    }
    if (temp_s1->unk2) {
        addActorToTextOverlayRenderArray(func_global_asm_8068DAF4, temp_s1, 3U);
    }
    if ((gCurrentActorPointer->unk11C) && (temp_s1->unk2) && (gCurrentActorPointer->unk11C->control_state == 5) && (temp_s1->unk1 == 0)) {
        temp_s1->unk1 = 0x1EU;
    }
    if (temp_s1->unk1) {
        temp_s1->unk1--;
        if (temp_s1->unk1 == 0) {
            sp4B = 1;
        }
    }
    for (i = 0; i < D_global_asm_807FBB70.unk254; i++) {
        if (D_global_asm_807FBB70.unk258[i] == 1) {
            temp_s1->unk3 = 1U;
            sp4A = temp_s1->unk3;
        }
    }
    if ((sp4B) || (sp4A)) {
        temp_s1->unk2 = 0U;
        if ((gCurrentActorPointer->control_state == 2) || (gCurrentActorPointer->control_state == 3)) {
            gCurrentActorPointer->control_state = 3;
        } else {
            gCurrentActorPointer->control_state = 0;
            gCurrentActorPointer->noclip_byte = 2;
        }
        if (gCurrentActorPointer->unk11C) {
            deleteActor(gCurrentActorPointer->unk11C);
            gCurrentActorPointer->unk11C = NULL;
        }
        if (sp4B) {
            func_global_asm_8063DA40(0x30, 0);
            func_global_asm_8063DA40(0x2F, 0);
            func_global_asm_8063DA40(0x31, 0);
        }
    }
    switch (gCurrentActorPointer->control_state) {
        case 0:
            gCurrentActorPointer->unk132 = 1;
            if ((D_global_asm_807FBB70.unk200 == 5) && (temp_s1->unk3 == 0)) {
                gCurrentActorPointer->control_state = 1;
                gCurrentActorPointer->unk132 = 2;
            }
            break;
        case 1:
            if (D_global_asm_807FBB70.unk200 == 4) {
                gCurrentActorPointer->control_state = 2;
                temp_s1->unk0 = 6U;
                temp_s1->unk4 = (Actor* ) D_global_asm_807FBB70.unk1FC;
                if (temp_s1->unk2 == 0) {
                    func_global_asm_8063DA40(0x30, 0xA);
                    temp_s1->unk2 = 1U;
                    func_global_asm_806A2A10(0xC8, 0x20, D_global_asm_8074E810);
                    func_global_asm_806A2B08(gLastSpawnedActor);
                    gCurrentActorPointer->object_properties_bitfield |= 0x200000;
                }
                gCurrentActorPointer->object_properties_bitfield &= ~4;
                gCurrentActorPointer->noclip_byte = 1;
            }
            break;
        case 2:
            if ((D_global_asm_807FD610[0].unk2C & 0x2000) && (!func_global_asm_8067ACDC(gCurrentActorPointer, 4U, NULL)) && (temp_s1->unk0)) {
                f32 temp_f0_3;
                f32 sp34;
                
                func_global_asm_806907F0(gCurrentActorPointer->position.f[0], gCurrentActorPointer->position.f[1] + D_global_asm_8074E7FC, gCurrentActorPointer->position.f[2]);
                temp_f0_3 = func_global_asm_80611BB4(
                    character_change_array->look_at_at_y - character_change_array->look_at_eye[1],
                    _sqrtf(
                        SQ(character_change_array->look_at_at_x - character_change_array->look_at_eye[0]) +
                        SQ(character_change_array->look_at_at_z - character_change_array->look_at_eye[2]) 
                    )
                );
                sp34 = func_global_asm_80612D1C(temp_f0_3) * D_global_asm_8074E808;
                func_global_asm_8069084C(0x4A, 0, 0.6f, temp_s1->unk8, func_global_asm_80612D10(temp_f0_3) * D_global_asm_8074E808, sp34, gCurrentActorPointer);
                playSoundAtActorPosition(gCurrentActorPointer, 0x17, 0xFFU, 0x7F, 0x19U);
                temp_s1->unk0--;
                if (temp_s1->unk0 == 0) {
                    gCurrentActorPointer->control_state = 3;
                }
            }
            break;
        case 3:
            if (!func_global_asm_8067ACDC(gCurrentActorPointer, 4U, NULL)) {
                func_global_asm_80679200(temp_s1->unk4, gCurrentActorPointer, 0x40, 1U, 0, NULL);
                func_global_asm_8067A70C(gCurrentActorPointer, temp_s1->unk4, gCurrentActorPointer->position.f[0], gCurrentActorPointer->position.f[1], gCurrentActorPointer->position.f[2] - 42.0f, 2U, 1U);
                temp_s1->unk4->object_properties_bitfield &= ~8;
                func_global_asm_806CFF9C(temp_s1->unk4);
                gCurrentActorPointer->object_properties_bitfield |= 4;
                gCurrentActorPointer->control_state = 0;
                gCurrentActorPointer->noclip_byte = 2;
            }
            break;
    }
    renderActor(gCurrentActorPointer, 0U);
}


Gfx *func_menu_8002F980(Gfx *, MenuAdditionalActorData *, u8 **, s8, s32 *, s32, f32 *, f32, s32);
s32 func_menu_800317E8(MenuAdditionalActorData *arg0, f32 arg1, f32 arg2, f32 *arg3, f32 *arg4, s32 arg5, s8 arg6, f32 arg7);
Gfx* printStyledText(Gfx* dl, s16 style, s16 x, s16 y, u8* string, u32 extraBitfield);
void func_menu_8002FD38(MenuAdditionalActorData*, s32, s32);
void func_menu_8002FC1C(Actor *, MenuAdditionalActorData *, s32);
void func_menu_8002FE08(MenuAdditionalActorData*, s32);
void func_global_asm_8060DEA8(void);
extern s8 D_global_asm_8074583C;
extern s8 D_global_asm_80745840;
extern s8 D_global_asm_80745844;
extern s16 D_menu_80033674;
extern u32 global_properties_bitfield;
extern u8 **label_string_pointer_array;

// @recomp: Sound DL
RECOMP_PATCH Gfx *func_menu_8002D520(Actor *arg0, Gfx *dl) {
    MenuAdditionalActorData *MaaD = arg0->MaaD;
    s32 sp60;
    s8 pad[4];
    f32 sp58;
    f32 sp54;
    f32 sp50;
    s16 pad4;
    s16 temp_f10;
    s8 pad3[2];
    s8 var_v0;
    char sp40[4];

    global_properties_bitfield &= ~0x10;
    func_menu_800317E8(MaaD, 160.0f, 20.0f, &sp58, &sp54, 2, 0, 1.5f);
    dl = printStyledText(dl, 1, 0x280, sp54 * 4.0f, label_string_pointer_array[32], 0x81U);
    dl = func_menu_8002F980(dl, MaaD, &label_string_pointer_array[33], 4, &sp60, 1, &sp50, 80.0f, -1);
    D_menu_80033674 = 0x3E8;
    func_menu_800317E8(MaaD, 160.0f, 150.0f, &sp58, &sp54, 2, 0, 1.5f);
    temp_f10 = sp54 * 4.0f;
    sp58 = (sp58 - sp50) * 4.0f;
    switch (sp60) {
        case 0:
            dl = printStyledText(dl, 1, sp58, temp_f10, label_string_pointer_array[D_global_asm_80745844 + 37], 0x81U);
            break;
        // case 1:
        // case 2:
        //     dl = printStyledText(dl, 1, sp58, temp_f10, label_string_pointer_array[40], 0x81U);
        //     var_v0 = sp60 == 1 ? D_global_asm_8074583C : D_global_asm_80745840;
        //     _sprintf(sp40, "%d", (var_v0 + 1) >> 1);
        //     dl = printStyledText(dl, 1, sp58, temp_f10 + 0x78, &sp40, 0x81U);
        //     break;
        // case 3:
        //     D_menu_80033674 = 0;
        //     break;
    }
    return dl;
}

extern s16 D_menu_80033670;
extern s8 D_global_asm_8074583C;
extern s8 D_global_asm_80745840;
extern s8 D_global_asm_80745844;

void func_global_asm_8060A398(s32);
void func_menu_80027E10(void);
void func_menu_8002907C(void);
void func_global_asm_8060C648(s32 arg0, u8 arg1, u8 arg2, u8 fileIndex, u32 arg4);

// @recomp: Sound screen code
RECOMP_PATCH void func_menu_8002CFA4(Actor *arg0, s32 arg1) {
    MenuAdditionalActorData *MaaD = arg0->MaaD;
    s8 sp23 = FALSE;

    func_menu_8002907C();
    if (MaaD->unk0 == 0.0f) {
        if (MaaD->unk4 == 0.0f) {
            switch (MaaD->unk17) {
                case 0:
                    if (!(D_menu_80033670 & 0x10) && (arg1 & 0x10)) {
                        D_global_asm_80745844++;
                        if (D_global_asm_80745844 > 2) {
                            D_global_asm_80745844 = 0;
                        }
                        sp23 = TRUE;
                    } else if (!(D_menu_80033670 & 0x20) && (arg1 & 0x20)) {
                        D_global_asm_80745844--;
                        if (D_global_asm_80745844 < 0) {
                            D_global_asm_80745844 = 2;
                        }
                        sp23 = TRUE;
                    }
                    if (sp23) {
                        func_menu_80027E10();
                    }
                    break;
                // case 1:
                //     if (arg1 & 0x10) {
                //         D_global_asm_8074583C++;
                //         playSound(0x2A0, 0x7FFF, 63.0f, 1.0f, 0, 0);
                //         if (D_global_asm_8074583C > 0x28) {
                //             D_global_asm_8074583C = 0x28;
                //         }
                //     } else if (arg1 & 0x20) {
                //         D_global_asm_8074583C--;
                //         playSound(0x2A0, 0x7FFF, 63.0f, 1.0f, 0, 0);
                //         if (D_global_asm_8074583C < 0) {
                //             D_global_asm_8074583C = 0;
                //         }
                //     }
                //     func_global_asm_80737B58(0, ((D_global_asm_8074583C * 0x61A8) / 40));
                //     func_global_asm_80737B58(1, ((D_global_asm_8074583C * 0x61A8) / 40));
                //     func_global_asm_80737B58(2, ((D_global_asm_8074583C * 0x61A8) / 40));
                //     func_global_asm_80737B58(3, ((D_global_asm_8074583C * 0x61A8) / 40));
                //     break;
                // case 2:
                //     if (arg1 & 0x10) {
                //         D_global_asm_80745840++;
                //         if (D_global_asm_80745840 >= 0x29) {
                //             D_global_asm_80745840 = 0x28;
                //         }
                //     } else if (arg1 & 0x20) {
                //         D_global_asm_80745840--;
                //         if (D_global_asm_80745840 < 0) {
                //             D_global_asm_80745840 = 0;
                //         }
                //     }
                //     func_global_asm_8060A398(0);
                //     func_global_asm_8060A398(2);
                //     break;
                // case 3:
                //     if (arg1 & 0x100) {
                //         D_global_asm_80745844 = 0;
                //         D_global_asm_8074583C = 0x28;
                //         D_global_asm_80745840 = 0x28;
                //         func_global_asm_80737B58(0, ((D_global_asm_8074583C * 0x61A8) / 40));
                //         func_global_asm_80737B58(1, ((D_global_asm_8074583C * 0x61A8) / 40));
                //         func_global_asm_80737B58(2, ((D_global_asm_8074583C * 0x61A8) / 40));
                //         func_global_asm_80737B58(3, ((D_global_asm_8074583C * 0x61A8) / 40));
                //         func_global_asm_8060A398(0);
                //         func_global_asm_8060A398(2);
                //         sp23 = 1;
                //         func_menu_80027E10();
                //     }
                //     break;
            }
            if (arg1 & 2) {
                if (func_global_asm_8060C6B8(0x1E, 0, 0, 0) != D_global_asm_80745844) {
                    func_global_asm_8060C648(0x1E, 0, 0, 0, D_global_asm_80745844);
                    func_global_asm_8060DEA8();
                }
                playSound(0x2C9, 0x7FFF, 63.0f, 1.0f, 0, 0);
                MaaD->unk16 = 0;
                MaaD->unk13 = 1;
            } else {
                func_menu_8002FD38(MaaD, 1, arg1);  // @recomp: Change to 1 option
            }
        }
        func_menu_8002FE08(MaaD, 1);  // @recomp: Change to 1 option
    }
    func_menu_8002FC1C(arg0, MaaD, 1);
    if (sp23) {
        playSound(0x74, 0x7FFF, 63.0f, 1.0f, 0, 0);
    }
}

extern s16 D_menu_80033674;
void func_global_asm_8060C8AC(u8 arg0);

// @recomp: Options DL
RECOMP_PATCH Gfx *func_menu_8002DBDC(Actor *arg0, Gfx *dl) {
    MenuAdditionalActorData *aaD;
    u32 sp68;
    f32 x;
    f32 sp60;
    f32 sp5C;
    s32 pad6;
    s16 y;
    s32 pad;
    s32 pad2;
    s32 pad3;
    s32 pad4;
    s32 pad5;
    s32 pad7;

    aaD = arg0->additional_actor_data;
    global_properties_bitfield &= ~0x10;
    func_menu_800317E8(aaD, 160.0f, 20.0f, &x, &sp60, 1, 0, 1.5f);
    dl = printStyledText(dl, 1, 640, sp60 * 4.0f, label_string_pointer_array[41], 0x81);
    dl = func_menu_8002F980(dl, aaD, &label_string_pointer_array[44], 1, &sp68, 1, &sp5C, 80.0f, -1);
    D_menu_80033674 = 1000;
    func_menu_800317E8(aaD, 160.0f, 150.0f, &x, &sp60, 2, 0, 1.5f);
    y = sp60 * 4.0f;
    x = (x - sp5C) * 4.0f;
    switch (sp68) {
        // case 0:
        //     dl = printStyledText(dl, 1, x, y + 64, label_string_pointer_array[47 + widescreen_enabled], 0x81);
        //     break;
        // case 1:
        //     dl = printStyledText(dl, 1, x, y + 64, label_string_pointer_array[49 + story_skip], 0x81);
        //     break;
        case 0:  // @recomp: Isolate to just resetting highscores
            D_menu_80033674 = 0;
            break;
        // case 3:
        //     dl = printStyledText(dl, 1, x, y + 64, label_string_pointer_array[52 + D_global_asm_80744530], 0x81);
        //     break;
        // case 4:
        //     dl = printStyledText(dl, 1, x, y + 64, label_string_pointer_array[51], 0x81);
        //     break;
    }
    return dl;
}

// @recomp: Options Code
extern u8 D_global_asm_80744530;
extern s16 D_global_asm_80744544;
extern u8 D_global_asm_807550C8;

RECOMP_PATCH void func_menu_8002D8AC(Actor *arg0, s32 arg1) {
    MenuAdditionalActorData *aaD;
    s8 optionChanged;
    s32 i;

    aaD = arg0->MaaD;
    optionChanged = FALSE;
    if (aaD->unk0 == 0.0f) {
        if (aaD->unk4 == 0.0f) {
            switch (aaD->unk17) {
                // case 0:
                //     if ((arg1 & 0x30) && !(D_menu_80033670 & 0x30)) {
                //         widescreen_enabled = 1 - widescreen_enabled;
                //         optionChanged = TRUE;
                //     }
                //     break;
                // case 1:
                //     if ((arg1 & 0x30) && !(D_menu_80033670 & 0x30)) {
                //         story_skip = 1 - story_skip;
                //         optionChanged = TRUE;
                //     }
                //     break;
                case 0:
                    if (arg1 & 0x100) {
                        func_global_asm_8060C8AC(0xFF);
                        playSound(0x23C, 0x7FFF, 63.0f, 1.0f, 0, 0);
                    }
                    break;
                // case 3:
                //     if ((arg1 & 0x30) && !(D_menu_80033670 & 0x30)) {
                //         D_global_asm_80744530 = 1 - D_global_asm_80744530;
                //         optionChanged = TRUE;
                //     }
                //     break;
                // case 4:
                //     if ((arg1 & 0x10) && !(D_menu_80033670 & 0x10)) {
                //         D_global_asm_807550C8++;
                //         optionChanged = TRUE;
                //     }
                //     if ((arg1 & 0x20) && !(D_menu_80033670 & 0x20)) {
                //         D_global_asm_807550C8--;
                //         optionChanged = TRUE;
                //     }
                //     D_global_asm_807550C8 &= 3;
                //     if (optionChanged) {
                //         for (i = 0; i < 101; i++) {
                //             func_global_asm_8061134C((label_string_pointer_array[i]));
                //         }
                //         func_global_asm_8061134C(label_string_pointer_array);
                //         func_menu_800324CC();
                //     }
                //     break;
            }
            if (arg1 & 2) {
                func_global_asm_8060C648(0x1F, 0, 0, 0, D_global_asm_807550C8);
                func_global_asm_8060C648(0x20, 0, 0, 0, D_global_asm_80744530);
                func_global_asm_8060DEA8();
                playSound(0x2C9, 0x7FFF, 63.0f, 1.0f, 0, 0);
                aaD->unk16 = 0;
                aaD->unk13 = 1;
            } else {
                func_menu_8002FD38(aaD, 1, arg1);
            }
        }
        func_menu_8002FE08(aaD, 1);
    }
    func_menu_8002FC1C(arg0, aaD, 1);
    // if (newly_pressed_input_copy & (U_CBUTTONS | D_CBUTTONS)) {
    //     D_global_asm_80744544 ^= 0x100;
    // }
    if (optionChanged) {
        playSound(0x74, 0x7FFF, 63.0f, 1.0f, 0, 0);
    }
}

u8 temporary_flag_block[0xE];
u8 *getFlagBlockAddress(u8 flagType);
void func_global_asm_807311C4(s16 index);
RECOMP_PATCH u8 isFlagSet(s16 flagIndex, u8 flagType) { // TODO: Can we use the FlagTypes enum? Needs to be a u8 to match
    u8 *flagBlock;
    s16 flagByte;
    
    if ((flagIndex == 0x1C) && (flagType == FLAG_TYPE_GLOBAL)) {
        // Multiplayer enabled global flag
        if (!recomp_get_mp_enabled()) {
            return FALSE;
        }
    }
    recomp_on_flag_check(&flagIndex, &flagType);
    if (flagIndex == -1) {
        return 0;
    }
    // @recomp (Android Touch Kong Swap, android_hooks.c): only the chosen Kong counts as
    // freed for the moment it takes Tag Anywhere to swap to them. Never while a map is loading
    // or a cutscene runs: the map would be built as if the other Kongs were still caged (they
    // would stand at their rescue spots for the rest of the visit).
    if ((dk64_android_tag_target >= 0) && (flagType == FLAG_TYPE_PERMANENT) &&
        (loading_zone_transition_speed <= 0.0f) && (is_cutscene_active == 0)) {
        static const s16 kong_freed_flags[5] = { 0x181, 0x006, 0x046, 0x042, 0x075 };
        s32 k;
        for (k = 0; k < 5; k++) {
            if ((flagIndex == kong_freed_flags[k]) && (k != dk64_android_tag_target)) {
                return FALSE;
            }
        }
    }
    switch (flagType) {
        case FLAG_TYPE_PERMANENT:
        case FLAG_TYPE_GLOBAL:
            flagBlock = getFlagBlockAddress(flagType);
            break;
        case FLAG_TYPE_TEMPORARY:
            flagBlock = temporary_flag_block;
    }
    flagByte = flagIndex >> 3;
    return flagBlock[flagByte] >> (s16)(flagIndex - flagByte * 8) & 1;
}

RECOMP_PATCH void setFlag(s16 flagIndex, u8 newValue, u8 flagType) {
    u8 *flagBlock;
    s16 flagByte;
    s32 sp2C; // This is load bearing, cannot remove

    recomp_on_flag_change(&flagIndex, &newValue, &flagType);
    if (flagIndex != -1) {
        switch (flagType) {
            case FLAG_TYPE_PERMANENT:
            case FLAG_TYPE_GLOBAL:
                flagBlock = getFlagBlockAddress(flagType);
                break;
            case FLAG_TYPE_TEMPORARY:
                flagBlock = temporary_flag_block;
                break;
        }
        flagByte = flagIndex >> 3;
        if (newValue) {
            flagBlock[flagByte] |= 1 << (s16)(flagIndex - (flagByte * 8));
        } else {
            flagBlock[flagByte] &= ~(1 << (s16)(flagIndex - (flagByte * 8)));
        }
        if (newValue && (flagType == FLAG_TYPE_PERMANENT)) {
            func_global_asm_807311C4(flagIndex);
        }
    }
}

RECOMP_PATCH void func_global_asm_80731030(void) { // clearTemporaryFlags()
    s32 flagIndex;
    
    for (flagIndex = 0; flagIndex != 0xE; flagIndex++) {
        temporary_flag_block[flagIndex] = 0;
    }
}