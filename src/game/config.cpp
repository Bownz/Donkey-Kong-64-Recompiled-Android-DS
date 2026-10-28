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
        {dk64::CameraTypeMode::Analog, "Analog", "Analog Camera"}
    };
    config.add_enum_option(
        dk64::configkeys::general::camera_type,
        "Camera Type",
        "Changes the camera behavior.<br /><recomp-color primary>Free Cam</recomp-color>: Camera can be controlled via pressing C-Left and C-Right. The camera does not try to push itself behind the player.<br /><recomp-color primary>Follow Cam</recomp-color>: Similar to <recomp-color secondary>Free Cam</recomp-color>, but the camera tries to push itself behind the player.<br /><recomp-color primary>Better Free Cam</recomp-color>: Similar to <recomp-color secondary>Free Cam</recomp-color>, but instead of a button press turning the camera 45 degrees, holding the button moves the camera at 5 degrees per frame.<br /><recomp-color primary>Analog Camera</recomp-color>: Control of the camera is controlled by your right analogue stick. It is advised that C-Up is remapped to something outside your right analogue stick in order to have a good time with this.",
        camera_type_options,
        dk64::CameraTypeMode::Free
    );
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
// Android: which Vulkan driver the renderer loads (the device's own, or a custom one such
// as Turnip installed through the "DK64 Graphics Driver" screen). The choice is kept by the
// Android side and takes effect the next time the game starts.
static const std::string gpu_driver_option = "android_gpu_driver";

static void add_gpu_driver_option(recomp::config::Config &config) {
    static std::vector<dk64::android::GpuDriverChoice> drivers;
    static std::vector<recomp::config::ConfigOptionEnumOption> options;
    std::string selected;
    drivers = dk64::android::list_gpu_drivers(selected);
    options.clear();
    uint32_t selected_index = 0;
    for (uint32_t i = 0; i < drivers.size(); i++) {
        options.emplace_back(i, drivers[i].id.empty() ? std::string("system") : drivers[i].id, drivers[i].label);
        if (drivers[i].id == selected) {
            selected_index = i;
        }
    }
    const uint32_t install_index = static_cast<uint32_t>(drivers.size());
    options.emplace_back(install_index, "install", "Install or remove drivers...");

    config.add_enum_option(
        gpu_driver_option,
        "GPU Driver",
        "The Vulkan driver used for rendering: the device's own, or a custom one (e.g. Turnip). "
        "Takes effect the next time the game starts. In use now: " + dk64::android::active_gpu_driver() + ". "
        "App version: " + dk64::android::app_version() + ".",
        options,
        selected_index
    );
    config.add_option_change_callback(gpu_driver_option,
        [install_index](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            const uint32_t index = std::get<uint32_t>(cur);
            std::string selected_now;
            dk64::android::list_gpu_drivers(selected_now);
            uint32_t actual = 0;
            for (uint32_t i = 0; i < drivers.size(); i++) {
                if (drivers[i].id == selected_now) {
                    actual = i;
                }
            }
            if (context == recomp::config::OptionChangeContext::Load) {
                // The Android side keeps the choice (it can also change outside the game);
                // show its value without marking the tab as changed.
                if (index != actual) {
                    recompui::config::get_graphics_config().set_option_value(gpu_driver_option, actual);
                    recompui::config::get_graphics_config().apply_option_value(gpu_driver_option);
                    recompui::config::get_graphics_config().modified_options.clear();
                }
                return;
            }
            if (context != recomp::config::OptionChangeContext::Permanent) {
                return;
            }
            if (index == install_index) {
                // Opens the driver manager; the shown value is corrected on the next start.
                dk64::android::open_gpu_driver_manager();
                return;
            }
            if (index < drivers.size()) {
                dk64::android::select_gpu_driver(drivers[index].id);
            }
        });
}
#endif

#if defined(__ANDROID__)
// Android: the "Dual Screen" tab (the bottom screen on handhelds with two screens).
static void add_dual_screen_options(recomp::config::Config &config) {
    using EnumOptionVector = const std::vector<recomp::config::ConfigOptionEnumOption>;
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
        1u
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
        0u
    );
    config.add_option_change_callback(touch_kong_swap_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_touch_kong_swap(std::get<uint32_t>(cur) == 1u);
            if (context == recomp::config::OptionChangeContext::Permanent) {
                recompui::config::get_config("dual_screen").save_config();
            }
        });
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
    // Android: internal render resolution (patch in RecompFrontend's rt64_render_context.cpp).
    static const std::string render_res_option = "android_render_resolution";
    static EnumOptionVector render_res_options = {
        {0u, "Screen", "Screen"},
        {3u, "720p", "720p"},
        {2u, "480p", "480p"},
    };
    config.add_enum_option(
        render_res_option,
        "Render Resolution",
        "The resolution the game is drawn at when Resolution is set to Auto. "
        "<recomp-color primary>Screen</recomp-color> matches the display (960p on a 1080p screen). "
        "<recomp-color primary>720p</recomp-color> draws about half as many pixels, which makes "
        "anti-aliasing much cheaper and still looks sharp on a handheld screen. "
        "Takes effect the next time the game starts.",
        render_res_options,
        0u
    );
    config.add_option_change_callback(render_res_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            if (context != recomp::config::OptionChangeContext::Temporary) {
                dk64::android::set_render_resolution(static_cast<int>(std::get<uint32_t>(cur)));
            }
        });

    config.add_option_change_callback(show_fps_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            if (context != recomp::config::OptionChangeContext::Temporary) {
                dk64::android::set_show_fps(std::get<uint32_t>(cur) == 1u);
            }
        });
#endif
}

dk64::CutsceneBordersMode dk64::get_cutscene_borders() {
    return get_graphics_config_enum_value<dk64::CutsceneBordersMode>(dk64::configkeys::graphics::cutscene_borders);
}

int dk64::get_draw_distance() {
    return get_graphics_config_number_value<int>(dk64::configkeys::graphics::draw_distance);
}

static void add_technical_options(recomp::config::Config &config) {
#if defined(__ANDROID__)
    // Android: share a log report for bug reports (Diagnostics.java), and show the version.
    static const std::string share_log_option = "android_share_log";
    static std::vector<recomp::config::ConfigOptionEnumOption> share_options = {
        {0u, "idle", "-"},
        {1u, "share", "Share now"},
    };
    config.add_enum_option(
        share_log_option,
        "Share Log",
        "Choose Share now and apply to send a log report (for bug reports) through any app: "
        "Files, Drive, Discord, email... App version: " + dk64::android::app_version() + ".",
        share_options,
        0u
    );
    config.add_option_change_callback(share_log_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            const uint32_t value = std::get<uint32_t>(cur);
            if (context == recomp::config::OptionChangeContext::Load) {
                if (value != 0) {
                    recompui::config::get_config("technical").set_option_value(share_log_option, 0u);
                    recompui::config::get_config("technical").apply_option_value(share_log_option);
                    recompui::config::get_config("technical").modified_options.clear();
                }
                return;
            }
            if (context == recomp::config::OptionChangeContext::Permanent && value == 1) {
                dk64::android::share_log();
            }
        });
#endif

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
}

static void set_control_descriptions() {
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

    auto &general_config = recompui::config::create_general_tab(general_options);
    add_general_options(general_config);

    auto &graphics_config = recompui::config::create_graphics_tab();
    add_graphics_options(graphics_config);

    set_control_defaults();
    set_control_descriptions();
    recompui::config::create_controls_tab();

    auto &sound_config = recompui::config::create_sound_tab();
    add_sound_options(sound_config);

#if defined(__ANDROID__)
    auto &dual_screen_config = recompui::config::create_config_tab("Dual Screen", "dual_screen", false);
    add_dual_screen_options(dual_screen_config);
#endif

    auto &technical_config = recompui::config::create_config_tab("Technical", "technical", true);
    add_technical_options(technical_config);

    recompui::config::create_mods_tab();

    recompui::config::finalize();

}
