// Optional custom Vulkan driver (e.g. Mesa Turnip for Adreno) for the renderer.
//
// The player installs a driver package in the app (DK64Activity / GpuDrivers.java), which
// unpacks it into the app's private files. When the renderer starts, plume asks
// dk64_android_vulkan_loader() (plume Android patch) for a vkGetInstanceProcAddr; this
// loads the chosen driver through libadrenotools, which redirects the system Vulkan loader
// to it. With no driver chosen, or if loading fails, it returns null and the system driver
// is used as usual.

#include <SDL.h>
#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>

#include <string>
#include <vector>

#if DK64_HAVE_ADRENOTOOLS
#include <adrenotools/driver.h>
#endif

namespace {
    constexpr const char* log_tag = "DK64Recomp";

    // Asks DK64Activity.getGpuDriver(): null for the system driver, otherwise four lines:
    // driver directory, driver library name, the app's native library directory (where the
    // adrenotools hooks are) and a writable temporary directory.
    std::vector<std::string> driver_from_activity() {
        std::vector<std::string> parts;
        JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        if (env == nullptr || activity == nullptr) {
            return parts;
        }
        jclass activity_class = env->GetObjectClass(activity);
        jmethodID method = env->GetMethodID(activity_class, "getGpuDriver", "()Ljava/lang/String;");
        if (method == nullptr) {
            env->ExceptionClear();
        }
        else {
            auto value = static_cast<jstring>(env->CallObjectMethod(activity, method));
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            else if (value != nullptr) {
                const char* chars = env->GetStringUTFChars(value, nullptr);
                std::string all = chars;
                env->ReleaseStringUTFChars(value, chars);
                env->DeleteLocalRef(value);
                size_t start = 0;
                while (start <= all.size()) {
                    size_t end = all.find('\n', start);
                    if (end == std::string::npos) {
                        end = all.size();
                    }
                    parts.emplace_back(all.substr(start, end - start));
                    start = end + 1;
                }
            }
        }
        env->DeleteLocalRef(activity_class);
        env->DeleteLocalRef(activity);
        return parts;
    }
}

extern "C" void* dk64_android_vulkan_loader() {
#if DK64_HAVE_ADRENOTOOLS
    std::vector<std::string> driver = driver_from_activity();
    if (driver.size() < 4 || driver[0].empty() || driver[1].empty()) {
        return nullptr; // System driver.
    }
    std::string driver_dir = driver[0];
    if (driver_dir.back() != '/') {
        driver_dir += '/';
    }
    std::string hook_dir = driver[2];
    if (!hook_dir.empty() && hook_dir.back() != '/') {
        hook_dir += '/';
    }
    __android_log_print(ANDROID_LOG_INFO, log_tag, "GPU driver: loading %s from %s", driver[1].c_str(), driver_dir.c_str());
    void* handle = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, driver[3].c_str(), hook_dir.c_str(),
        driver_dir.c_str(), driver[1].c_str(), nullptr, nullptr);
    if (handle == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, log_tag, "GPU driver: could not load the custom driver; using the system driver");
        return nullptr;
    }
    void* get_instance_proc_addr = dlsym(handle, "vkGetInstanceProcAddr");
    if (get_instance_proc_addr == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, log_tag, "GPU driver: custom driver has no vkGetInstanceProcAddr; using the system driver");
        return nullptr;
    }
    __android_log_print(ANDROID_LOG_INFO, log_tag, "GPU driver: custom driver loaded");
    return get_instance_proc_addr;
#else
    return nullptr;
#endif
}
