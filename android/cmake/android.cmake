# Android-specific build configuration for DK64Recompiled.
#
# Included from the top-level CMakeLists.txt when it is configured by the Android
# Gradle plugin (see app/build.gradle). Desktop builds never include this file.
#
# What it does:
#   * builds SDL2 from source as libSDL2.so (its Java glue is compiled by Gradle
#     from the same source tree, so the two always match);
#   * replaces nativefiledialog-extended (no Android backend) with a stub that
#     opens Android's system file picker over JNI;
#   * turns the game target into libmain.so, which SDLActivity loads and runs.

if (NOT DEFINED DK64_ANDROID_SDL2_DIR OR NOT EXISTS "${DK64_ANDROID_SDL2_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "DK64_ANDROID_SDL2_DIR does not point to an SDL2 source tree. "
        "Build through Gradle (it downloads SDL2 into android/third_party), "
        "or pass -DDK64_ANDROID_SDL2_DIR=<path to SDL2 source>.")
endif()

if (NOT DEFINED RT64_HOST_FILE_TO_C OR NOT EXISTS "${RT64_HOST_FILE_TO_C}")
    message(FATAL_ERROR
        "RT64_HOST_FILE_TO_C must point to a file_to_c binary built for the build machine. "
        "Gradle builds it automatically into build/host-tools.")
endif()

# ---------------------------------------------------------------------------
# SDL2
# ---------------------------------------------------------------------------
# SDL compiles the NDK's cpufeatures sources via ${ANDROID_NDK}. The NDK's toolchain file
# sets that; CMake's built-in Android support only sets CMAKE_ANDROID_NDK.
if (NOT ANDROID_NDK AND CMAKE_ANDROID_NDK)
    set(ANDROID_NDK "${CMAKE_ANDROID_NDK}")
endif()

set(SDL_SHARED ON  CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
add_subdirectory("${DK64_ANDROID_SDL2_DIR}" "${CMAKE_BINARY_DIR}/SDL2" EXCLUDE_FROM_ALL)

if (NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 ALIAS SDL2)
endif()

# RT64, plume, ultramodern, recompui and recompinput all expect SDL headers on the
# include path (desktop builds get them from the system or vcpkg).
set(SDL2_INCLUDE_DIRS "${DK64_ANDROID_SDL2_DIR}/include")
include_directories(${SDL2_INCLUDE_DIRS})

# RT64 calls find_package(SDL2 REQUIRED) on every non-Windows platform. Answer it with
# the in-tree build above. (An explicit <Pkg>_DIR bypasses the NDK's find-root rules.)
set(SDL2_DIR "${CMAKE_SOURCE_DIR}/android/cmake/sdl2" CACHE PATH "SDL2 config shim for Android" FORCE)

# ---------------------------------------------------------------------------
# Every static library (RT64, librecomp, RmlUi, the recompiled code, ...) is linked
# into libmain.so, so all of it must be position-independent.
# ---------------------------------------------------------------------------
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Keeps RT64 away from its X11 code paths. On Android, RT64/plume still build the
# Vulkan surface from the window's ANativeWindow (see android/patches).
add_compile_definitions(RT64_SDL_WINDOW_VULKAN)

# ---------------------------------------------------------------------------
# nfd replacement (must exist before lib/rt64 is added; the rt64 patch checks for it)
# ---------------------------------------------------------------------------
add_library(nfd STATIC "${CMAKE_SOURCE_DIR}/android/jni/nfd_android.cpp")
target_include_directories(nfd PUBLIC
    "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/nativefiledialog-extended/src/include")
target_link_libraries(nfd PRIVATE SDL2::SDL2 log)

# ---------------------------------------------------------------------------
# Game target -> libmain.so
# ---------------------------------------------------------------------------
function(dk64_configure_android_target target)
    # SDLActivity loads libSDL2.so and libmain.so, then calls SDL_main() from libmain.
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "main")

    # The desktop entry point is a plain main() (SDL_MAIN_HANDLED). Rename it so the
    # Android shim can set up paths first and then call it.
    set_source_files_properties("${CMAKE_SOURCE_DIR}/src/main/main.cpp"
        PROPERTIES COMPILE_DEFINITIONS "main=dk64_main")
    target_sources(${target} PRIVATE "${CMAKE_SOURCE_DIR}/android/jni/android_main.cpp")

    target_link_libraries(${target} PRIVATE SDL2::SDL2 android log)

    # Fail at link time rather than at System.loadLibrary() time on the device.
    target_link_options(${target} PRIVATE "-Wl,--no-undefined")
endfunction()
