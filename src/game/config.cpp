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
// Read by the on-screen touch controls (TouchInput.nativeUiState).
static std::atomic<bool> dk64_android_analog_camera{false};

// Bit 1: a menu (the launcher, the menu bar, settings) is taking input, so the touch
// buttons are hidden and touches go to it. Bit 2: the Analog Camera is selected.
extern "C" JNIEXPORT jint JNICALL
Java_io_github_bownz_dk64recomp_TouchInput_nativeUiState(JNIEnv*, jclass) {
    jint bits = 0;
    if (recompui::is_context_capturing_input()) {
        bits |= 1;
    }
    if (dk64_android_analog_camera.load()) {
        bits |= 2;
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
        {dk64::CameraTypeMode::Analog, "Analog", "Analog Camera"}
    };
    config.add_enum_option(
        dk64::configkeys::general::camera_type,
        "Camera Type",
        "Changes the camera behavior.<br /><recomp-color primary>Free Cam</recomp-color>: Camera can be controlled via pressing C-Left and C-Right. The camera does not try to push itself behind the player.<br /><recomp-color primary>Follow Cam</recomp-color>: Similar to <recomp-color secondary>Free Cam</recomp-color>, but the camera tries to push itself behind the player.<br /><recomp-color primary>Better Free Cam</recomp-color>: Similar to <recomp-color secondary>Free Cam</recomp-color>, but instead of a button press turning the camera 45 degrees, holding the button moves the camera at 5 degrees per frame.<br /><recomp-color primary>Analog Camera</recomp-color>: Control of the camera is controlled by your right analogue stick. It is advised that C-Up is remapped to something outside your right analogue stick in order to have a good time with this.",
        camera_type_options,
        dk64::CameraTypeMode::Free
    );
#if defined(__ANDROID__)
    // The touch controls show a second (camera) stick while the Analog Camera is selected.
    config.add_option_change_callback(dk64::configkeys::general::camera_type,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            dk64_android_analog_camera.store(std::get<uint32_t>(cur) == static_cast<uint32_t>(dk64::CameraTypeMode::Analog));
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
extern "C" void dk64_android_set_touch_controls_mode(int mode);
extern "C" void dk64_android_set_button_icons(int enabled);
extern "C" int dk64_android_has_second_screen();
extern "C" void dk64_android_edit_touch_layout();

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
    };
    config.add_enum_option(
        bottom_buttons_option,
        "Bottom Screen Buttons",
        "The touch buttons on the bottom screen (oranges, gun, instrument, camera, first person). "
        "Turn them off to give the Kong barrels and collectibles the whole screen.",
        bottom_buttons_options,
        1u,
        one_screen
    );
    config.add_option_change_callback(bottom_buttons_option,
        [](recomp::config::ConfigValueVariant cur, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            dk64::android::set_bottom_buttons(std::get<uint32_t>(cur) == 1u);
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
            option(g, keys::swimming_invert_mode),
            option(d, "android_button_icons"),
            header("Touch"),
            option(d, "android_touch_controls"),
            edit_touch,
            option(d, "android_bottom_screen"),
            option(d, "android_bottom_buttons"),
            option(d, "android_touch_kong_swap"),
            option(d, "android_touch_feedback"),
            option(d, "android_touch_feedback_strength"),
            header("Camera"),
            option(g, keys::camera_type),
            option(g, keys::analog_camera_sensitivity),
            option(g, keys::third_person_camera_invert_mode),
            option(g, keys::first_person_invert_mode),
            header("Gyro"),
            option(g, gopts::gyro_sensitivity),
            option(g, keys::gyro_invert_mode),
        }, { g, d });

        auto &sound_config = recompui::config::create_sound_tab("Audio");
        add_sound_options(sound_config);

        recompui::config::create_layout_tab("Gameplay", "gameplay", {
            option(g, "android_experience"),
            option(g, keys::story_skip),
            option(g, keys::lightning_flashes),
            option(g, keys::skip_launcher),
        }, { g });
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

extern "C" int dk64_android_control_preset() {
    std::ifstream in{ control_preset_marker() };
    std::string value;
    if (!(in >> value)) {
        return -1;   // not chosen yet
    }
    return value == "modern" ? 1 : 0;
}

extern "C" void dk64_android_apply_control_preset(int modern) {
    using namespace recompinput;
    const int profile = profiles::get_sp_controller_profile_index();
    if (profile >= 0) {
        profiles::reset_profile_bindings(profile, InputDevice::Controller);
        if (modern) {
            // The right stick keeps C Down / Left / Right; only C Up (first person) comes off it,
            // so pushing the camera up doesn't drop into first person. C Up is R3.
            for (GameInput input : { GameInput::C_UP }) {
                for (size_t b = 0; b < num_bindings_per_input; b++) {
                    if (profiles::get_input_binding(profile, input, b).input_type == InputType::ControllerAnalog) {
                        // Pressing the right stick in is C Up instead (X still is too).
                        profiles::set_input_binding(profile, input, b, InputField::controller_digital(SDL_CONTROLLER_BUTTON_RIGHTSTICK));
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
        }
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
    graphics.save_config();

    recomp::mods::enable_mod("dk64_tag_anywhere", modern != 0);
    recomp::config::Config &dual = recompui::config::get_config("dual_screen");
    dual.update_option_value("android_touch_kong_swap", static_cast<uint32_t>(modern ? 1u : 0u));
    dual.update_option_value("android_button_icons", static_cast<uint32_t>(modern ? 1u : 0u));
    dual.save_config();

    recomp::config::Config &general = recompui::config::get_general_config();
    general.update_option_value(experience_option, static_cast<uint32_t>(modern ? 1u : 0u));
    general.save_config();

    std::ofstream out{ experience_marker() };
    out << (modern ? "modern" : "classic") << "\n";
    applying_experience = false;
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
            "anti-aliasing, quick Kong swapping (Tag Anywhere and Touch Kong Swap) and button icons that match your controller.<br />"
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
