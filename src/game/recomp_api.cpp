#include <cmath>

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "librecomp/addresses.hpp"
#include "donk_config.h"
#include "recompinput/recompinput.h"
#include "recompui/recompui.h"
#include "recompui/renderer.h"
#include "donk_sound.h"
#include "donk_draw.h"
#include "librecomp/helpers.hpp"
#include "../patches/input.h"
#include "../patches/graphics.h"
#include "../patches/sound.h"
#include "../patches/options.h"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/config.hpp"
#include "../lib/N64ModernRuntime/thirdparty/xxHash/xxh3.h"

extern "C" void recomp_update_inputs(uint8_t* rdram, recomp_context* ctx) {
    recompinput::poll_inputs();
}

extern "C" void recomp_puts(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) cur_str = _arg<0, PTR(char)>(rdram, ctx);
    u32 length = _arg<1, u32>(rdram, ctx);

    for (u32 i = 0; i < length; i++) {
        fputc(MEM_B(i, (gpr)cur_str), stdout);
    }
}

extern "C" void recomp_exit(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::quit();
}

extern "C" void recomp_error(uint8_t* rdram, recomp_context* ctx) {
    std::string str{};
    PTR(u8) str_ptr = _arg<0, PTR(u8)>(rdram, ctx);

    for (size_t i = 0; MEM_B(str_ptr, i) != '\x00'; i++) {
        str += (char)MEM_B(str_ptr, i);
    }

    recompui::message_box(str.c_str());
    assert(false);
    ultramodern::error_handling::quick_exit(__FILE__, __LINE__, __FUNCTION__);
}

#if defined(__ANDROID__)
// android/jni/game_state.cpp: what the Android bottom screen and FPS counter read.
extern "C" void dk64_android_report_input_handler(uint32_t index);
extern "C" void dk64_android_report_frame();
extern "C" int32_t dk64_android_tag_target();
#endif

// patches/android_hooks.c: the player's current input-handler set (see there).
extern "C" void recomp_report_input_handler(uint8_t* rdram, recomp_context* ctx) {
#if defined(__ANDROID__)
    dk64_android_report_input_handler(_arg<0, uint32_t>(rdram, ctx));
#else
    (void)rdram;
    (void)ctx;
#endif
}

// patches/android_hooks.c: once per game frame.
extern "C" void recomp_report_frame(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
#if defined(__ANDROID__)
    dk64_android_report_frame();
#endif
}

// patches/button_icons.c: draws an in-game button icon (an N64 button's sprite) as the
// physical button it's bound to on the player's controller, in the sprite's own size and
// texture format. Returns 0 to keep the game's own icon (no controller, or not bound to one).
#if defined(__ANDROID__)
#include "recompinput/profiles.h"
#include "util/file.h"
#include <algorithm>
#include <cstdlib>
#include <android/log.h>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <vector>

#include <atomic>
static std::atomic<bool> button_icons_enabled{ true };
extern "C" void dk64_android_set_button_icons(int enabled) {
    button_icons_enabled = enabled != 0;
}

namespace {
    constexpr int icon_size = 64;   // assets/button_icons/*.rgba: 64x64 RGBA (android/tools/gen_button_icons.py)

    const std::vector<uint8_t> *load_button_icon(const std::string &name) {
        static std::mutex mutex;
        static std::map<std::string, std::vector<uint8_t>> cache;
        std::lock_guard lock{ mutex };
        auto found = cache.find(name);
        if (found != cache.end()) {
            return found->second.empty() ? nullptr : &found->second;
        }
        std::vector<uint8_t> pixels;
        std::ifstream in{ recompui::file::get_asset_path(("button_icons/" + name + ".rgba").c_str()), std::ios::binary };
        if (in) {
            pixels.resize(icon_size * icon_size * 4);
            in.read(reinterpret_cast<char *>(pixels.data()), pixels.size());
            if (!in) {
                pixels.clear();
            }
        }
        auto &stored = cache[name] = std::move(pixels);
        return stored.empty() ? nullptr : &stored;
    }

    bool game_input_for_sprite(int32_t sprite, recompinput::GameInput &out) {
        using GI = recompinput::GameInput;
        switch (sprite) {
            case 0x6E: out = GI::A; return true;
            case 0x6F: out = GI::B; return true;
            case 0x70: out = GI::Z; return true;
            case 0x71: out = GI::C_DOWN; return true;
            case 0x72: out = GI::C_UP; return true;
            case 0x73: out = GI::C_LEFT; return true;
            case 0x87: out = GI::R; return true;
            case 0x88: out = GI::L; return true;
            case 0x91: out = GI::START; return true;
            default: return false;
        }
    }

    std::string icon_name_for(const recompinput::InputField &field, recompinput::FaceButtonStyle style) {
        using recompinput::FaceButtonStyle;
        const bool nin = style == FaceButtonStyle::Nintendo;
        const bool ps = style == FaceButtonStyle::PlayStation;
        if (field.input_type == recompinput::InputType::ControllerDigital) {
            switch (static_cast<SDL_GameControllerButton>(field.input_id)) {
                case SDL_CONTROLLER_BUTTON_A: return ps ? "ps_cross" : nin ? "nin_a" : "xbox_a";
                case SDL_CONTROLLER_BUTTON_B: return ps ? "ps_circle" : nin ? "nin_b" : "xbox_b";
                case SDL_CONTROLLER_BUTTON_X: return ps ? "ps_square" : nin ? "nin_x" : "xbox_x";
                case SDL_CONTROLLER_BUTTON_Y: return ps ? "ps_triangle" : nin ? "nin_y" : "xbox_y";
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return ps ? "l1" : nin ? "l" : "lb";
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return ps ? "r1" : nin ? "r" : "rb";
                case SDL_CONTROLLER_BUTTON_START: return nin ? "plus" : "start";
                case SDL_CONTROLLER_BUTTON_BACK: return nin ? "minus" : "back";
                case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "ls_click";
                case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "rs_click";
                case SDL_CONTROLLER_BUTTON_DPAD_UP: return "dpad_up";
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "dpad_down";
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "dpad_left";
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "dpad_right";
                default: return "";
            }
        }
        if (field.input_type == recompinput::InputType::ControllerAnalog) {
            const bool positive = field.input_id > 0;
            switch (static_cast<SDL_GameControllerAxis>(std::abs(field.input_id) - 1)) {
                case SDL_CONTROLLER_AXIS_LEFTX: return positive ? "ls_right" : "ls_left";
                case SDL_CONTROLLER_AXIS_LEFTY: return positive ? "ls_down" : "ls_up";
                case SDL_CONTROLLER_AXIS_RIGHTX: return positive ? "rs_right" : "rs_left";
                case SDL_CONTROLLER_AXIS_RIGHTY: return positive ? "rs_down" : "rs_up";
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return ps ? "l2" : nin ? "zl" : "lt";
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return ps ? "r2" : nin ? "zr" : "rt";
                default: return "";
            }
        }
        return "";
    }

    // The icon's pixel at (x, y) of a w x h area, the icon (square, centred) inside it;
    // a box filter over the source pixels it covers.
    void sample_icon(const std::vector<uint8_t> &icon, int x, int y, int w, int h, uint8_t rgba[4]) {
        // 70% of the original icon's size (ours have no empty border of their own).
        const int side = std::max(1, static_cast<int>(std::min(w, h) * 0.7f + 0.5f));
        const int ox = (w - side) / 2, oy = (h - side) / 2;
        rgba[0] = rgba[1] = rgba[2] = rgba[3] = 0;
        if (x < ox || y < oy || x >= ox + side || y >= oy + side) {
            return;
        }
        const float scale = static_cast<float>(icon_size) / side;
        const int sx0 = static_cast<int>((x - ox) * scale), sx1 = std::max(sx0 + 1, static_cast<int>((x - ox + 1) * scale));
        const int sy0 = static_cast<int>((y - oy) * scale), sy1 = std::max(sy0 + 1, static_cast<int>((y - oy + 1) * scale));
        uint32_t sum[4] = {}, alpha_sum = 0, count = 0;
        for (int sy = sy0; sy < sy1 && sy < icon_size; sy++) {
            for (int sx = sx0; sx < sx1 && sx < icon_size; sx++) {
                const uint8_t *p = &icon[(sy * icon_size + sx) * 4];
                // Colours weighted by alpha, so transparent edges don't darken.
                sum[0] += p[0] * p[3];
                sum[1] += p[1] * p[3];
                sum[2] += p[2] * p[3];
                alpha_sum += p[3];
                count++;
            }
        }
        if (count == 0 || alpha_sum == 0) {
            return;
        }
        rgba[0] = static_cast<uint8_t>(sum[0] / alpha_sum);
        rgba[1] = static_cast<uint8_t>(sum[1] / alpha_sum);
        rgba[2] = static_cast<uint8_t>(sum[2] / alpha_sum);
        rgba[3] = static_cast<uint8_t>(alpha_sum / count);
    }
}
#endif

extern "C" void recomp_fill_button_icon(uint8_t* rdram, recomp_context* ctx) {
#if defined(__ANDROID__)
    PTR(u8) request = _arg<0, PTR(u8)>(rdram, ctx);
    PTR(u8) out = _arg<1, PTR(u8)>(rdram, ctx);
    const int32_t sprite = MEM_W(0x00, request);
    const int32_t width = MEM_W(0x04, request);
    const int32_t height = MEM_W(0x08, request);
    const int32_t codec = MEM_W(0x0C, request);
    const int32_t tile_x = MEM_W(0x10, request);
    const int32_t tile_y = MEM_W(0x14, request);
    const int32_t tiles_x = std::max<int32_t>(1, MEM_W(0x18, request));
    const int32_t tiles_y = std::max<int32_t>(1, MEM_W(0x1C, request));

    recompinput::GameInput input;
    if (!button_icons_enabled || width <= 0 || height <= 0 || width > 256 || height > 256 || codec < 0 || codec > 3
            || !game_input_for_sprite(sprite, input) || recompinput::connected_controller_name().empty()) {
        _return<int32_t>(ctx, 0);
        return;
    }
    const int profile = recompinput::profiles::get_sp_controller_profile_index();
    if (profile < 0) {
        _return<int32_t>(ctx, 0);
        return;
    }
    const std::string name = icon_name_for(recompinput::profiles::get_input_binding(profile, input, 0),
        recompinput::get_face_button_style());
    const std::vector<uint8_t> *icon = name.empty() ? nullptr : load_button_icon(name);
    {
        // Once per sprite and icon, for checking what the game asks for.
        static std::set<std::string> logged;
        const std::string key = std::to_string(sprite) + ":" + name;
        if (logged.insert(key).second) {
            __android_log_print(ANDROID_LOG_INFO, "DK64Icons", "sprite 0x%X %dx%d codec %d tile %d,%d of %dx%d -> %s%s",
                sprite, width, height, codec, tile_x, tile_y, tiles_x, tiles_y, name.empty() ? "(none)" : name.c_str(),
                icon == nullptr && !name.empty() ? " (missing)" : "");
        }
    }
    if (icon == nullptr) {
        _return<int32_t>(ctx, 0);
        return;
    }

    const int full_w = width * tiles_x, full_h = height * tiles_y;
    uint8_t nibble_byte = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t px[4];
            // The game's sprite pictures are stored bottom row first, so the icon is drawn flipped.
            sample_icon(*icon, tile_x * width + x, full_h - 1 - (tile_y * height + y), full_w, full_h, px);
            const int i = y * width + x;
            const uint8_t luma = static_cast<uint8_t>((px[0] * 77 + px[1] * 150 + px[2] * 29) >> 8);
            switch (codec) {
                case 3:   // RGBA32
                    MEM_B(i * 4 + 0, out) = px[0];
                    MEM_B(i * 4 + 1, out) = px[1];
                    MEM_B(i * 4 + 2, out) = px[2];
                    MEM_B(i * 4 + 3, out) = px[3];
                    break;
                case 2: { // RGBA16 (5551)
                    const uint16_t v = static_cast<uint16_t>(((px[0] >> 3) << 11) | ((px[1] >> 3) << 6) | ((px[2] >> 3) << 1) | (px[3] >= 128 ? 1 : 0));
                    MEM_B(i * 2 + 0, out) = static_cast<uint8_t>(v >> 8);
                    MEM_B(i * 2 + 1, out) = static_cast<uint8_t>(v & 0xFF);
                    break;
                }
                case 1:   // IA8
                    MEM_B(i, out) = static_cast<uint8_t>((luma & 0xF0) | (px[3] >> 4));
                    break;
                default: { // IA4: two pixels per byte, the first in the high nibble
                    const uint8_t nibble = static_cast<uint8_t>(((luma >> 5) << 1) | (px[3] >= 128 ? 1 : 0));
                    if ((i & 1) == 0) {
                        nibble_byte = static_cast<uint8_t>(nibble << 4);
                    } else {
                        MEM_B(i / 2, out) = static_cast<uint8_t>(nibble_byte | nibble);
                    }
                    break;
                }
            }
        }
    }
    _return<int32_t>(ctx, 1);
#else
    (void)rdram;
    _return<int32_t>(ctx, 0);
#endif
}

// patches/android_hooks.c: the Kong a Touch Kong Swap is going to, or -1.
extern "C" void recomp_get_tag_target(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
#if defined(__ANDROID__)
    _return<int32_t>(ctx, dk64_android_tag_target());
#else
    _return<int32_t>(ctx, -1);
#endif
}

extern "C" void recomp_get_gyro_deltas(uint8_t* rdram, recomp_context* ctx) {
    float* x_out = _arg<0, float*>(rdram, ctx);
    float* y_out = _arg<1, float*>(rdram, ctx);

    // TODO: use controller number
    recompinput::get_gyro_deltas(0, x_out, y_out);
}

extern "C" void recomp_get_mouse_deltas(uint8_t* rdram, recomp_context* ctx) {
    float* x_out = _arg<0, float*>(rdram, ctx);
    float* y_out = _arg<1, float*>(rdram, ctx);

    recompinput::get_mouse_deltas(x_out, y_out);
}

extern "C" void recomp_powf(uint8_t* rdram, recomp_context* ctx) {
    float a = _arg<0, float>(rdram, ctx);
    float b = ctx->f14.fl; //_arg<1, float>(rdram, ctx);

    _return(ctx, std::pow(a, b));
}

extern "C" void recomp_get_target_framerate(uint8_t* rdram, recomp_context* ctx) {
    int frame_divisor = _arg<0, u32>(rdram, ctx);

    _return(ctx, ultramodern::get_target_framerate(60 / frame_divisor));
}

extern "C" void recomp_get_window_resolution(uint8_t* rdram, recomp_context* ctx) {
    int width, height;
    recompui::get_window_size(width, height);

    gpr width_out = _arg<0, PTR(u32)>(rdram, ctx);
    gpr height_out = _arg<1, PTR(u32)>(rdram, ctx);

    MEM_W(0, width_out) = (u32)width;
    MEM_W(0, height_out) = (u32)height;
}

extern "C" void recomp_get_target_aspect_ratio(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::renderer::GraphicsConfig graphics_config = ultramodern::renderer::get_graphics_config();
    float original = _arg<0, float>(rdram, ctx);
    int width, height;
    recompui::get_window_size(width, height);

    switch (graphics_config.ar_option) {
        case ultramodern::renderer::AspectRatio::Original:
        default:
            _return(ctx, original);
            return;
        case ultramodern::renderer::AspectRatio::Expand:
            _return(ctx, std::max(static_cast<float>(width) / height, original));
            return;
    }
}

extern "C" void recomp_get_bgm_volume(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, dk64::get_bgm_volume() / 2.5f);
}

extern "C" void recomp_get_sfx_volume(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, dk64::get_sfx_volume() / 2.5f);
}

extern "C" void recomp_get_draw_distance(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, dk64::get_draw_distance() * 50.0f);
}

extern "C" void recomp_get_story_skip(uint8_t* rdram, recomp_context* ctx) {
    switch (dk64::get_story_skip()) {
        case dk64::StorySkipMode::Off:
            _return(ctx, 0);
            return;
        case dk64::StorySkipMode::IntroStory:
            _return(ctx, 1);
            return;
        case dk64::StorySkipMode::VanillaOn:
            _return(ctx, 2);
            return;
    }
}

extern "C" void recomp_get_camera_type(uint8_t* rdram, recomp_context* ctx) {
    switch (dk64::get_camera_type()) {
        case dk64::CameraTypeMode::Free:
            _return(ctx, 0);
            return;
        case dk64::CameraTypeMode::Follow:
            _return(ctx, 1);
            return;
        case dk64::CameraTypeMode::BetterFree:
            _return(ctx, 2);
            return;
        case dk64::CameraTypeMode::Analog:
            _return(ctx, 3);
            return;
    }
}

extern "C" void recomp_get_lightning_frequency(uint8_t* rdram, recomp_context* ctx) {
    switch (dk64::get_lightning_flash()) {
        case dk64::LightningFlashMode::Off:
            _return(ctx, 0);
            return;
        case dk64::LightningFlashMode::Reduced:
            _return(ctx, 10);
            return;
        case dk64::LightningFlashMode::Vanilla:
            _return(ctx, 1);
            return;
    }
}

extern "C" void recomp_get_cutscene_bordering(uint8_t* rdram, recomp_context* ctx) {
    switch (dk64::get_cutscene_borders()) {
        case dk64::CutsceneBordersMode::Off:
            _return(ctx, 0);
            return;
        case dk64::CutsceneBordersMode::On:
            _return(ctx, 40);
            return;
    }
}

extern "C" void recomp_get_mp_enabled(uint8_t* rdram, recomp_context* ctx) {
    switch (dk64::get_multiplayer_enabled()) {
        case dk64::MultiplayerEnabled::Off:
            _return(ctx, 0);
            return;
        case dk64::MultiplayerEnabled::On:
            _return(ctx, 1);
            return;
    }
}

extern "C" void recomp_get_ui_bounds(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::renderer::GraphicsConfig graphics_config = ultramodern::renderer::get_graphics_config();
    int width, height;
    recompui::get_window_size(width, height);
    switch (graphics_config.ar_option) {
        case ultramodern::renderer::AspectRatio::Original:
        default:
            width = 320;
            break;
        case ultramodern::renderer::AspectRatio::Expand:
            width = (width * 240) / height;
            if (width < 320) {
                width = 320;
            }
            break;
    }
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);
    *x_out = width;
    *y_out = 240;
    return;
}

extern "C" void recomp_get_ui_pillar(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::renderer::GraphicsConfig graphics_config = ultramodern::renderer::get_graphics_config();
    int width, height, delta;
    recompui::get_window_size(width, height);
    switch (graphics_config.ar_option) {
        case ultramodern::renderer::AspectRatio::Original:
        default:
            width = 320;
            break;
        case ultramodern::renderer::AspectRatio::Expand:
            width = (width * 240) / height;
            if (width < 320) {
                width = 320;
            }
            break;
    }
    delta = 0;
    switch (graphics_config.hr_option) {
        default:
        case ultramodern::renderer::HUDRatioMode::Original:
            delta = (width - 320) / 2;
            break;
        case ultramodern::renderer::HUDRatioMode::Clamp16x9:
            delta = (width - 426) / 2;
            break;
        case ultramodern::renderer::HUDRatioMode::Full:
            delta = 0;
            break;
    }
    if (delta < 0) {
        delta = 0;
    }
    _return(ctx, delta);
    return;
}

extern "C" void recomp_get_analog_cam_sensitivity(uint8_t* rdram, recomp_context* ctx) {
    _return<uint32_t>(ctx, dk64::get_analog_cam_sensitivity());
}


extern "C" void recomp_time_us(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, static_cast<u32>(std::chrono::duration_cast<std::chrono::microseconds>(ultramodern::time_since_start()).count()));
}

extern "C" void recomp_load_overlays(uint8_t * rdram, recomp_context * ctx) {
    u32 rom = _arg<0, u32>(rdram, ctx);
    PTR(void) ram = _arg<1, PTR(void)>(rdram, ctx);
    u32 size = _arg<2, u32>(rdram, ctx);

    load_overlays(rom, ram, size);
}

extern "C" void recomp_high_precision_fb_enabled(uint8_t * rdram, recomp_context * ctx) {
    _return(ctx, static_cast<s32>(recompui::renderer::RT64HighPrecisionFBEnabled()));
}

extern "C" void recomp_get_resolution_scale(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, ultramodern::get_resolution_scale());
}

extern "C" void recomp_get_inverted_axes(uint8_t* rdram, recomp_context* ctx) {
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);

    dk64::CameraInvertMode mode = dk64::get_camera_invert_mode();

    *x_out = (mode == dk64::CameraInvertMode::InvertX || mode == dk64::CameraInvertMode::InvertBoth);
    *y_out = (mode == dk64::CameraInvertMode::InvertY || mode == dk64::CameraInvertMode::InvertBoth);
}

extern "C" void recomp_get_analog_inverted_axes(uint8_t* rdram, recomp_context* ctx) {
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);

    dk64::CameraInvertMode mode = dk64::get_third_person_camera_mode();

    *x_out = (mode == dk64::CameraInvertMode::InvertX || mode == dk64::CameraInvertMode::InvertBoth);
    *y_out = (mode == dk64::CameraInvertMode::InvertY || mode == dk64::CameraInvertMode::InvertBoth);
}

extern "C" void recomp_get_swimming_inverted_axes(uint8_t* rdram, recomp_context* ctx) {
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);

    dk64::CameraInvertMode mode = dk64::get_swimming_invert_mode();

    *x_out = (mode == dk64::CameraInvertMode::InvertX || mode == dk64::CameraInvertMode::InvertBoth);
    *y_out = (mode == dk64::CameraInvertMode::InvertY || mode == dk64::CameraInvertMode::InvertBoth);
}

extern "C" void recomp_get_first_person_inverted_axes(uint8_t* rdram, recomp_context* ctx) {
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);

    dk64::CameraInvertMode mode = dk64::get_first_person_invert_mode();

    *x_out = (mode == dk64::CameraInvertMode::InvertX || mode == dk64::CameraInvertMode::InvertBoth);
    *y_out = (mode == dk64::CameraInvertMode::InvertY || mode == dk64::CameraInvertMode::InvertBoth);
}

extern "C" void recomp_get_gyro_inverted_axes(uint8_t* rdram, recomp_context* ctx) {
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);

    dk64::CameraInvertMode mode = dk64::get_gyro_invert_mode();

    *x_out = (mode == dk64::CameraInvertMode::InvertX || mode == dk64::CameraInvertMode::InvertBoth);
    *y_out = (mode == dk64::CameraInvertMode::InvertY || mode == dk64::CameraInvertMode::InvertBoth);
}

extern "C" void recomp_get_mouse_inverted_axes(uint8_t* rdram, recomp_context* ctx) {
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);

    dk64::CameraInvertMode mode = dk64::get_mouse_invert_mode();

    *x_out = (mode == dk64::CameraInvertMode::InvertX || mode == dk64::CameraInvertMode::InvertBoth);
    *y_out = (mode == dk64::CameraInvertMode::InvertY || mode == dk64::CameraInvertMode::InvertBoth);
}

extern "C" void recomp_get_right_analog_inputs(uint8_t* rdram, recomp_context* ctx) {
    float* x_out = _arg<0, float*>(rdram, ctx);
    float* y_out = _arg<1, float*>(rdram, ctx);

    // Don't return right analog inputs while game input is disabled.
    if (recompinput::game_input_disabled()) {
        *x_out = 0.0f;
        *y_out = 0.0f;
        return;
    }

    // TODO: Use controller number.
    recompinput::get_right_analog(0, x_out, y_out);
}

extern "C" void recomp_set_right_analog_suppressed(uint8_t* rdram, recomp_context* ctx) {
    s32 suppressed = _arg<0, s32>(rdram, ctx);

    recompinput::set_right_analog_suppressed(suppressed);
}

constexpr uint32_t k1_to_phys(uint32_t addr) {
    return addr & 0x1FFFFFFF;
}

extern "C" void osPiReadIo_recomp(RDRAM_ARG recomp_context * ctx) {
    uint32_t devAddr = recomp::rom_base | ctx->r4;
    gpr dramAddr = ctx->r5;
    uint32_t physical_addr = k1_to_phys(devAddr);

    if (physical_addr > recomp::rom_base) {
        // cart rom
        recomp::do_rom_pio(PASS_RDRAM dramAddr, physical_addr);
    } else {
        // sram
        assert(false && "SRAM ReadIo unimplemented");
    }

    ctx->r2 = 0;
}

extern "C" void osPfsInit_recomp(uint8_t * rdram, recomp_context* ctx) {
    ctx->r2 = 11; // PFS_ERR_DEVICE
}

// u32 rom_addr, void *ram_addr, u32 size
extern "C" void recomp_load_overlays_by_rom(uint8_t* rdram, recomp_context* ctx) {
    u32 rom_addr = _arg<0, u32>(rdram, ctx);
    PTR(void) ram_addr = _arg<1, PTR(void)>(rdram, ctx);
    u32 size = _arg<2, u32>(rdram, ctx);

    load_overlays(rom_addr, ram_addr, size);
}

extern "C" void recomp_abort(uint8_t* rdram, recomp_context* ctx) {
    std::string msg = _arg_string<0>(rdram, ctx);
    recompui::message_box(msg.c_str());
    assert(false);
    ultramodern::error_handling::quick_exit(__FILE__, __LINE__, __FUNCTION__);
}

extern "C" void recomp_xxh3(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) data = _arg<0, PTR(void)>(rdram, ctx);
    u32 size = _arg<1, u32>(rdram, ctx);
    XXH3_state_t xxh3;
    XXH3_64bits_reset(&xxh3);

    // Hash 1 byte at a time to account for byteswapping.
    for (size_t i = 0; i < size; i++) {
        XXH3_64bits_update(&xxh3, TO_PTR(u8, data + i), 1);
    }

    uint64_t ret = XXH3_64bits_digest(&xxh3);
    
    ctx->r2 = (int32_t)(ret >> 32);
    ctx->r3 = (int32_t)(ret >> 0);
}

//map compressed rom address -> decompressed rom address, then load the overlay
extern "C" void load_dk64_overlay(uint32_t compressed_rom, int32_t ram_addr, uint32_t size) {
    uint32_t decompressed_rom = 0;
    switch (compressed_rom) {
        case 0x113F0: //global_asm
            decompressed_rom = 0x2000000;
            ram_addr = 0x805FB300;
            size = 0x165D50;
            break;
        case 0xCBE70: //menu
            decompressed_rom = 0x2165D50;
            ram_addr = 0x80024000;
            size = 0xFF10;
            break;
        case 0xD4B00: //multiplayer
            decompressed_rom = 0x2175C60;
            ram_addr = 0x80024000;
            size = 0x3100;
            break;
        case 0xD6B00: //minecart
            decompressed_rom = 0x2178D60;
            ram_addr = 0x80024000;
            size = 0x4E10;
            break;
        case 0xD9A40: //bonus
            decompressed_rom = 0x217DB70;
            ram_addr = 0x80024000;
            size = 0x9EF0;
            break;
        case 0xDF600: //race
            decompressed_rom = 0x2187A60;
            ram_addr = 0x80024000;
            size = 0xC160;
            break;
        case 0xE6780: //critter
            decompressed_rom = 0x2193BC0;
            ram_addr = 0x80024000;
            size = 0x61B0;
            break;
        case 0xEA0B0: //boss
            decompressed_rom = 0x2199D70;
            ram_addr = 0x80024000;
            size = 0x12DC0;
            break;
        case 0xF41A0: //arcade
            decompressed_rom = 0x21ACB30;
            ram_addr = 0x80024000;
            size = 0x26C00;
            break;
        case 0xFD2F0: //jetpac
            decompressed_rom = 0x21D3730;
            ram_addr = 0x80024000;
            size = 0xAC30;
            break;
    }
    if (decompressed_rom != 0) {
        load_overlays(decompressed_rom, ram_addr, size);
    }
}

extern "C" void boot_osPiRawStartDma(uint8_t* rdram, recomp_context* ctx) {
    uint32_t direction = ctx->r4;
    uint32_t device_address = ctx->r5;
    gpr rdram_address = ctx->r6;
    uint32_t size = ctx->r7;

    assert(direction == 0); // Only reads

    printf("boot_osPiRawStartDma: Rom %08X, Ram %08X, Size: %08X\n", device_address, rdram_address, size);

    // Complete the DMA synchronously (the game immediately waits until it's done anyways)
    recomp::do_rom_read(rdram, rdram_address, device_address + recomp::rom_base, size);
}

extern "C" void __f_to_ull_recomp(uint8_t * rdram, recomp_context * ctx) {
    uint64_t ret = (uint64_t)ctx->f12.fl;
    ctx->r2 = (int32_t)(ret >> 32);
    ctx->r3 = (int32_t)(ret >> 0);
}

extern "C" void __osSpSetStatus_recomp(RDRAM_ARG recomp_context * ctx) {
    //assert(false && "SRAM ReadIo unimplemented");
}

extern "C" void osViGetCurrentMode_recomp(uint8_t* rdram, recomp_context* ctx) {
    constexpr gpr os_vi_curr_ptr_addr = 0xFFFFFFFF80010190ull;
    const gpr vi_curr_ctx = (gpr)(int32_t)MEM_W(0, os_vi_curr_ptr_addr);

    if (vi_curr_ctx == 0) {
        ctx->r2 = 0;
        return;
    }

    const gpr modep = (gpr)(int32_t)MEM_W(0x8, vi_curr_ctx);
    if (modep == 0) {
        ctx->r2 = 0;
        return;
    }

    ctx->r2 = MEM_BU(0x3, modep);
}