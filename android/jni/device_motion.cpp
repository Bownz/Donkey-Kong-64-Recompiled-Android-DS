// Gyro aiming with the device's own motion sensors.
//
// Android handhelds such as the AYN Thor have their gyro and accelerometer in the device, not
// in the built-in controller, so SDL sees a controller without motion sensors. This reads the
// device sensors directly (NDK sensor API, on its own thread so every sample keeps its real
// timestamp) and feeds them to RecompFrontend's gyro handling as one more controller, mapped
// to the way the screen is held. It steps aside while a controller with its own gyro is used.

#include <android/looper.h>
#include <android/sensor.h>
#include <android/log.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <thread>

#include "recompinput/recompinput.h"

extern "C" bool recompinput_android_controller_gyro_active() __attribute__((weak));
extern "C" int dk64_android_display_rotation();

namespace {
    constexpr const char* log_tag = "DK64Recomp";
    constexpr SDL_JoystickID device_motion_id = -1000;
    constexpr int looper_id = 0x6D6F; // "mo"
    constexpr int sample_period_us = 1000000 / 200;

    std::atomic<bool> started{false};

    // Device axes (natural orientation) -> screen axes for the current rotation, then to SDL
    // controller axes as if the screen were the controller's face: +X right, +Y out of the
    // screen, +Z down the screen (towards the player when lying flat).
    void device_to_controller(int rotation, const float* in, float* out) {
        float sx, sy;
        switch (rotation) {
            case 1:  sx = -in[1]; sy = in[0]; break;
            case 2:  sx = -in[0]; sy = -in[1]; break;
            case 3:  sx = in[1];  sy = -in[0]; break;
            default: sx = in[0];  sy = in[1]; break;
        }
        out[0] = sx;
        out[1] = in[2];
        out[2] = -sy;
    }

    void run() {
        ASensorManager* manager = ASensorManager_getInstanceForPackage("io.github.bownz.dk64recomp");
        if (manager == nullptr) {
            manager = ASensorManager_getInstance();
        }
        const ASensor* gyro = manager ? ASensorManager_getDefaultSensor(manager, ASENSOR_TYPE_GYROSCOPE) : nullptr;
        const ASensor* accel = manager ? ASensorManager_getDefaultSensor(manager, ASENSOR_TYPE_ACCELEROMETER) : nullptr;
        if (gyro == nullptr || accel == nullptr) {
            __android_log_print(ANDROID_LOG_INFO, log_tag, "Device motion: no %s", gyro == nullptr ? "gyroscope" : "accelerometer");
            return;
        }
        ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
        ASensorEventQueue* queue = ASensorManager_createEventQueue(manager, looper, looper_id, nullptr, nullptr);
        if (queue == nullptr) {
            __android_log_print(ANDROID_LOG_WARN, log_tag, "Device motion: couldn't create a sensor queue");
            return;
        }
        for (const ASensor* s : { gyro, accel }) {
            ASensorEventQueue_enableSensor(queue, s);
            int period = sample_period_us;
            if (period < ASensor_getMinDelay(s)) {
                period = ASensor_getMinDelay(s);
            }
            ASensorEventQueue_setEventRate(queue, s, period);
        }
        __android_log_print(ANDROID_LOG_INFO, log_tag, "Device motion: using %s and %s", ASensor_getName(gyro), ASensor_getName(accel));

        recompinput::ControllerState& state = recompinput::get_controller_state(device_motion_id);
        float accel_g[3] = { 0.0f, 1.0f, 0.0f };
        int64_t prev_gyro_ns = 0;
        int rotation = dk64_android_display_rotation() & 3;
        int64_t rotation_checked_ns = 0;
        uint32_t samples = 0;
        constexpr float rad_to_deg = 180.0f / static_cast<float>(M_PI);

        while (true) {
            int id = ALooper_pollOnce(-1, nullptr, nullptr, nullptr);
            if (id != looper_id) {
                continue;
            }
            ASensorEvent events[16];
            ssize_t count;
            while ((count = ASensorEventQueue_getEvents(queue, events, 16)) > 0) {
                const bool controller_gyro = recompinput_android_controller_gyro_active != nullptr && recompinput_android_controller_gyro_active();
                for (ssize_t i = 0; i < count; i++) {
                    const ASensorEvent& e = events[i];
                    if (e.timestamp - rotation_checked_ns > 2000000000LL) {
                        rotation = dk64_android_display_rotation() & 3;
                        rotation_checked_ns = e.timestamp;
                    }
                    float v[3];
                    device_to_controller(rotation, e.data, v);
                    if (e.type == ASENSOR_TYPE_ACCELEROMETER) {
                        accel_g[0] = v[0] / ASENSOR_STANDARD_GRAVITY;
                        accel_g[1] = v[1] / ASENSOR_STANDARD_GRAVITY;
                        accel_g[2] = v[2] / ASENSOR_STANDARD_GRAVITY;
                        continue;
                    }
                    if (e.type != ASENSOR_TYPE_GYROSCOPE) {
                        continue;
                    }
                    float dt = prev_gyro_ns == 0 ? 0.0f : (e.timestamp - prev_gyro_ns) * 1e-9f;
                    prev_gyro_ns = e.timestamp;
                    if (dt <= 0.0f || dt > 0.1f) {
                        continue;
                    }
                    state.motion.ProcessMotion(v[0] * rad_to_deg, v[1] * rad_to_deg, v[2] * rad_to_deg,
                        accel_g[0], accel_g[1], accel_g[2], dt);
                    if (samples++ == 0) {
                        __android_log_print(ANDROID_LOG_INFO, log_tag, "Device motion: receiving samples (screen rotation %d)", rotation);
                    }
                    if (controller_gyro) {
                        continue;
                    }
                    float rot_x = 0.0f;
                    float rot_y = 0.0f;
                    state.motion.GetPlayerSpaceGyro(rot_x, rot_y);
                    recompinput::add_rotation_deltas(device_motion_id, rot_x, rot_y);
                }
            }
        }
    }
}

// Called once after SDL_Init (src/main/main.cpp).
extern "C" void dk64_android_start_device_motion() {
    if (started.exchange(true)) {
        return;
    }
    // Create the controller entry here, on the game's thread, before samples arrive.
    recompinput::get_controller_state(device_motion_id);
    std::thread(run).detach();
}
