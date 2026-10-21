// Android entry point for DK64: Recompiled.
//
// SDLActivity loads libmain.so and calls SDL_main() on its own native thread.
// This shim prepares the environment the desktop code expects, then calls the
// unmodified desktop main() (renamed to dk64_main at compile time, see
// android/cmake/android.cmake).

#include <SDL.h>
#include <SDL_system.h>
#include <android/log.h>

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

        std::thread([] {
            char buffer[1024];
            ssize_t size;
            while ((size = read(pipe_fds[0], buffer, sizeof(buffer) - 1)) > 0) {
                if (buffer[size - 1] == '\n') {
                    size--;
                }
                buffer[size] = '\0';
                __android_log_write(ANDROID_LOG_INFO, log_tag, buffer);
            }
        }).detach();
    }
}

extern "C" int SDL_main(int argc, char* argv[]) {
    redirect_stdio_to_logcat();

    // Internal storage: private to the app. DK64Activity extracts the UI assets,
    // icons and controller database from the APK into <internal>/game.
    const char* internal_path = SDL_AndroidGetInternalStoragePath();
    // External app storage (Android/data/<package>/files): visible over USB and in
    // file managers, so saves, config, mods and the imported ROM live here.
    const char* external_path = SDL_AndroidGetExternalStoragePath();

    if (internal_path == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, log_tag, "No internal storage path: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    const std::filesystem::path game_dir = std::filesystem::path{internal_path} / "game";
    const std::filesystem::path data_dir = external_path != nullptr
        ? std::filesystem::path{external_path}
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

    __android_log_print(ANDROID_LOG_INFO, log_tag, "Assets: %s", game_dir.c_str());
    __android_log_print(ANDROID_LOG_INFO, log_tag, "User data: %s", data_dir.c_str());

    return dk64_main(argc, argv);
}
