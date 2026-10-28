// Android entry point for DK64: Recompiled.
//
// SDLActivity loads libmain.so and calls SDL_main() on its own native thread.
// This shim prepares the environment the desktop code expects, then calls the
// unmodified desktop main() (renamed to dk64_main at compile time, see
// android/cmake/android.cmake).

#include <SDL.h>
#include <SDL_system.h>
#include <android/log.h>
#include <jni.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <unistd.h>

int dk64_main(int argc, char** argv);

namespace {
    constexpr const char* log_tag = "DK64Recomp";

    // Forward stdout/stderr to logcat so the runtime's printf diagnostics show up in
    // `adb logcat -s DK64Recomp`.
    void redirect_stdio_to_logcat() {
        static int pipe_fds[2];
        setvbuf(stdout, nullptr, _IOLBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
        if (pipe(pipe_fds) != 0) {
            return;
        }
        dup2(pipe_fds[1], STDOUT_FILENO);
        dup2(pipe_fds[1], STDERR_FILENO);

        // Also keep this session's output in <internal>/logs/session.txt, so testers can share
        // it from the app (Diagnostics.java rotates the previous one before the game starts).
        FILE* session_log = nullptr;
        if (const char* internal = SDL_AndroidGetInternalStoragePath()) {
            std::string dir = std::string{internal} + "/logs";
            mkdir(dir.c_str(), 0700);
            session_log = fopen((dir + "/session.txt").c_str(), "a");
        }

        std::thread([session_log] {
            char buffer[1024];
            ssize_t size;
            while ((size = read(pipe_fds[0], buffer, sizeof(buffer) - 1)) > 0) {
                if (session_log != nullptr) {
                    fwrite(buffer, 1, size_t(size), session_log);
                    fflush(session_log);
                }
                if (buffer[size - 1] == '\n') {
                    size--;
                }
                buffer[size] = '\0';
                __android_log_write(ANDROID_LOG_INFO, log_tag, buffer);
            }
        }).detach();
    }

    // Asks DK64Activity.getDataDirectory() where saves, config, mods and the ROM go. On
    // first launch that shows the storage choice (and any permission prompt) and blocks
    // until the user answers, which is why it runs before the game creates its window.
    std::string data_directory_from_activity() {
        JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        if (env == nullptr || activity == nullptr) {
            return {};
        }

        std::string result;
        jclass activity_class = env->GetObjectClass(activity);
        jmethodID method = env->GetMethodID(activity_class, "getDataDirectory", "()Ljava/lang/String;");
        if (method == nullptr) {
            env->ExceptionClear();
        }
        else {
            auto path = static_cast<jstring>(env->CallObjectMethod(activity, method));
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            else if (path != nullptr) {
                const char* chars = env->GetStringUTFChars(path, nullptr);
                result = chars;
                env->ReleaseStringUTFChars(path, chars);
                env->DeleteLocalRef(path);
            }
        }
        env->DeleteLocalRef(activity_class);
        env->DeleteLocalRef(activity);
        return result;
    }
}

extern "C" void dk64_android_game_closing(); // gpu_driver.cpp

extern "C" int SDL_main(int argc, char* argv[]) {
    redirect_stdio_to_logcat();

    // Internal storage: private to the app. DK64Activity extracts the UI assets,
    // icons and controller database from the APK into <internal>/game.
    const char* internal_path = SDL_AndroidGetInternalStoragePath();
    // User data: either a shared "DK64Recompiled" folder (with the storage permission) or
    // the app's own external folder (Android/data/<package>/files). Both are reachable over
    // USB and in file managers. DK64Activity decides; this is the fallback.
    const std::string chosen_data_path = data_directory_from_activity();
    const char* external_path = SDL_AndroidGetExternalStoragePath();

    if (internal_path == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, log_tag, "No internal storage path: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    const std::filesystem::path game_dir = std::filesystem::path{internal_path} / "game";
    const std::filesystem::path data_dir = !chosen_data_path.empty() ? std::filesystem::path{chosen_data_path}
        : external_path != nullptr ? std::filesystem::path{external_path}
        : std::filesystem::path{internal_path} / "data";

    std::error_code ec;
    std::filesystem::create_directories(data_dir, ec);

    // recompui::file::get_app_folder_path() honours APP_FOLDER_PATH on Linux-like
    // platforms (Android defines __linux__), and RT64's user paths fall back to HOME.
    setenv("APP_FOLDER_PATH", data_dir.c_str(), 1);
    setenv("HOME", data_dir.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (data_dir / ".config").c_str(), 1);
    setenv("XDG_DATA_HOME", (data_dir / ".local" / "share").c_str(), 1);

    // The UI loads "assets/...", "icons/..." and "recompcontrollerdb.txt" relative to
    // the working directory, exactly like running the desktop build from the repo root.
    if (chdir(game_dir.c_str()) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, log_tag, "Could not enter %s", game_dir.c_str());
        return EXIT_FAILURE;
    }

    // Prefer AAudio, Android's current audio API, over SDL's default OpenSL ES. On an AYN Thor
    // the OpenSL ES player was stopped and restarted ~80 times a second, flooding the system
    // audio and Bluetooth services. OpenSL ES stays as the fallback. An explicit
    // SDL_AUDIODRIVER in the environment still wins.
    SDL_SetHintWithPriority(SDL_HINT_AUDIODRIVER, "aaudio,openslES", SDL_HINT_DEFAULT);

    __android_log_print(ANDROID_LOG_INFO, log_tag, "Assets: %s", game_dir.c_str());
    __android_log_print(ANDROID_LOG_INFO, log_tag, "User data: %s", data_dir.c_str());

    const int result = dk64_main(argc, argv);
    dk64_android_game_closing();
    return result;
}
