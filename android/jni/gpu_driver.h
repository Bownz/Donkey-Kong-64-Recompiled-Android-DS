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
}
