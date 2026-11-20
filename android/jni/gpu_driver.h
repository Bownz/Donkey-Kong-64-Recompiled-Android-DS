// Custom Vulkan driver selection for the Android build (android/jni/gpu_driver.cpp).
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace dk64::android {
    struct GpuDriverChoice {
        std::string id;      // "" = the system driver
        std::string label;
    };

    // The installed drivers (system driver first) and the id of the selected one.
    std::vector<GpuDriverChoice> list_gpu_drivers(std::string &selected_id);
    // Selects a driver for the next start ("" = system).
    void select_gpu_driver(const std::string &id);
    // Opens the driver manager (install / remove packages).
    void open_gpu_driver_manager();
    // A description of the driver the renderer is using right now.
    std::string active_gpu_driver();
    // Opens the share sheet with a log report (Diagnostics.java).
    void share_log();
    // The app's version, e.g. "0.2.0-beta (76e2c32)".
    std::string app_version();
    // The "Touch Kong Swap" setting (android/jni/game_state.cpp reports it to the bottom screen).
    void set_touch_kong_swap(bool enabled);
    void set_touch_feedback(bool enabled);
    void set_bottom_first_person(bool shown);   // the bottom screen's first person button
    void set_bottom_buttons(int mode);   // the bottom screen's touch buttons: 0 off, 1 on, 2 appear on Z
    // Settings > Gameplay > Game Files Location (StorageAccess.startChange / describeLocation).
    void change_storage_location();
    std::string storage_description();
    // Input > Touch > Drag to Move Camera (android/jni/touch_controller.cpp).
    void set_drag_camera(bool enabled);
    void set_haptic_feedback(bool enabled);   // Haptic Feedback (Input)
    void set_gun_button(int mode);            // Shoot Gun With (Input): 0 B, 1 ZR, 2 both
    int gun_button();
    void set_jump_assist(bool enabled);       // Jump Assist (Input): buffered jumps, coyote time
    void set_quick_turn(bool enabled);        // Quick Turn (Input): no skid on sharp turns
    void set_movement_polish(bool enabled);   // Movement Polish (Input): lean, landing squash, momentum jumps
    void set_smooth_running(bool enabled);    // Smooth Running (Input): gradual speed up / slow down
    void set_game_feel_bit(int bit, bool enabled);   // the other Game Feel options (bits 16+)
    int game_feel_flags();
    void report_rainbow_coin(int level, int flag);   // at boot (recomp_report_rainbow_coin)
    void set_fp_status(int status);   // Walk in First Person: 1 on, 2 gun out (recomp_set_fp_status)
    int fp_status();                    // bit 0 Jump Assist, bit 1 Quick Turn, bit 2 Smooth Running
    bool haptic_feedback_enabled();
    void phone_haptic_pulse(float strength, int ms);   // the phone's vibrator (touch play)
    bool drag_camera_enabled();
    void set_touch_feedback_strength(int strength);   // 1 light, 2 medium, 3 strong
    // The "Show FPS" setting (the counter is drawn by DK64Activity).
    void set_show_fps(bool enabled);
    // Technical > Show Area: the current map-chunk in the corner (for reporting slow spots).
    void set_show_area(bool enabled);
    // The "Bottom Screen" setting (DK64Activity shows or hides the second-screen view).
    void set_bottom_screen(bool enabled);
    // The draw distance to use (android/jni/game_state.cpp): the setting, or the game's normal
    // distance in the listed heavy areas.
    int draw_distance(int configured);
}
