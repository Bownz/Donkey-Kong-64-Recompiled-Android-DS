#ifndef __INPUT_H__
#define __INPUT_H__

#include "patch_helpers.h"

typedef enum {
    RECOMP_CAMERA_NORMAL,
    RECOMP_CAMERA_DUALANALOG,
} RecompCameraMode;

typedef enum {
    RECOMP_AIMING_OVERRIDE_OFF = 0,
    RECOMP_AIMING_OVERRIDE_DISABLE_LEFT_STICK = 1,
    RECOMP_AIMING_OVERRIDE_FORCE_RIGHT_STICK = 2
} RecompAimingOverideMode;

extern RecompCameraMode recomp_camera_mode;
extern RecompAimingOverideMode recomp_aiming_override_mode;

DECLARE_FUNC(void, recomp_get_gyro_deltas, float* x, float* y);
DECLARE_FUNC(void, recomp_get_mouse_deltas, float* x, float* y);
DECLARE_FUNC(void, recomp_get_inverted_axes, s32* x, s32* y);
DECLARE_FUNC(u32, recomp_get_analog_cam_sensitivity);
DECLARE_FUNC(void, recomp_get_analog_inverted_axes, s32* x, s32* y);
DECLARE_FUNC(void, recomp_get_swimming_inverted_axes, s32* x, s32* y);
DECLARE_FUNC(void, recomp_get_first_person_inverted_axes, s32* x, s32* y);
DECLARE_FUNC(void, recomp_get_gyro_inverted_axes, s32* x, s32* y);
DECLARE_FUNC(void, recomp_get_mouse_inverted_axes, s32* x, s32* y);
DECLARE_FUNC(void, recomp_get_right_analog_inputs, float* x, float* y);
DECLARE_FUNC(void, recomp_set_right_analog_suppressed, s32 suppressed);
DECLARE_FUNC(void, recomp_report_input_handler, u32 index);
DECLARE_FUNC(void, recomp_report_frame);
DECLARE_FUNC(s32, recomp_get_tag_target);

// patches/button_icons.c: an in-game button icon drawn for the player's controller.
typedef struct {
    s32 sprite_id;
    s32 width;
    s32 height;
    s32 codec;
    s32 tile_x;
    s32 tile_y;
    s32 tiles_x;
    s32 tiles_y;
} ButtonIconRequest;
DECLARE_FUNC(s32, recomp_fill_button_icon, ButtonIconRequest *request, u8 *out);
// Android: the touch-drag camera turn since the last call (see recomp_api.cpp). Returns 1 if any.
DECLARE_FUNC(s32, recomp_touch_camera_get, f32 *x, f32 *y);

#endif