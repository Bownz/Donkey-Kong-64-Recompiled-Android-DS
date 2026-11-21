#include "donk_config.h"
#include "recompui/recompui.h"
#include "recompui/config.h"
#include "recompinput/recompinput.h"
#include "donk_sound.h"
#include "donk_draw.h"
#include "donk_support.h"
#include "ultramodern/config.hpp"
#include "librecomp/files.hpp"
#include "librecomp/config.hpp"
#if defined(__ANDROID__)
#include <fstream>
#include "recompinput/profiles.h"
#include "librecomp/game.hpp"
#endif
#if defined(__ANDROID__)
#include "../../android/jni/gpu_driver.h"
#endif
#include "util/file.h"
#include <filesystem>
#include <fstream>
#include <iomanip>

#if defined(_WIN32)
#include <Shlobj.h>
#elif defined(__linux__)
#include <unistd.h>
#include <pwd.h>
#elif defined(__APPLE__)
#include "apple/rt64_apple.h"
#endif

#if defined(__ANDROID__)
#include <atomic>
#include <jni.h>
#include "ultramodern/ultramodern.hpp"
extern "C" void dk64_android_set_gpu_keep_awake(bool on);
// Read by the on-screen touch controls (TouchInput.nativeUiState).
static std::atomic<bool> dk64_android_analog_camera{false};

// Bit 1: a menu (the launcher, the menu bar, settings) is taking input, so the touch
// buttons are hidden and touches go to it. Bit 2: the Analog Camera is selected.
// Bit 4: Drag to Move Camera is on. Bit 16: the game has started (not just the launcher).
extern "C" JNIEXPORT jint JNICALL
Java_io_github_bownz_dk64recomp_TouchInput_nativeUiState(JNIEnv*, jclass) {
    jint bits = 0;
    if (recompui::is_context_capturing_input()) {
        bits |= 1;
    }
    if (dk64_android_analog_camera.load()) {
        bits |= 2;
    }
    if (dk64::android::drag_camera_enabled()) {
        bits |= 4;
    }
    if (ultramodern::is_game_started()) {
        bits |= 16;
    }
    return bits;
}
#endif

static void add_general_options(recomp::config::Config &config) {
    using EnumOptionVector = const std::vector<recomp::config::ConfigOptionEnumOption>;

    static EnumOptionVector camera_invert_mode_options = {
        {dk64::CameraInvertMode::InvertNone, "InvertNone", "None"},
        {dk64::CameraInvertMode::InvertX, "InvertX", "Invert X"},
    };
    config.add_enum_option(
        dk64::configkeys::general::third_person_camera_invert_mode,
        "Invert Camera",
        "Inverts the camera controls for the third person camera if it's enabled.",
        camera_invert_mode_options,
        dk64::CameraInvertMode::InvertNone
    );
    static EnumOptionVector first_person_invert_mode_options = {
        {dk64::CameraInvertMode::InvertNone, "InvertNone", "None"},
        {dk64::CameraInvertMode::InvertX, "InvertX", "Invert X"},
        {dk64::CameraInvertMode::InvertY, "InvertY", "Invert Y"},
        {dk64::CameraInvertMode::InvertBoth, "InvertBoth", "Invert Both"}
    };
    config.add_enum_option(
        dk64::configkeys::general::first_person_invert_mode,
        "Invert First Person View",
        "Inverts the camera controls in first person view. <recomp-color primary>Invert Y</recomp-color> is the default and matches the original game.",
        first_person_invert_mode_options,
        dk64::CameraInvertMode::InvertY
    );
    static EnumOptionVector gyro_invert_mode_options = {
        {dk64::CameraInvertMode::InvertNone, "InvertNone", "None"},
        {dk64::CameraInvertMode::InvertX, "InvertX", "Invert X"},
        {dk64::CameraInvertMode::InvertY, "InvertY", "Invert Y"},
        {dk64::CameraInvertMode::InvertBoth, "InvertBoth", "Invert Both"}
    };
    config.add_enum_option(
        dk64::configkeys::general::gyro_invert_mode,
        "Invert Gyro",
        "Inverts gyro controls.",
        gyro_invert_mode_options,
        dk64::CameraInvertMode::InvertY
    );

    static EnumOptionVector mouse_invert_mode_options = {
        {dk64::CameraInvertMode::InvertNone, "InvertNone", "None"},
        {dk64::CameraInvertMode::InvertX, "InvertX", "Invert X"},
        {dk64::CameraInvertMode::InvertY, "InvertY", "Invert Y"},
        {dk64::CameraInvertMode::InvertBoth, "InvertBoth", "Invert Both"}
    };
    config.add_enum_option(
        dk64::configkeys::general::mouse_invert_mode,
        "Invert Mouse",
        "Inverts Mouse controls.",
        mouse_invert_mode_options,
        dk64::CameraInvertMode::InvertY
#ifdef __ANDROID__
        , true   // hidden: no mouse camera on Android
#endif
    );
    static EnumOptionVector swimming_invert_options = {
        {dk64::CameraInvertMode::InvertNone, "InvertNone", "None"},
        {dk64::CameraInvertMode::InvertX, "InvertX", "Invert X"},
        {dk64::CameraInvertMode::InvertY, "InvertY", "Invert Y"},
        {dk64::CameraInvertMode::InvertBoth, "InvertBoth", "Invert Both"}
    };
    config.add_enum_option(
        dk64::configkeys::general::swimming_invert_mode,
        "Invert Swimming",
        "Inverts the controls for swimming. <recomp-color primary>Invert Y</recomp-color> is the default and matches the original game.",
        swimming_invert_options,
        dk64::CameraInvertMode::InvertY
    );
    // Story Skip
    static EnumOptionVector story_skip_options = {
        {dk64::StorySkipMode::Off, "Off", "None"},
        {dk64::StorySkipMode::IntroStory, "IntroStory", "Intro Story Only"},
        {dk64::StorySkipMode::VanillaOn, "VanillaOn", "On"}
    };
    config.add_enum_option(
        dk64::configkeys::general::story_skip,
        "Story Skip",
        "Skips some story cutscenes.<br /><recomp-color primary>Off</recomp-color>: No story cutscenes are skipped.<br /><recomp-color primary>Intro Story Only</recomp-color>: Only the 5-minute introductory story is skipped. The K. Rool level intros will still play.<br /><recomp-color primary>On</recomp-color>: Both the introductory story and all K. Rool level intros are skipped.",
        story_skip_options,
        dk64::StorySkipMode::Off
    );
    // Camera Type
    static EnumOptionVector camera_type_options = {
        {dk64::CameraTypeMode::Free, "Free", "Free Cam"},
        {dk64::CameraTypeMode::Follow, "Follow", "Follow Cam"},
        {dk64::CameraTypeMode::BetterFree, "BetterFree", "Better Free Cam"},
        {dk64::CameraTypeMode::Analog, "Analog", "Analog Camera"},
        {dk64::CameraTypeMode::Modern, "Modern", "Modern Camera"}
    };
    config.add_enum_option(
        dk64::configkeys::general::camera_type,
        "Camera Type",
        "Changes the camera behavior.<br /><recomp-color primary>Free Cam</recomp-color>: Camera can be controlled via pressing C-Left and C-Right. The camera does not try to push itself behind the player.<br /><recomp-color primary>Follow Cam</recomp-color>: Similar to <recomp-color secondary>Free Cam</recomp-color>, but the camera tries to push itself behind the player.<br /><recomp-color primary>Better Free Cam</recomp-color>: Similar to <recomp-color secondary>Free Cam</recomp-color>, but instead of a button press turning the camera 45 degrees, holding the button moves the camera at 5 degrees per frame.<br /><recomp-color primary>Analog Camera</recomp-color>: Control of the camera is controlled by your right analogue stick. It is advised that C-Up is remapped to something outside your right analogue stick in order to have a good time with this.<br /><recomp-color primary>Modern Camera</recomp-color>: Right stick camera that feels like recent 3D platformers: smooth turning, up/down tilts the camera, L swings it behind your Kong, and it drifts gently back behind you while you run. It also eases out of the game's fixed-camera spots instead of cutting, pulls back out from walls smoothly, widens the view a little at full speed, and coasts to a stop when you let go of the stick.",
        camera_type_options,
        dk64::CameraTypeMode::Free
    );
#if defined(__ANDROID__)
    // The touch controls show a second (camera) stick while the Analog Camera is selected.
    config.add_option_change_callback(dk64::configkeys::general::camera_type,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            dk64_android_analog_camera.store(std::get<uint32_t>(cur) == static_cast<uint32_t>(dk64::CameraTypeMode::Analog) ||
                std::get<uint32_t>(cur) == static_cast<uint32_t>(dk64::CameraTypeMode::Modern));
        });
#endif
    config.add_number_option(
        dk64::configkeys::general::analog_camera_sensitivity,
        "Analog Camera Sensitivity",
        "Sets the sensitivity of the right stick analog camera, if enabled.",
        1, 10, 1, 0, false, 3
    );
    config.add_option_hidden_dependency(
        dk64::configkeys::general::analog_camera_sensitivity,
        dk64::configkeys::general::camera_type,
        dk64::CameraTypeMode::Free,
        dk64::CameraTypeMode::Follow,
        dk64::CameraTypeMode::BetterFree
    );
    // Lightning Flashes
    static EnumOptionVector lightning_flash_options = {
        {dk64::LightningFlashMode::Off, "Off", "Off"},
        {dk64::LightningFlashMode::Reduced, "Reduced", "Reduced"},
        {dk64::LightningFlashMode::Vanilla, "Vanilla", "Vanilla"}
    };
    config.add_enum_option(
        dk64::configkeys::general::lightning_flashes,
        "Lightning Flash Frequency",
        "Changes the frequency of lightning flashes within the game.<br /><recomp-color primary>Vanilla</recomp-color>: Lightning flashes at vanilla frequency<br /><recomp-color primary>Reduced</recomp-color>: Lightning flashes at 10x as less<br /><recomp-color primary>Off</recomp-color>: Lightning flashes are completely disabled",
        lightning_flash_options,
        dk64::LightningFlashMode::Reduced
    );
    static EnumOptionVector skip_launcher_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
    };
    config.add_enum_option(
        dk64::configkeys::general::skip_launcher,
        "Skip Launcher",
        "Goes straight into the game when it starts, without the launcher screen "
        "(Start Game, Setup Controls, Settings, Mods). Everything there is still in this menu.<br />"
        "Only skips once your ROM has been loaded, and not when an enabled mod adds game modes to pick from.",
        skip_launcher_options,
        0u
    );
}

template <typename T = uint32_t>
T get_general_config_enum_value(const std::string& option_id) {
    return static_cast<T>(std::get<uint32_t>(recompui::config::get_general_config().get_option_value(option_id)));
}

template <typename T = uint32_t>
T get_general_config_number_value(const std::string& option_id) {
    return static_cast<T>(std::get<double>(recompui::config::get_general_config().get_option_value(option_id)));
}

dk64::CameraInvertMode dk64::get_camera_invert_mode() {
    return get_general_config_enum_value<dk64::CameraInvertMode>(dk64::configkeys::general::camera_invert_mode);
}

dk64::CameraInvertMode dk64::get_third_person_camera_mode() {
    return get_general_config_enum_value<dk64::CameraInvertMode>(dk64::configkeys::general::third_person_camera_invert_mode);
}

dk64::CameraInvertMode dk64::get_swimming_invert_mode() {
    return get_general_config_enum_value<dk64::CameraInvertMode>(dk64::configkeys::general::swimming_invert_mode);
}

dk64::StorySkipMode dk64::get_story_skip() {
    return get_general_config_enum_value<dk64::StorySkipMode>(dk64::configkeys::general::story_skip);
}

dk64::CameraTypeMode dk64::get_camera_type() {
    return get_general_config_enum_value<dk64::CameraTypeMode>(dk64::configkeys::general::camera_type);
}

bool dk64::get_skip_launcher() {
    return get_general_config_enum_value(dk64::configkeys::general::skip_launcher) == 1u;
}

dk64::LightningFlashMode dk64::get_lightning_flash() {
    return get_general_config_enum_value<dk64::LightningFlashMode>(dk64::configkeys::general::lightning_flashes);
}

dk64::CameraInvertMode dk64::get_first_person_invert_mode() {
    return get_general_config_enum_value<dk64::CameraInvertMode>(dk64::configkeys::general::first_person_invert_mode);
}

dk64::CameraInvertMode dk64::get_gyro_invert_mode() {
    return get_general_config_enum_value<dk64::CameraInvertMode>(dk64::configkeys::general::gyro_invert_mode);
}

dk64::CameraInvertMode dk64::get_mouse_invert_mode() {
    return get_general_config_enum_value<dk64::CameraInvertMode>(dk64::configkeys::general::mouse_invert_mode);
}

uint32_t dk64::get_analog_cam_sensitivity() {
    return get_general_config_number_value(dk64::configkeys::general::analog_camera_sensitivity);
}

template <typename T = uint32_t>
T get_graphics_config_enum_value(const std::string& option_id) {
    return static_cast<T>(std::get<uint32_t>(recompui::config::get_graphics_config().get_option_value(option_id)));
}

template <typename T = uint32_t>
T get_graphics_config_number_value(const std::string& option_id) {
    return static_cast<T>(std::get<double>(recompui::config::get_graphics_config().get_option_value(option_id)));
}

template <typename T = uint32_t>
T get_technical_config_enum_value(const std::string& option_id) {
    return static_cast<T>(std::get<uint32_t>(recompui::config::get_config("technical").get_option_value(option_id)));
}

#if defined(__ANDROID__)
extern "C" void dk64_android_set_ui_sounds(int enabled);
#endif

static void add_sound_options(recomp::config::Config &config) {
    config.add_percent_number_option(
        dk64::configkeys::sound::bgm_volume,
        "Background Music Volume",
        "Controls the overall volume of background music.",
        100.0f
    );
    config.add_percent_number_option(
        dk64::configkeys::sound::sfx_volume,
        "SFX Volume",
        "Controls the overall volume of sound effects.",
        100.0f
    );
#if defined(__ANDROID__)
    // Little bongo taps when moving through the menus with a controller.
    static const std::string menu_sounds_option = "android_menu_sounds";
    static const std::vector<recomp::config::ConfigOptionEnumOption> menu_sounds_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
    };
    config.add_enum_option(
        menu_sounds_option,
        "Menu Sounds",
        "Small bongo taps when you move through, select or back out of the menus with a controller.",
        menu_sounds_options,
        1u
    );
    config.add_option_change_callback(menu_sounds_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            if (context != recomp::config::OptionChangeContext::Temporary) {
                dk64_android_set_ui_sounds(static_cast<int>(std::get<uint32_t>(cur)));
            }
        });
#endif
}
template <typename T = uint32_t>
T get_sound_config_number_value(const std::string& option_id) {
    return static_cast<T>(std::get<double>(recompui::config::get_sound_config().get_option_value(option_id)));
}

int dk64::get_bgm_volume() {
    return get_sound_config_number_value<int>(dk64::configkeys::sound::bgm_volume);
}

int dk64::get_sfx_volume() {
    return get_sound_config_number_value<int>(dk64::configkeys::sound::sfx_volume);
}

#if defined(__ANDROID__)
#include "ultramodern/ultramodern.hpp"
// Video > Graphics Driver: a button that opens the driver menu (pick, install, remove).
// The driver loads when the app starts, so changing it restarts the app, which is only
// offered on the main menu.
static void add_gpu_driver_option(recomp::config::Config &config) {
    recompui::config::LayoutEntry button;
    button.kind = recompui::config::LayoutEntry::Kind::Button;
    button.text = "Graphics Driver";
    button.description = "Choose the Vulkan driver used for rendering: the device's own, or a custom one such as Turnip. "
        "Install and remove drivers here too. Changing the driver restarts the app, so it can only be done from the main menu. "
        "In use now: " + dk64::android::active_gpu_driver() + ". App version: " + dk64::android::app_version() + ".";
    button.on_press = []() {
        if (ultramodern::is_game_started()) {
            recompui::open_info_prompt("Graphics Driver",
                "The graphics driver can only be changed from the main menu, because the app restarts to use it. "
                "Use QUIT > Main Menu, then open Settings > Video > Graphics Driver.",
                "OK", []() {});
            return;
        }
        dk64::android::open_gpu_driver_manager();
    };
    recompui::config::add_page_button(config, std::move(button));
}
#endif

#if defined(__ANDROID__)
// Android: the "Dual Screen" tab (the bottom screen on handhelds with two screens).
extern "C" void dk64_android_set_touch_controls_mode(int mode);
void dk64_android_choose_experience();   // (below)
extern "C" void dk64_android_set_button_icons(int enabled);
extern "C" int dk64_android_has_second_screen();
extern "C" void dk64_android_edit_touch_layout();

// Combined options (see add_dual_screen_options): an option made of a few others.
struct CombinedOption {
    const char *id;
    const char *name;
    const char *desc;
    std::vector<recomp::config::ConfigOptionEnumOption> choices;   // the last one: Custom
    std::vector<std::string> parts;                               // the options it sets
    std::vector<std::vector<uint32_t>> sets;                       // their values, per choice
    bool second_screen_only;
};
static bool combined_syncing = false;

static const std::vector<CombinedOption> &combined_options() {
    static const std::vector<CombinedOption> list = {
        { "android_bottom_layout", "Bottom Screen",
          "What the second screen shows: <recomp-color primary>Info only</recomp-color> (health, Kongs and collectibles), "
          "<recomp-color primary>Info + Buttons</recomp-color> (also the oranges, gun, instrument, camera and first person buttons), "
          "or <recomp-color primary>Buttons on Z</recomp-color> (the buttons slide in while you hold Z). Off leaves the second screen free.",
          { {0u, "Off", "Off"}, {1u, "Info", "Info only"}, {2u, "Buttons", "Info + Buttons"}, {3u, "OnZ", "Buttons on Z"}, {4u, "Custom", "Custom"} },
          { "android_bottom_screen", "android_bottom_buttons", "android_bottom_first_person" },
          { {0u, 1u, 1u}, {1u, 0u, 0u}, {1u, 1u, 1u}, {1u, 2u, 1u} }, true },
        { "android_touch_feedback_level", "Touch Feedback",
          "A short vibration when you press a touch button, and how strong.",
          { {0u, "Off", "Off"}, {1u, "Light", "Light"}, {2u, "Medium", "Medium"}, {3u, "Strong", "Strong"}, {4u, "Custom", "Custom"} },
          { "android_touch_feedback", "android_touch_feedback_strength" },
          { {0u, 2u}, {1u, 1u}, {1u, 2u}, {1u, 3u} }, false },
        { "android_feel_rumbles", "Feel Rumbles",
          "Small rumbles for game feel, like recent 3D platformers: landings, skids, each banana and coin, a Golden Banana, "
          "shots and oranges, getting hurt, running into walls, playing an instrument, and your hits landing. On your controller, or the phone with touch controls; as strong as Rumble Strength.",
          { {0u, "Off", "Off"}, {1u, "On", "On"}, {2u, "Custom", "Custom"} },
          { "android_haptic_feedback", "android_hit_feel" },
          { {0u, 0u}, {1u, 1u} }, false },
        { "android_first_person_mode", "First Person",
          "<recomp-color primary>Classic</recomp-color>: as on the N64. "
          "<recomp-color primary>Right Stick Looks</recomp-color>: the game's own view, looking with the right stick. "
          "<recomp-color primary>Modern</recomp-color>: walk with the left stick and look with the right (in the water too), "
          "ZR shoots, B leaves, with smoother looking, a kick when you shoot and a dip when you land.",
          { {0u, "Classic", "Classic"}, {1u, "RightStick", "Right Stick Looks"}, {2u, "Modern", "Modern"}, {3u, "Custom", "Custom"} },
          { "android_fp_right_stick", "android_fp_walk", "android_fp_polish" },
          { {0u, 0u, 0u}, {1u, 0u, 0u}, {1u, 1u, 1u} }, false },
        { "android_jump_help", "Jump Help",
          "<recomp-color primary>Jump Assist</recomp-color>: a jump pressed just before landing, or just after running off a ledge, still happens. "
          "<recomp-color primary>Full</recomp-color>: also steer much more in mid-air (pulling back brakes instead of flipping round).",
          { {0u, "Off", "Off"}, {1u, "Assist", "Jump Assist"}, {2u, "Full", "Full"}, {3u, "Custom", "Custom"} },
          { "android_jump_assist", "android_air_control" },
          { {0u, 0u}, {1u, 0u}, {1u, 1u} }, false },
        { "android_movement", "Movement",
          "<recomp-color primary>Modern</recomp-color>: Odyssey-style running. Speed builds and eases off smoothly, "
          "reversing gives a quick skid turn, running feels more responsive, and you can jump straight out of a punch or a roll, keeping its speed.",
          { {0u, "Classic", "Classic"}, {1u, "Modern", "Modern"}, {2u, "Custom", "Custom"} },
          { "android_quick_turn", "android_smooth_running", "android_movement_polish", "android_move_chaining" },
          { {0u, 0u, 0u, 0u}, {1u, 1u, 1u, 1u} }, false },
    };
    return list;
}

// Sets each combined option from the options it's made of (Custom if they match none of its
// choices). Called once the settings are loaded, and after the Experience changes them.
void dk64_sync_combined_options() {
    recomp::config::Config &dual = recompui::config::get_config("dual_screen");
    combined_syncing = true;
    for (const CombinedOption &co : combined_options()) {
        uint32_t found = static_cast<uint32_t>(co.sets.size());   // Custom
        for (size_t c = 0; c < co.sets.size() && found == co.sets.size(); c++) {
            bool match = true;
            for (size_t i = 0; i < co.parts.size(); i++) {
                const auto v = dual.get_option_value(co.parts[i]);
                // Off for the bottom screen / touch feedback: the others don't matter.
                if ((i > 0) && (c == 0) && (co.sets[0][0] == 0u) &&
                    (std::string(co.id) == "android_bottom_layout" || std::string(co.id) == "android_touch_feedback_level")) {
                    continue;
                }
                if (std::get<uint32_t>(v) != co.sets[c][i]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                found = static_cast<uint32_t>(c);
            }
        }
        if (std::get<uint32_t>(dual.get_option_value(co.id)) != found) {
            dual.update_option_value(co.id, found);
        }
    }
    combined_syncing = false;
}

static void add_dual_screen_options(recomp::config::Config &config) {
    using EnumOptionVector = const std::vector<recomp::config::ConfigOptionEnumOption>;
    // Devices with one screen: the bottom screen's options (Bottom Screen, Touch Kong Swap)
    // aren't shown - there's nowhere to put the bottom screen.
    const bool one_screen = dk64_android_has_second_screen() == 0;
    // On-screen touch buttons on the game screen: Auto shows them when no controller is
    // connected (DK64Activity keeps the setting; this passes it on).
    static const std::string touch_controls_option = "android_touch_controls";
    static EnumOptionVector touch_controls_options = {
        {0u, "Auto", "Auto"},
        {1u, "On", "On"},
        {2u, "Off", "Off"},
    };
    config.add_enum_option(
        touch_controls_option,
        "Touch Controls",
        "On-screen buttons over the game. <b>Auto</b> shows them only when no controller is connected.",
        touch_controls_options,
        0u
    );
    config.add_option_change_callback(touch_controls_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            if (context != recomp::config::OptionChangeContext::Temporary) {
                dk64_android_set_touch_controls_mode(static_cast<int>(std::get<uint32_t>(cur)));
            }
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });
    static const std::string bottom_screen_option = "android_bottom_screen";
    static EnumOptionVector bottom_screen_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
    };
    config.add_enum_option(
        bottom_screen_option,
        "Bottom Screen",
        "Shows health, collectibles and action buttons on the second screen, on devices that have one. "
        "Turn it off to leave the second screen free.",
        bottom_screen_options,
        1u,
        one_screen
    );
    // This tab applies changes straight away; save them straight away too (the menu only
    // saves when the tab is closed, which doesn't happen if the app is closed from it).
    config.add_option_change_callback(bottom_screen_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            if (context != recomp::config::OptionChangeContext::Temporary) {
                dk64::android::set_bottom_screen(std::get<uint32_t>(cur) == 1u);
            }
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });
    // Android: tapping a Kong on the bottom screen swaps to them, using the bundled
    // Tag Anywhere mod (Killklli/DK64TagAnywhereRecomp). Off by default; it also needs the mod
    // to be enabled in the Mods menu.
    static const std::string touch_kong_swap_option = "android_touch_kong_swap";
    static EnumOptionVector touch_kong_swap_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
    };
    config.add_enum_option(
        touch_kong_swap_option,
        "Touch Kong Swap",
        "Tap a Kong on the bottom screen to swap to them, anywhere you could tag in a tag barrel. "
        "Needs the <recomp-color primary>Tag Anywhere</recomp-color> mod, which comes with the game but is off by default: "
        "enable it in the Mods menu and restart the game.<br />"
        "When this is off, tapping a Kong only shows their collectibles.",
        touch_kong_swap_options,
        0u,
        one_screen
    );
    config.add_option_change_callback(touch_kong_swap_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_touch_kong_swap(std::get<uint32_t>(cur) == 1u);
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });

    // In-game button icons (text boxes, menus) drawn as the controller's own buttons.
    static const std::string button_icons_option = "android_button_icons";
    static EnumOptionVector button_icons_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
    };
    config.add_enum_option(
        button_icons_option,
        "Controller Button Icons",
        "Shows the button icons in the game's text and menus as the buttons on your controller "
        "that they're set to, instead of the N64's buttons.",
        button_icons_options,
        1u
    );
    config.add_option_change_callback(button_icons_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            if (context != recomp::config::OptionChangeContext::Temporary) {
                dk64_android_set_button_icons(static_cast<int>(std::get<uint32_t>(cur)));
            }
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });

    // The bottom screen's touch buttons (gun, instrument, oranges, camera, first person...).
    static const std::string bottom_buttons_option = "android_bottom_buttons";
    static EnumOptionVector bottom_buttons_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
        {2u, "AppearOnZ", "Appear on Z"},
    };
    config.add_enum_option(
        bottom_buttons_option,
        "Bottom Screen Buttons",
        "The touch buttons on the bottom screen (oranges, gun, instrument, camera, first person). "
        "Turn them off to give the Kong barrels and collectibles the whole screen. With Appear on Z, "
        "the Kong barrels and collectibles have the whole screen, and the oranges, gun, instrument and "
        "camera buttons (and Cancel, Orangstand or a Z + B move when there is one) slide in from the right while you hold Z on your controller.",
        bottom_buttons_options,
        1u,
        one_screen
    );
    config.add_option_change_callback(bottom_buttons_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_bottom_buttons(static_cast<int>(std::get<uint32_t>(cur)));
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });

    // The bottom screen's first person button, kept separately so it can stay with Appear on Z.
    static const std::string bottom_first_person_option = "android_bottom_first_person";
    static EnumOptionVector bottom_first_person_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
    };
    config.add_enum_option(
        bottom_first_person_option,
        "Bottom Screen First Person Button",
        "The first person button in the bottom screen's top-left corner. It stays on screen with "
        "Bottom Screen Buttons set to On or Appear on Z (not Off).",
        bottom_first_person_options,
        1u,
        one_screen
    );
    config.add_option_change_callback(bottom_first_person_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_bottom_first_person(std::get<uint32_t>(cur) == 1u);
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });

    // Dragging a finger on the free part of the screen turns the camera (touch controls only).
    static const std::string drag_camera_option = "android_drag_camera";
    static EnumOptionVector drag_camera_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
    };
    config.add_enum_option(
        drag_camera_option,
        "Drag to Move Camera",
        "With the touch controls on screen, drag a finger anywhere away from the buttons to turn "
        "the camera left and right, or up and down to raise and lower it. Works with every camera "
        "type; the speed follows Analog Camera Sensitivity.",
        drag_camera_options,
        1u
    );
    config.add_option_change_callback(drag_camera_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_drag_camera(std::get<uint32_t>(cur) == 1u);
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });

    // Which button fires the gun (while it's out): B as on the N64, ZR, or either.
    static const std::string gun_button_option = "android_gun_button";
    static EnumOptionVector gun_button_options = {
        {0u, "B", "B"},
        {1u, "ZR", "ZR"},
        {2u, "Both", "B and ZR"},
    };
    config.add_enum_option(
        gun_button_option,
        "Shoot Gun With",
        "Which button fires the gun while it's out. <recomp-color primary>B</recomp-color>: as on the N64. "
        "<recomp-color primary>ZR</recomp-color>: the right trigger (the N64's R) instead, and B puts the gun away (with on-screen prompts). "
        "<recomp-color primary>B and ZR</recomp-color>: either.",
        gun_button_options,
        0u
    );
    config.add_option_change_callback(gun_button_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_gun_button(static_cast<int>(std::get<uint32_t>(cur)));
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });

    // Jump Assist: a jump pressed just before landing still happens on landing, and one pressed
    // just after running off a ledge still jumps (patches/game_feel_patches.c).
    static const std::string jump_assist_option = "android_jump_assist";
    static EnumOptionVector on_off_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
    };
    config.add_enum_option(
        jump_assist_option,
        "Jump Assist",
        "Like recent 3D platformers: a jump pressed a moment before landing happens as soon as you "
        "land, and one pressed a moment after running off a ledge still jumps.",
        on_off_options,
        1u
    );
    config.add_option_change_callback(jump_assist_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_jump_assist(std::get<uint32_t>(cur) == 1u);
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });

    // Quick Turn: no long skid when reversing direction while running.
    static const std::string quick_turn_option = "android_quick_turn";
    config.add_enum_option(
        quick_turn_option,
        "Quick Turn",
        "Reversing direction while running gives a quick skid (with its dust) and the Kong is off the "
        "other way, instead of the N64's long slide.",
        on_off_options,
        1u
    );
    config.add_option_change_callback(quick_turn_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_quick_turn(std::get<uint32_t>(cur) == 1u);
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });

    // Smooth Running: speed builds up and runs down over a few frames (patches/game_feel_patches.c).
    static const std::string smooth_running_option = "android_smooth_running";
    config.add_enum_option(
        smooth_running_option,
        "Smooth Running",
        "Running builds up to full speed over about a third of a second and slows to a stop over a "
        "few frames when you let go, for a little weight, instead of starting and stopping at once.",
        on_off_options,
        1u
    );
    config.add_option_change_callback(smooth_running_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_smooth_running(std::get<uint32_t>(cur) == 1u);
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });

    // Movement Polish: lean into turns, landing squash, momentum jumps (patches/game_feel_patches.c).
    static const std::string movement_polish_option = "android_movement_polish";
    config.add_enum_option(
        movement_polish_option,
        "Movement Polish",
        "Small touches from recent 3D platformers: the Kong leans into tight turns while running, "
        "squashes a little when landing from a big fall, and a jump from a full run carries a bit further.",
        on_off_options,
        1u
    );
    config.add_option_change_callback(movement_polish_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_movement_polish(std::get<uint32_t>(cur) == 1u);
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });

    // More modern-platformer options (patches/game_feel_patches.c), each a bit of the game feel flags.
    struct GameFeelOption { const char *id; const char *name; const char *desc; int bit; };
    static const GameFeelOption game_feel_options[] = {
        { "android_move_chaining", "Move Chaining",
          "Jump straight out of a punch or a running attack, keeping its speed (a roll into a jump goes further).", 32 },
        { "android_air_control", "Air Control",
          "Steer much more in mid-air; pulling back brakes the jump instead of flipping round.", 64 },
        { "android_hit_feel", "Hit Feel",
          "Hits land with weight: a small camera punch (Modern Camera) and a rumble.", 128 },
        { "android_fast_transitions", "Fast Transitions",
          "Fades to and from black through loading zones and doors are almost twice as fast.", 256 },
        { "android_fp_right_stick", "First Person: Right Stick Looks",
          "In first person view (C-Up), look around with the right stick (the left stick still looks too, "
          "unless Walk in First Person is on).", 1024 },
        { "android_fp_walk", "First Person: Walk",
          "In first person view, walk with the left stick (forward, back and sideways) and look with the "
          "right stick. Walls and ledges stop you.", 2048 },
        { "android_camera_polish", "Camera Polish",
          "Modern Camera extras: the camera eases out of the game's fixed-camera areas instead of cutting, "
          "pulls back out from walls smoothly, widens the view a little at full speed, coasts to a stop when "
          "you let go of the stick, and the Kong turns see-through when the camera's squeezed right up to it.", 8192 },
        { "android_fp_polish", "First Person Polish",
          "First person extras: smoother right-stick looking (the gyro stays direct), a small kick when you "
          "shoot and a dip in the view when you land.", 16384 },
    };
    for (const auto &opt : game_feel_options) {
        config.add_enum_option(opt.id, opt.name, opt.desc, on_off_options, 1u);
        const int bit = opt.bit;
        config.add_option_change_callback(opt.id,
            [bit](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
                dk64::android::set_game_feel_bit(bit, std::get<uint32_t>(cur) == 1u);
                if (context == recomp::config::OptionChangeContext::Permanent) {
                    recompui::config::get_config("dual_screen").save_config();
                }
            });
    }

    // Small rumbles for game feel, like Odyssey: landings (stronger the longer the fall, and for
    // the Simian Slam) and skidding round. On the controller, or the phone with touch controls.
    static const std::string haptic_feedback_option = "android_haptic_feedback";
    static EnumOptionVector haptic_feedback_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
    };
    config.add_enum_option(
        haptic_feedback_option,
        "Haptic Feedback",
        "Small rumbles for game feel, like recent 3D platformers: landing (stronger after a long fall "
        "or a Simian Slam) and skidding round when you turn sharply while running. "
        "Also: a soft tap for each banana and coin, a strong one for a Golden Banana, a tick for each "
        "shot and orange, a thump when you're hurt, a bump when you run into a wall, and a beat while playing an instrument. "
        "On your controller, or the phone when you play with the touch controls. The strength follows Rumble Strength.",
        haptic_feedback_options,
        1u
    );
    config.add_option_change_callback(haptic_feedback_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_haptic_feedback(std::get<uint32_t>(cur) == 1u);
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });

    // A little vibration when a touch button is pressed, and how strong it is.
    static const std::string touch_feedback_option = "android_touch_feedback";
    static EnumOptionVector touch_feedback_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
    };
    config.add_enum_option(
        touch_feedback_option,
        "Touch Feedback",
        "A short vibration when you press a touch button.",
        touch_feedback_options,
        1u
    );
    config.add_option_change_callback(touch_feedback_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_touch_feedback(std::get<uint32_t>(cur) == 1u);
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });
    static const std::string touch_feedback_strength_option = "android_touch_feedback_strength";
    static EnumOptionVector touch_feedback_strength_options = {
        {1u, "Light", "Light"},
        {2u, "Medium", "Medium"},
        {3u, "Strong", "Strong"},
    };
    config.add_enum_option(
        touch_feedback_strength_option,
        "Feedback Strength",
        "How strong the touch button vibration is.",
        touch_feedback_strength_options,
        2u
    );
    config.add_option_change_callback(touch_feedback_strength_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_touch_feedback_strength(static_cast<int>(std::get<uint32_t>(cur)));
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });

    // ---- Combined options --------------------------------------------------------------------
    // Each sets a few of the options above (which stay, unlisted, and keep their saved values).
    // They show "Custom" when those have been mixed some other way (by an older version, or a
    // future Advanced list). Their own values are only derived (dk64_sync_combined_options).
    for (const CombinedOption &co : combined_options()) {
        config.add_enum_option(co.id, co.name, co.desc, co.choices, 0u, co.second_screen_only && one_screen);
        const CombinedOption *opt = &co;
        config.add_option_change_callback(co.id,
            [opt](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
                if ((context == recomp::config::OptionChangeContext::Load) || combined_syncing) {
                    return;
                }
                const uint32_t choice = std::get<uint32_t>(cur);
                if (choice >= opt->sets.size()) {
                    return;   // Custom: leave them as they are
                }
                recomp::config::Config &dual = recompui::config::get_config("dual_screen");
                for (size_t i = 0; i < opt->parts.size(); i++) {
                    dual.update_option_value(opt->parts[i], opt->sets[choice][i]);
                }
                if (context == recomp::config::OptionChangeContext::Permanent) {
                    dual.save_config();
                }
            });
    }
}
#endif

static void add_graphics_options(recomp::config::Config &config) {
    using EnumOptionVector = const std::vector<recomp::config::ConfigOptionEnumOption>;
    // Cutscene borders
    static EnumOptionVector cutscene_border_options = {
        {dk64::CutsceneBordersMode::Off, "Off", "Off"},
        {dk64::CutsceneBordersMode::On, "On", "On"}
    };
    config.add_enum_option(
        dk64::configkeys::graphics::cutscene_borders,
        "Cutscene Borders",
        "If turned on, cutscenes will show borders on the top and the bottom of the screen as in the vanilla game.",
        cutscene_border_options,
        dk64::CutsceneBordersMode::On
    );
    config.add_percent_number_option(
        dk64::configkeys::graphics::draw_distance,
        "Draw Distance",
        "Controls the draw distance within the game from 0% (vanilla) to 100% (10x vanilla). Some objects do not get a draw distance increase for technical reasons.",
        0.0f
    );
#if defined(__ANDROID__)
    add_gpu_driver_option(config);

    // Android: game frames per second in the corner of the screen.
    static const std::string show_fps_option = "android_show_fps";
    static EnumOptionVector show_fps_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
    };
    config.add_enum_option(
        show_fps_option,
        "Show FPS",
        "Shows the game's frame rate in the top corner of the screen.",
        show_fps_options,
        0u
    );
    config.add_option_change_callback(show_fps_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            if (context != recomp::config::OptionChangeContext::Temporary) {
                dk64::android::set_show_fps(std::get<uint32_t>(cur) == 1u);
            }
        });

    // Android: RT64's "idle work", a tiny GPU job every millisecond between frames that stops
    // the GPU clocking right down. The performance hints now do that job, so it's off by default
    // (less heat, so less throttling later on); kept as an option to compare.
    static const std::string keep_gpu_awake_option = "android_keep_gpu_awake";
    static EnumOptionVector keep_gpu_awake_options = {
        {0u, "Off", "Off"},
        {1u, "On", "On"},
    };
    config.add_enum_option(
        keep_gpu_awake_option,
        "Keep GPU Awake",
        "Sends the GPU a tiny job every millisecond between frames so it doesn't slow right down. "
        "Off runs cooler; turn it on if the frame rate dips when little is happening on screen.",
        keep_gpu_awake_options,
        0u
    );
    config.add_option_change_callback(keep_gpu_awake_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            if (context != recomp::config::OptionChangeContext::Temporary) {
                dk64_android_set_gpu_keep_awake(std::get<uint32_t>(cur) == 1u);
            }
        });
#endif
}

dk64::CutsceneBordersMode dk64::get_cutscene_borders() {
    return get_graphics_config_enum_value<dk64::CutsceneBordersMode>(dk64::configkeys::graphics::cutscene_borders);
}

int dk64::get_draw_distance() {
#if defined(__ANDROID__)
    return dk64::android::draw_distance(get_graphics_config_number_value<int>(dk64::configkeys::graphics::draw_distance));
#endif
    return get_graphics_config_number_value<int>(dk64::configkeys::graphics::draw_distance);
}

static void add_technical_options(recomp::config::Config &config) {
    // Android: no Share Log / Show Area here (the crash prompt still offers the log), and
    // this tab is "Extras", next to the mod tabs.
    using EnumOptionVector = const std::vector<recomp::config::ConfigOptionEnumOption>;
    // Multiplayer
    static EnumOptionVector multiplayer_options = {
        {dk64::MultiplayerEnabled::Off, "Off", "Off"},
        {dk64::MultiplayerEnabled::On, "On", "On"}
    };
    config.add_enum_option(
        dk64::configkeys::technical::multiplayer_enabled,
        "Enable Multiplayer",
        "Enables the ability to enter Multiplayer. Multiplayer is considered a <recomp-color secondary>Work-in-Progress feature</recomp-color> that is classed as non-functional.<br />You should not enable this feature unless you wish to use glitches such as \"Funky Weapons Glitch\" or \"Main Menu Moves\"",
        multiplayer_options,
        dk64::MultiplayerEnabled::Off
    );
}

dk64::MultiplayerEnabled dk64::get_multiplayer_enabled() {
    return get_technical_config_enum_value<dk64::MultiplayerEnabled>(dk64::configkeys::technical::multiplayer_enabled);
}

static void set_control_defaults() {
    using namespace recompinput;

    // Left shoulder -> C Down | Backwards eggs / zoom out
    set_default_mapping_for_controller(
        GameInput::C_DOWN,
        { 
            InputField::controller_analog(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY, true),
            InputField::controller_digital(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
        }
    );

    // Right shoulder -> C Up | Forwards eggs / first person
    set_default_mapping_for_controller(
        GameInput::C_UP,
        { 
            InputField::controller_analog(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY, false),
            InputField::controller_digital(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
        }
    );

    // North button -> C Left | Talon trot / camera left
    set_default_mapping_for_controller(
        GameInput::C_LEFT,
        { 
            InputField::controller_analog(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX, false),
            InputField::controller_digital(SDL_CONTROLLER_BUTTON_NORTH)
        }
    );

    // East button -> C Right | Wonderwing / camera right
    set_default_mapping_for_controller(
        GameInput::C_RIGHT,
        { 
            InputField::controller_analog(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX, true),
            InputField::controller_digital(SDL_CONTROLLER_BUTTON_EAST)
        }
    );

    // R3 -> L | Unused in BK but can be used in mods
    set_default_mapping_for_controller(GameInput::L, { InputField::controller_digital(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSTICK) });

#if defined(__ANDROID__)
    // Android (the Classic and Modern presets start from these; Modern then frees the right
    // stick for the camera). The N64 buttons go to the buttons with the same letters, so the
    // game's button prompts (see Controller Button Icons) read naturally:
    //   A -> A (jump), B -> B (attack), Z -> left trigger (crouch / items with C),
    //   The shoulders are the N64's shoulders: L -> left shoulder (unused, for mods),
    //   R -> right shoulder and right trigger (centre camera), Start -> Start,
    //   C Up    -> right stick up    + X  (first person; Z + C Up: instrument)
    //   C Left  -> right stick left  + Y  (Z + C Left: fruit weapon)
    //   C Down  -> right stick down  (zoom; Z + C Down: fairy camera)
    //   C Right -> right stick right (Z + C Right: throw an orange)
    //   D-Pad   -> D-Pad (Tag Anywhere and other mods use it).
    // Modern takes C Up off the right stick (the stick turns the camera); X is C Up then.
    set_default_mapping_for_controller(GameInput::A, { InputField::controller_digital(SDL_CONTROLLER_BUTTON_A) });
    set_default_mapping_for_controller(GameInput::B, { InputField::controller_digital(SDL_CONTROLLER_BUTTON_B) });
    set_default_mapping_for_controller(GameInput::Z, { InputField::controller_analog(SDL_CONTROLLER_AXIS_TRIGGERLEFT, true) });
    set_default_mapping_for_controller(GameInput::R, {
        InputField::controller_digital(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER),
        InputField::controller_analog(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, true) });
    set_default_mapping_for_controller(GameInput::L, { InputField::controller_digital(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) });
    set_default_mapping_for_controller(GameInput::START, { InputField::controller_digital(SDL_CONTROLLER_BUTTON_START) });
    set_default_mapping_for_controller(GameInput::C_UP, {
        InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTY, false),
        InputField::controller_digital(SDL_CONTROLLER_BUTTON_X) });
    set_default_mapping_for_controller(GameInput::C_LEFT, {
        InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTX, false),
        InputField::controller_digital(SDL_CONTROLLER_BUTTON_Y) });
    set_default_mapping_for_controller(GameInput::C_DOWN, { InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTY, true) });
    set_default_mapping_for_controller(GameInput::C_RIGHT, { InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTX, true) });
    set_default_mapping_for_controller(GameInput::DPAD_UP, { InputField::controller_digital(SDL_CONTROLLER_BUTTON_DPAD_UP) });
    set_default_mapping_for_controller(GameInput::DPAD_DOWN, { InputField::controller_digital(SDL_CONTROLLER_BUTTON_DPAD_DOWN) });
    set_default_mapping_for_controller(GameInput::DPAD_LEFT, { InputField::controller_digital(SDL_CONTROLLER_BUTTON_DPAD_LEFT) });
    set_default_mapping_for_controller(GameInput::DPAD_RIGHT, { InputField::controller_digital(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) });
    // Menus: A selects, B backs out (the library's default for Back is the left face button).
    set_default_mapping_for_controller(GameInput::ACCEPT_MENU, { InputField::controller_digital(SDL_CONTROLLER_BUTTON_A) });
    set_default_mapping_for_controller(GameInput::BACK_MENU, { InputField::controller_digital(SDL_CONTROLLER_BUTTON_B) });
#endif
}

static void set_control_descriptions() {
    recompinput::set_game_input_description(recompinput::GameInput::INSTRUMENT, "Plays your Kong's instrument in one press (the same as Z + C Up).");
    recompinput::set_game_input_description(recompinput::GameInput::Z_INSTRUMENT, "While holding Z, plays your Kong's instrument (the N64's Z + C Up). On its own this button does whatever else it's set to (the right stick: the camera).");
    recompinput::set_game_input_description(recompinput::GameInput::Z_FAIRY_CAMERA, "While holding Z, takes out the fairy camera (the N64's Z + C Down).");
    recompinput::set_game_input_description(recompinput::GameInput::Z_GUN, "While holding Z, takes out (or puts away) your Kong's gun (the N64's Z + C Left).");
    recompinput::set_game_input_description(recompinput::GameInput::Z_ORANGE, "While holding Z, throws an orange (the N64's Z + C Right).");
    recompinput::set_game_input_description(recompinput::GameInput::GUN, "Takes out your Kong's gun in one press (the same as Z + C Left), or puts it away.");
    recompinput::set_game_input_description(recompinput::GameInput::ORANGE, "Throws an orange in one press (the same as Z + C Right).");
    recompinput::set_game_input_description(recompinput::GameInput::FAIRY_CAMERA, "Takes out the fairy camera in one press (the same as Z + C Down). Press it again, or B, to put it away.");
    recompinput::set_game_input_description(recompinput::GameInput::Y_AXIS_POS, "Used to move and for steering while swimming. Axis inversion for swimming can be configured in the General tab.");
    recompinput::set_game_input_description(recompinput::GameInput::Y_AXIS_NEG, "Used to move and for steering while swimming. Axis inversion for swimming can be configured in the General tab.");
    recompinput::set_game_input_description(recompinput::GameInput::X_AXIS_NEG, "Used to move and for steering while swimming. Axis inversion for swimming can be configured in the General tab.");
    recompinput::set_game_input_description(recompinput::GameInput::X_AXIS_POS, "Used to move and for steering while swimming. Axis inversion for swimming can be configured in the General tab.");
    recompinput::set_game_input_description(recompinput::GameInput::A, "Used to jump and select options in menus.");
    recompinput::set_game_input_description(recompinput::GameInput::B, "Used for attacks, which change depending on whether you are stationary, moving, in the air, or crouching.");
    recompinput::set_game_input_description(recompinput::GameInput::Z, "Used to crouch, which enables A, B and the C-Buttons to perform different actions.");
    recompinput::set_game_input_description(recompinput::GameInput::L, "Unused. Mods may use it for additional features.");
    recompinput::set_game_input_description(recompinput::GameInput::R, "Used to center the camera behind the player, and to perform tighter turns while in the coconut boat.");
    recompinput::set_game_input_description(recompinput::GameInput::START, "Used for pausing and for skipping certain cutscenes.");
    recompinput::set_game_input_description(recompinput::GameInput::C_UP, "Used to enter first-person mode and to play your instrument while holding Z");
    recompinput::set_game_input_description(recompinput::GameInput::C_DOWN, "Used to toggle between the different camera zoom levels, and to pull out the fairy camera while holding Z.");
    recompinput::set_game_input_description(recompinput::GameInput::C_LEFT, "Used to rotate the camera sideways. Axis inversion can be configured in the General tab. Also used to pull out your fruit weapon while holding Z.");
    recompinput::set_game_input_description(recompinput::GameInput::C_RIGHT, "Used to rotate the camera sideways. Axis inversion can be configured in the General tab). Also used to throw an orange while holding Z.");
    recompinput::set_game_input_description(recompinput::GameInput::DPAD_UP, "Used to move in Jetpac and Donkey Kong arcade games. Mods may also use it separately for additional features.");
    recompinput::set_game_input_description(recompinput::GameInput::DPAD_DOWN, "Used to move in Jetpac and Donkey Kong arcade games. Mods may also use it separately for additional features.");
    recompinput::set_game_input_description(recompinput::GameInput::DPAD_LEFT, "Used to move in Jetpac and Donkey Kong arcade games. Mods may also use it separately for additional features.");
    recompinput::set_game_input_description(recompinput::GameInput::DPAD_RIGHT, "Used to move in Jetpac and Donkey Kong arcade games. Mods may also use it separately for additional features.");
}

void dk64::init_config() {
    std::filesystem::path recomp_dir = recompui::file::get_app_folder_path();

    if (!recomp_dir.empty()) {
        std::filesystem::create_directories(recomp_dir);
    }

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = true;
    general_options.has_mouse_sensitivity = true;

#if defined(__ANDROID__)
    // Android: four tabs - Video, Input, Audio, Gameplay. The General and Dual Screen configs
    // still exist (and keep their files) but their options are shown on the Input and
    // Gameplay tabs instead; the controller setup opens from Input > Configure Inputs.
    // Mouse Sensitivity stays (input code reads it every frame), it just isn't listed.
    auto &general_config = recompui::config::create_general_tab(general_options);
    add_general_options(general_config);
    recompui::config::set_tab_visible(recompui::config::general::id, false);

    auto &graphics_config = recompui::config::create_graphics_tab("Video");
    add_graphics_options(graphics_config);

    {
        using Entry = recompui::config::LayoutEntry;
        using Kind = Entry::Kind;
        const std::string g = recompui::config::general::id;
        const std::string d = "dual_screen";
        namespace keys = dk64::configkeys::general;
        namespace gopts = recompui::config::general::options;
        auto header = [](const std::string &text) { Entry e; e.kind = Kind::Header; e.text = text; return e; };
        auto option = [](const std::string &cfg, const std::string &id) { Entry e; e.kind = Kind::Option; e.config_id = cfg; e.option_id = id; return e; };
        Entry configure; configure.kind = Kind::Button; configure.text = "Configure Inputs";
        configure.description = "Choose which button does what on your controller, or pick the Classic or Modern layout.";
        configure.on_press = []() { recompui::config::set_tab(recompui::config::controls::id); };

        Entry edit_touch; edit_touch.kind = Kind::Button; edit_touch.text = "Edit Touch Layout";
        edit_touch.description = "Move and resize the on-screen buttons. Drag a button to move it, pinch or use - and + to resize it. "
            "With the Analog Camera selected, this edits the layout with the second stick.";
        edit_touch.on_press = []() { dk64_android_edit_touch_layout(); };

        recompui::config::create_layout_tab("Input", "input", {
            header("Inputs"),
            configure,
            option(g, gopts::joystick_deadzone),
            option(g, gopts::rumble_strength),
            option(d, "android_button_icons"),
            header("Camera"),
            option(g, keys::camera_type),
            option(g, keys::analog_camera_sensitivity),
            option(g, keys::third_person_camera_invert_mode),
            header("First Person"),
            option(d, "android_first_person_mode"),
            option(d, "android_gun_button"),
            option(g, keys::first_person_invert_mode),
            header("Gyro"),
            option(g, gopts::gyro_sensitivity),
            option(g, keys::gyro_invert_mode),
            header("Swimming"),
            option(g, keys::swimming_invert_mode),
        }, { g, d });

        recompui::config::create_layout_tab("Touch", "touch", {
            option(d, "android_touch_controls"),
            edit_touch,
            option(d, "android_drag_camera"),
            option(d, "android_touch_feedback_level"),
            header("Bottom Screen"),
            option(d, "android_bottom_layout"),
            option(d, "android_touch_kong_swap"),
        }, { g, d });

        auto &sound_config = recompui::config::create_sound_tab("Audio");
        add_sound_options(sound_config);

        // Where saves, settings, mods and the ROM are kept: app storage or a shared folder.
        // Moving them restarts the app, so (like the graphics driver) only from the main menu.
        Entry files; files.kind = Kind::Button; files.text = "Game Files Location";
        {
            const std::string where = dk64::android::storage_description();
            files.description = "Move your saves, settings, mods and ROM between app storage and a shared folder "
                "(easy to reach from a PC, and kept if the app is uninstalled). Everything is copied, then the app restarts; "
                "only from the main menu."
                + (where.empty() ? std::string() : "<br />Now in " + where + ".");
        }
        files.on_press = []() {
            if (ultramodern::is_game_started()) {
                recompui::open_info_prompt("Game Files Location",
                    "The game files can only be moved from the main menu, because the app restarts to use the new place. "
                    "Use QUIT > Main Menu, then open Settings > Gameplay > Game Files Location.",
                    "OK", []() {});
                return;
            }
            dk64::android::change_storage_location();
        };

        // Experience: a button rather than a list, so changing it is a deliberate choice:
        // pick Modern or Classic, then confirm, and the app restarts with it applied.
        Entry experience; experience.kind = Kind::Button; experience.text = "Choose Experience";
        experience.description = "Set up the whole game at once, the modern way or as it was on the N64. "
            "You pick one, confirm, and the app restarts with it applied. Any single option can still be changed afterwards.";
        experience.on_press = []() { dk64_android_choose_experience(); };

        recompui::config::create_layout_tab("Gameplay", "gameplay", {
            experience,
            header("Game Feel"),
            option(d, "android_movement"),
            option(d, "android_jump_help"),
            option(d, "android_feel_rumbles"),
            option(d, "android_fast_transitions"),
            header("Story"),
            option(g, keys::story_skip),
            option(g, keys::skip_launcher),
            option(g, keys::lightning_flashes),
            header("Storage"),
            files,
        }, { g, d });
    }

    set_control_defaults();
    set_control_descriptions();
    recompui::config::create_controls_tab("Configure Inputs");

    auto &dual_screen_config = recompui::config::create_config_tab("Dual Screen", "dual_screen", false);
    add_dual_screen_options(dual_screen_config);
    recompui::config::set_tab_visible("dual_screen", false);

    recompui::config::create_mods_tab();
    auto &technical_config = recompui::config::create_config_tab("Extras", "technical", false);
    add_technical_options(technical_config);
#else
    auto &general_config = recompui::config::create_general_tab(general_options);
    add_general_options(general_config);

    auto &graphics_config = recompui::config::create_graphics_tab();
    add_graphics_options(graphics_config);

    set_control_defaults();
    set_control_descriptions();
    recompui::config::create_controls_tab();

    auto &sound_config = recompui::config::create_sound_tab();
    add_sound_options(sound_config);

    auto &technical_config = recompui::config::create_config_tab("Technical", "technical", true);
    add_technical_options(technical_config);

    recompui::config::create_mods_tab();
#endif

    recompui::config::finalize();

#if defined(__ANDROID__)
    dk64_sync_combined_options();   // the combined options from what was loaded
    // Once: set-ups from before B backed out of menus get Back (Menu) on B.
    {
        const std::filesystem::path marker = recomp::get_config_path() / "menu_back_b.txt";
        if (!std::filesystem::exists(marker)) {
            using namespace recompinput;
            const int profile = profiles::get_sp_controller_profile_index();
            const InputField b = InputField::controller_digital(SDL_CONTROLLER_BUTTON_B);
            bool has_b = false;
            for (size_t i = 0; profile >= 0 && i < num_bindings_per_input; i++) {
                has_b = has_b || profiles::get_input_binding(profile, GameInput::BACK_MENU, i) == b;
            }
            if (profile >= 0 && !has_b) {
                profiles::set_input_binding(profile, GameInput::BACK_MENU, 0, b);
                profiles::save_controls_config(recomp::get_config_path() / (recompui::config::controls::id + ".json"));
            }
            std::ofstream out{ marker };
            out << "done\n";
        }
    }
#endif

}

#ifdef __ANDROID__
#include "recompinput/profiles.h"
#include "librecomp/game.hpp"
#include "librecomp/mods.hpp"

// ---- Control presets (Android) -----------------------------------------------------------
//
// Classic: the N64 way. The right stick works the C buttons, the Free camera, no gyro.
// Modern:  the right stick turns the camera (Analog Camera); it still presses C Down/Left/Right,
//          C Up moves to X only. Gyro aiming on, first person and gyro not inverted.
// Chosen on first launch (main.cpp) and from the Controls page's footer (RecompFrontend patch).

static std::filesystem::path control_preset_marker() {
    return recomp::get_config_path() / "controls_preset.txt";
}

// (Read once, then kept: the game asks every frame.)
static std::atomic<int> control_preset_cached{-2};
extern "C" int dk64_android_control_preset() {
    int cached = control_preset_cached.load(std::memory_order_relaxed);
    if (cached != -2) {
        return cached;
    }
    std::ifstream in{ control_preset_marker() };
    std::string value;
    cached = !(in >> value) ? -1 : (value == "modern" ? 1 : 0);   // -1: not chosen yet
    control_preset_cached.store(cached, std::memory_order_relaxed);
    return cached;
}

// ---- Controller bindings log (for choosing the shipped defaults) ---------------------------
// Logs the controller profile's bindings (the session log, lines starting DK64Controls) as C++ ready to paste into
// dk64_android_apply_control_preset: at startup, whenever they change (checked every ~2 s), and
// just before switching layout (so the layout being left is logged too).
#include <android/log.h>
#include <mutex>
static const char *dk64_sdl_button_name(int id) {
    switch (id) {
        case SDL_CONTROLLER_BUTTON_A: return "SDL_CONTROLLER_BUTTON_A";
        case SDL_CONTROLLER_BUTTON_B: return "SDL_CONTROLLER_BUTTON_B";
        case SDL_CONTROLLER_BUTTON_X: return "SDL_CONTROLLER_BUTTON_X";
        case SDL_CONTROLLER_BUTTON_Y: return "SDL_CONTROLLER_BUTTON_Y";
        case SDL_CONTROLLER_BUTTON_BACK: return "SDL_CONTROLLER_BUTTON_BACK";
        case SDL_CONTROLLER_BUTTON_GUIDE: return "SDL_CONTROLLER_BUTTON_GUIDE";
        case SDL_CONTROLLER_BUTTON_START: return "SDL_CONTROLLER_BUTTON_START";
        case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "SDL_CONTROLLER_BUTTON_LEFTSTICK";
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "SDL_CONTROLLER_BUTTON_RIGHTSTICK";
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "SDL_CONTROLLER_BUTTON_LEFTSHOULDER";
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "SDL_CONTROLLER_BUTTON_RIGHTSHOULDER";
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return "SDL_CONTROLLER_BUTTON_DPAD_UP";
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "SDL_CONTROLLER_BUTTON_DPAD_DOWN";
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "SDL_CONTROLLER_BUTTON_DPAD_LEFT";
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "SDL_CONTROLLER_BUTTON_DPAD_RIGHT";
        default: return nullptr;
    }
}
static const char *dk64_sdl_axis_name(int axis) {
    switch (axis) {
        case SDL_CONTROLLER_AXIS_LEFTX: return "SDL_CONTROLLER_AXIS_LEFTX";
        case SDL_CONTROLLER_AXIS_LEFTY: return "SDL_CONTROLLER_AXIS_LEFTY";
        case SDL_CONTROLLER_AXIS_RIGHTX: return "SDL_CONTROLLER_AXIS_RIGHTX";
        case SDL_CONTROLLER_AXIS_RIGHTY: return "SDL_CONTROLLER_AXIS_RIGHTY";
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return "SDL_CONTROLLER_AXIS_TRIGGERLEFT";
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return "SDL_CONTROLLER_AXIS_TRIGGERRIGHT";
        default: return nullptr;
    }
}
static std::string dk64_field_code(const recompinput::InputField &f) {
    using recompinput::InputType;
    switch (f.input_type) {
        case InputType::None: return "InputField{}";
        case InputType::ControllerDigital: {
            const char *n = dk64_sdl_button_name(f.input_id);
            return n ? std::string("InputField::controller_digital(") + n + ")"
                     : "InputField{ InputType::ControllerDigital, " + std::to_string(f.input_id) + " }";
        }
        case InputType::ControllerAnalog: {
            const char *n = dk64_sdl_axis_name(std::abs(f.input_id) - 1);
            return n ? std::string("InputField::controller_analog(") + n + (f.input_id > 0 ? ", true)" : ", false)")
                     : "InputField{ InputType::ControllerAnalog, " + std::to_string(f.input_id) + " }";
        }
        default:
            return "InputField{ InputType(" + std::to_string(static_cast<int>(f.input_type)) + "), " + std::to_string(f.input_id) + " }";
    }
}
static std::string dk64_bindings_dump(int profile) {
    using namespace recompinput;
    std::string out;
    for (int i = 0; i < static_cast<int>(GameInput::COUNT); i++) {
        const GameInput input = static_cast<GameInput>(i);
        if (get_game_input_is_menu(input)) {
            continue;
        }
        out += "{ GameInput::" + get_game_input_enum_name(input) + ", {";
        for (size_t b = 0; b < num_bindings_per_input; b++) {
            out += (b ? ", " : " ") + dk64_field_code(profiles::get_input_binding(profile, input, b));
        }
        out += " } },\n";
    }
    return out;
}
extern "C" void dk64_android_log_bindings(const char *why, int force) {
    static std::mutex m;
    static std::string last;
    const int profile = recompinput::profiles::get_sp_controller_profile_index();
    if (profile < 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m);
    std::string dump = dk64_bindings_dump(profile);
    if (!force && dump == last) {
        return;
    }
    last = dump;
    const int preset = dk64_android_control_preset();
    printf("DK64Controls ==== %s layout bindings (%s) ====\n",
        preset == 1 ? "MODERN" : preset == 0 ? "CLASSIC" : "UNCHOSEN", why);
    size_t start = 0;
    while (start < dump.size()) {
        size_t end = dump.find('\n', start);
        if (end == std::string::npos) end = dump.size();
        printf("DK64Controls %s\n", dump.substr(start, end - start).c_str());
        start = end + 1;
    }
    printf("DK64Controls ==== end ====\n");
    fflush(stdout);
}

// Configure Inputs with the Modern layout: each row named by what it does in the game (the
// Classic layout shows the N64 buttons' own names). Null: keep the usual name.
extern "C" const char *dk64_android_control_action_name(int input) {
    using GI = recompinput::GameInput;
    switch (static_cast<GI>(input)) {
        case GI::A: return "Jump";
        case GI::B: return "Attack";
        case GI::Z: return "Crouch (Z)";
        case GI::L: return "Camera Behind";
        case GI::R: return "Shoot / Take Photo";
        case GI::START: return "Pause";
        case GI::C_UP: return "First Person";
        case GI::C_DOWN: return "Camera Zoom";
        case GI::INSTRUMENT: return "Instrument (one press)";
        case GI::FAIRY_CAMERA: return "Fairy Camera (one press)";
        case GI::Z_INSTRUMENT: return "Instrument";
        case GI::Z_FAIRY_CAMERA: return "Fairy Camera";
        case GI::Z_GUN: return "Gun";
        case GI::Z_ORANGE: return "Orange";
        case GI::C_LEFT: return "C Left";
        case GI::C_RIGHT: return "C Right";
        case GI::GUN: return "Gun (one press)";
        case GI::ORANGE: return "Orange (one press)";
        case GI::DPAD_UP: return "Arcade Up";
        case GI::DPAD_DOWN: return "Arcade Down";
        case GI::DPAD_LEFT: return "Previous Kong";
        case GI::DPAD_RIGHT: return "Next Kong";
        case GI::Y_AXIS_POS: return "Move Forward";
        case GI::Y_AXIS_NEG: return "Move Back";
        case GI::X_AXIS_NEG: return "Move Left";
        case GI::X_AXIS_POS: return "Move Right";
        default: return nullptr;
    }
}

// Each layout keeps your own bindings: switching saves the one you're leaving (controls_<layout>.txt
// in the config folder) and brings back the other's, if you've used it before.
static std::filesystem::path dk64_layout_bindings_path(int modern) {
    return recomp::get_config_path() / (modern ? "controls_modern.txt" : "controls_classic.txt");
}
static void dk64_save_layout_bindings(int profile, int modern) {
    using namespace recompinput;
    std::ofstream out{ dk64_layout_bindings_path(modern) };
    for (int i = 0; i < static_cast<int>(GameInput::COUNT); i++) {
        const GameInput input = static_cast<GameInput>(i);
        if (get_game_input_is_menu(input)) {
            continue;
        }
        out << get_game_input_enum_name(input);
        for (size_t b = 0; b < num_bindings_per_input; b++) {
            const InputField f = profiles::get_input_binding(profile, input, b);
            out << ' ' << static_cast<int>(f.input_type) << ' ' << f.input_id;
        }
        out << '\n';
    }
}
static bool dk64_load_layout_bindings(int profile, int modern) {
    using namespace recompinput;
    std::ifstream in{ dk64_layout_bindings_path(modern) };
    if (!in) {
        return false;
    }
    std::string name;
    bool any = false;
    while (in >> name) {
        int found = -1;
        for (int i = 0; i < static_cast<int>(GameInput::COUNT); i++) {
            if (get_game_input_enum_name(static_cast<GameInput>(i)) == name) {
                found = i;
                break;
            }
        }
        for (size_t b = 0; b < num_bindings_per_input; b++) {
            int type = 0, id = 0;
            in >> type >> id;
            if (found >= 0 && type >= 0 && type <= static_cast<int>(InputType::ControllerAnalog)) {
                profiles::set_input_binding(profile, static_cast<GameInput>(found), b, InputField{ static_cast<InputType>(type), id });
                any = true;
            }
        }
    }
    return any;
}

extern "C" void dk64_android_apply_control_preset(int modern) {
    using namespace recompinput;
    const int profile = profiles::get_sp_controller_profile_index();
    if (profile >= 0) {
        dk64_android_log_bindings("before switching layout", 1);
        const int leaving = dk64_android_control_preset();
        if (leaving == 0 || leaving == 1) {
            dk64_save_layout_bindings(profile, leaving);
        }
        profiles::reset_profile_bindings(profile, InputDevice::Controller);
        if (modern) {
            // The right stick is only the camera: the C buttons come off it. C Up (first person)
            // is pressing it in; C Down, Left and Right aren't used - with Z held, the stick's
            // four directions are the Z moves instead (Z_INSTRUMENT... below).
            for (GameInput input : { GameInput::C_UP, GameInput::C_DOWN, GameInput::C_LEFT, GameInput::C_RIGHT }) {
                for (size_t b = 0; b < num_bindings_per_input; b++) {
                    if (profiles::get_input_binding(profile, input, b).input_type == InputType::ControllerAnalog) {
                        profiles::set_input_binding(profile, input, b, input == GameInput::C_UP
                            ? InputField::controller_digital(SDL_CONTROLLER_BUTTON_RIGHTSTICK) : InputField{});
                    }
                }
            }
        }
        if (modern) {
            // Modern leaves X and Y free: the C buttons are on the right stick (C Up: pressing
            // it in).
            const InputField x = InputField::controller_digital(SDL_CONTROLLER_BUTTON_X);
            const InputField y = InputField::controller_digital(SDL_CONTROLLER_BUTTON_Y);
            for (int i = 0; i < static_cast<int>(GameInput::COUNT); i++) {
                const GameInput input = static_cast<GameInput>(i);
                if (get_game_input_is_menu(input)) {
                    continue;
                }
                for (size_t b = 0; b < num_bindings_per_input; b++) {
                    const InputField field = profiles::get_input_binding(profile, input, b);
                    if (field == x || field == y) {
                        profiles::set_input_binding(profile, input, b, InputField{});
                    }
                }
            }
            // ...which the gun (X) and oranges (Y) take: one press each instead of Z + C Left /
            // Z + C Right. (Instrument and Fairy Camera start unassigned.)
            for (size_t b = 0; b < num_bindings_per_input; b++) {
                for (GameInput shortcut : { GameInput::INSTRUMENT, GameInput::FAIRY_CAMERA, GameInput::GUN, GameInput::ORANGE,
                                            GameInput::Z_INSTRUMENT, GameInput::Z_FAIRY_CAMERA, GameInput::Z_GUN, GameInput::Z_ORANGE }) {
                    profiles::set_input_binding(profile, shortcut, b, InputField{});
                }
            }
            profiles::set_input_binding(profile, GameInput::GUN, 0, x);
            profiles::set_input_binding(profile, GameInput::ORANGE, 0, y);
            // Z moves: hold Z and push the right stick (up: instrument, down: fairy camera,
            // left: gun, right: orange), as the N64's Z + C buttons were.
            profiles::set_input_binding(profile, GameInput::Z_INSTRUMENT, 0, InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTY, false));
            profiles::set_input_binding(profile, GameInput::Z_FAIRY_CAMERA, 0, InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTY, true));
            profiles::set_input_binding(profile, GameInput::Z_GUN, 0, InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTX, false));
            profiles::set_input_binding(profile, GameInput::Z_ORANGE, 0, InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTX, true));
        } else {
            for (size_t b = 0; b < num_bindings_per_input; b++) {
                for (GameInput shortcut : { GameInput::INSTRUMENT, GameInput::FAIRY_CAMERA, GameInput::GUN, GameInput::ORANGE,
                                            GameInput::Z_INSTRUMENT, GameInput::Z_FAIRY_CAMERA, GameInput::Z_GUN, GameInput::Z_ORANGE }) {
                    profiles::set_input_binding(profile, shortcut, b, InputField{});
                }
            }
        }
        // Your own bindings for this layout, if you've set it up before (on top of the defaults).
        dk64_load_layout_bindings(profile, modern ? 1 : 0);
        profiles::save_controls_config(recomp::get_config_path() / (recompui::config::controls::id + ".json"));
    }

    recomp::config::Config &general = recompui::config::get_general_config();
    general.update_option_value(dk64::configkeys::general::camera_type,
        static_cast<uint32_t>(modern ? dk64::CameraTypeMode::Analog : dk64::CameraTypeMode::Free));
    // Analog Camera speed: the turn rate scales with this (patches/sound_options_patches.c).
    // Modern: 4 (tried on the Thor - a bit quicker than the default 3 felt right).
    general.update_option_value(dk64::configkeys::general::analog_camera_sensitivity, modern ? 4.0 : 3.0);
    // Aiming: Modern looks up when you push (or tilt) up, like most games now; Classic keeps
    // the N64's (up looks down). Swimming follows the same rule (Modern: up swims up). The
    // third person camera isn't inverted in either.
    general.update_option_value(dk64::configkeys::general::third_person_camera_invert_mode,
        static_cast<uint32_t>(dk64::CameraInvertMode::InvertNone));
    general.update_option_value(dk64::configkeys::general::first_person_invert_mode,
        static_cast<uint32_t>(modern ? dk64::CameraInvertMode::InvertNone : dk64::CameraInvertMode::InvertY));
    // Gyro's left/right comes out reversed without Invert X (turning the device right looked
    // left), so both presets invert X; Classic also keeps the N64's up/down.
    general.update_option_value(dk64::configkeys::general::gyro_invert_mode,
        static_cast<uint32_t>(modern ? dk64::CameraInvertMode::InvertX : dk64::CameraInvertMode::InvertBoth));
    general.update_option_value(dk64::configkeys::general::swimming_invert_mode,
        static_cast<uint32_t>(modern ? dk64::CameraInvertMode::InvertNone : dk64::CameraInvertMode::InvertY));
    if (general.has_option(recompui::config::general::options::gyro_sensitivity)) {
        general.update_option_value(recompui::config::general::options::gyro_sensitivity, modern ? 25.0 : 0.0);
    }
    general.save_config();

    control_preset_cached.store(modern ? 1 : 0, std::memory_order_relaxed);
    std::ofstream out{ control_preset_marker() };
    out << (modern ? "modern" : "classic") << "\n";
}
#endif

#ifdef __ANDROID__
// ---- Experience (Android) -------------------------------------------------------------------
//
// One choice that sets several things at once (General tab, top; also asked on first launch):
//   Classic: the N64 as it was - Classic controls (right stick = C buttons, Free camera, no
//            gyro), the original 240p resolution, no anti-aliasing, no quick Kong swap.
//   Modern:  Modern controls (Analog Camera, gyro aiming), the device's resolution, 2x MSAA,
//            and quick Kong swapping (the Tag Anywhere mod + Touch Kong Swap).
//   Custom:  whatever has been set by hand (nothing is changed).

static const std::string experience_option = "android_experience";
static bool applying_experience = false;

static std::filesystem::path experience_marker() {
    return recomp::get_config_path() / "experience.txt";
}

extern "C" int dk64_android_experience_chosen() {
    return std::filesystem::exists(experience_marker()) ? 1 : 0;
}

extern "C" void dk64_android_apply_experience(int modern) {
    if (applying_experience) {
        return;
    }
    applying_experience = true;

    dk64_android_apply_control_preset(modern);

    recomp::config::Config &graphics = recompui::config::get_graphics_config();
    graphics.update_option_value(recompui::config::graphics::options::res_option,
        static_cast<uint32_t>(modern ? ultramodern::renderer::Resolution::Auto : ultramodern::renderer::Resolution::Original));
    graphics.update_option_value(recompui::config::graphics::options::msaa_option,
        static_cast<uint32_t>(modern ? ultramodern::renderer::Antialiasing::MSAA2X : ultramodern::renderer::Antialiasing::None));
    // Classic keeps the N64's 4:3 picture and HUD; Modern fills a widescreen.
    graphics.update_option_value(recompui::config::graphics::options::ar_option,
        static_cast<uint32_t>(modern ? ultramodern::renderer::AspectRatio::Expand : ultramodern::renderer::AspectRatio::Original));
    graphics.update_option_value(recompui::config::graphics::options::hr_option,
        static_cast<uint32_t>(modern ? ultramodern::renderer::HUDRatioMode::Clamp16x9 : ultramodern::renderer::HUDRatioMode::Original));
    // Classic: the cutscene borders and the original draw distance; Modern: no borders, and
    // things stay drawn a bit further away.
    graphics.update_option_value(dk64::configkeys::graphics::cutscene_borders,
        static_cast<uint32_t>(modern ? dk64::CutsceneBordersMode::Off : dk64::CutsceneBordersMode::On));
    graphics.update_option_value(dk64::configkeys::graphics::draw_distance, modern ? 30.0 : 0.0);
    graphics.save_config();

    // Camera: Modern Camera, or the original's own (Free Cam); lightning flashes as in the
    // original, or toned down.
    {
        recomp::config::Config &gen = recompui::config::get_general_config();
        gen.update_option_value(dk64::configkeys::general::camera_type,
            static_cast<uint32_t>(modern ? dk64::CameraTypeMode::Modern : dk64::CameraTypeMode::Free));
        gen.update_option_value(dk64::configkeys::general::lightning_flashes,
            static_cast<uint32_t>(modern ? dk64::LightningFlashMode::Reduced : dk64::LightningFlashMode::Vanilla));
        gen.save_config();
    }

    recomp::mods::enable_mod("dk64_tag_anywhere", modern != 0);
    recomp::config::Config &dual = recompui::config::get_config("dual_screen");
    dual.update_option_value("android_touch_kong_swap", static_cast<uint32_t>(modern ? 1u : 0u));
    dual.update_option_value("android_button_icons", static_cast<uint32_t>(modern ? 1u : 0u));
    dual.update_option_value("android_haptic_feedback", static_cast<uint32_t>(modern ? 1u : 0u));
    dual.update_option_value("android_jump_assist", static_cast<uint32_t>(modern ? 1u : 0u));
    dual.update_option_value("android_gun_button", static_cast<uint32_t>(modern ? 1u : 0u));   // ZR shoots, B puts it away
    dual.update_option_value("android_quick_turn", static_cast<uint32_t>(modern ? 1u : 0u));
    dual.update_option_value("android_smooth_running", static_cast<uint32_t>(modern ? 1u : 0u));
    dual.update_option_value("android_movement_polish", static_cast<uint32_t>(modern ? 1u : 0u));
    for (const char *id : { "android_move_chaining", "android_air_control",
                            "android_hit_feel", "android_fast_transitions",
                            "android_fp_right_stick", "android_fp_walk",
                            "android_fp_polish" }) {
        dual.update_option_value(id, static_cast<uint32_t>(modern ? 1u : 0u));
    }
    dk64_sync_combined_options();
    dual.save_config();

    recomp::config::Config &general = recompui::config::get_general_config();
    general.update_option_value(experience_option, static_cast<uint32_t>(modern ? 1u : 0u));
    general.save_config();

    std::ofstream out{ experience_marker() };
    out << (modern ? "modern" : "classic") << "\n";
    applying_experience = false;
}

extern "C" void dk64_android_restart_to_launcher();

// Settings > Gameplay > Choose Experience: one prompt with both choices and Cancel; picking one
// applies it and restarts the app (to the main menu) so everything starts fresh with it.
// (A second, confirming prompt can't be opened from a prompt's button: the prompt code would
// call back into the first one while holding its lock, and freeze.)
void dk64_android_choose_experience() {
    const std::string unsaved = ultramodern::is_game_started()
        ? " Anything since your last save will be lost." : "";
    recompui::open_choice_prompt(
        "Choose Your Experience",
        "Modern: the Modern Camera, walking in first person, smoother movement, jump help and feel rumbles, "
        "sharp widescreen graphics and quick Kong swapping.\n\n"
        "Classic: as close to the original N64 game as possible.\n\n"
        "The app restarts to apply your choice." + unsaved,
        "Modern",
        "Cancel",
        []() {
            dk64_android_apply_experience(1);
            dk64_android_restart_to_launcher();
        },
        []() {},
        recompui::ButtonStyle::Primary,
        recompui::ButtonStyle::Tertiary,
        true
    );
    recompui::set_prompt_extra_button("Classic", []() {
        dk64_android_apply_experience(0);
        dk64_android_restart_to_launcher();
    }, recompui::ButtonStyle::Secondary);
}

namespace recompui::config {
    void dk64_android_add_experience_option(recomp::config::Config &config) {
        static const std::vector<recomp::config::ConfigOptionEnumOption> experience_options = {
            {0u, "Classic", "Classic"},
            {1u, "Modern", "Modern"},
            {2u, "Custom", "Custom"},
        };
        config.add_enum_option(
            experience_option,
            "Experience",
            "Sets several options at once.<br />"
            "<recomp-color primary>Classic</recomp-color>: the N64 as it was. Classic controls (the right stick works the C buttons), "
            "Free Cam, no gyro, the original resolution, no anti-aliasing and the N64 button icons.<br />"
            "<recomp-color primary>Modern</recomp-color>: Analog Camera on the right stick, gyro aiming, the device's resolution, "
            "anti-aliasing, quick Kong swapping (Tag Anywhere and Touch Kong Swap), button icons that match your controller "
            "and haptic feedback.<br />"
            "<recomp-color primary>Custom</recomp-color>: keeps whatever you've set yourself.<br />"
            "Each option can still be changed afterwards.",
            experience_options,
            2u
        );
        config.add_option_change_callback(experience_option,
            [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
                const uint32_t value = std::get<uint32_t>(cur);
                if (context == recomp::config::OptionChangeContext::Permanent && !applying_experience && value < 2u) {
                    dk64_android_apply_experience(int(value));
                }
            });
    }
}
#endif
