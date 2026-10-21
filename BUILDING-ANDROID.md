# Building for Android

The Android port builds the same top-level `CMakeLists.txt` as the desktop versions,
driven by Gradle and the NDK. The result is an `arm64-v8a` APK that runs the game
through SDL2 and RT64's Vulkan renderer. It needs Android 9 (API 28) or newer and a
Vulkan 1.1 GPU.

## How the port is put together

| Piece | Where | What it does |
|---|---|---|
| Gradle project | `settings.gradle`, `build.gradle`, `app/build.gradle` | Builds the CMake project with the NDK, packages the APK |
| Manifest | `app/src/main/AndroidManifest.xml` | One landscape SDL activity, requires Vulkan 1.1 |
| Activity | `app/src/main/java/.../DK64Activity.java` | Extends `SDLActivity`: lifecycle, immersive fullscreen, asset extraction, file picker, places the touch controls |
| Storage | `app/src/main/java/.../StorageAccess.java` | First-launch choice of shared folder vs app storage, and the storage permission flow |
| Touch controls | `TouchControllerView.java`, `ControllerPresentation.java`, `TouchInput.java`, `android/jni/touch_controller.cpp` | On-screen N64 controller driving an SDL virtual gamepad; on a second screen when there is one |
| Native entry | `android/jni/android_main.cpp` | `SDL_main` → sets paths and working directory → calls the desktop `main` |
| File dialogs | `android/jni/nfd_android.cpp` | Replaces nativefiledialog-extended with Android's document picker |
| CMake glue | `android/cmake/android.cmake` | Builds SDL2 from source, makes the game target `libmain.so` |
| Submodule patches | `android/patches/*.patch` | Small fixes to RT64 and RecompFrontend for Android (applied automatically) |

Desktop builds are unaffected: everything Android-specific is behind `if (ANDROID)`.

## Prerequisites

- **Android Studio** with, from *SDK Manager → SDK Tools* (tick "Show Package Details"):
  - **NDK 30.0.16248370** (r30)
  - **CMake 4.1.2**
  - SDK Platform **Android 37**
  The project is set up for AGP 9.4.1 / Gradle 9.6.0, the versions Android Studio
  currently generates.
- **Git** on `PATH` (Git for Windows on Windows). Gradle uses it to apply
  `android/patches/` to two submodules.
- The full submodule checkout (`git clone --recurse-submodules`). Don't make `lib/vcpkg`
  shallow: vcpkg needs its history to resolve the `builtin-baseline`.
- For **generating the recompiled code** (step 1), the same tools as a desktop build:
  N64Recomp/RSPRecomp, plus `make`, `clang` and `ld.lld` for the `patches/` step
  (see `BUILDING.md`; on Windows: LLVM and `choco install make`).

No host C++ compiler is needed for the Android part itself: the build-time `file_to_c`
step runs as a CMake script, and RT64's shader compiler (`dxc`) ships prebuilt for
Windows, Linux and macOS.

## Steps

1. **Generate the recompiled code**, exactly as for desktop (`BUILDING.md` steps 3–4).
   You need `RecompiledFuncs/`, `RecompiledPatches/` and `rsp/n_aspMain.cpp` before
   the APK will build.

   ```
   N64Recomp us.toml
   RSPRecomp n_aspMain.toml
   ```

2. **Open the repository root in Android Studio** (*File → Open*, pick the folder with
   `settings.gradle`) and let it sync. On the first sync, Gradle will:
   - download SDL2 2.32.10 into `android/third_party/` (SDLActivity's Java comes from here)
   - apply `android/patches/` to `lib/rt64` and `lib/RecompFrontend`
   - configure CMake, during which vcpkg bootstraps itself and builds curl (with
     OpenSSL and zlib) and FreeType for arm64-android. This first sync takes several
     minutes.

3. **Build** with *Build → Generate App Bundles or APKs → Generate APKs*, or from a terminal:

   ```
   gradlew assembleRelease
   ```

   (If there's no `gradlew` yet, let Android Studio create it, or run `gradle wrapper`.)

4. **Install and run**: `adb install -r app/build/outputs/apk/release/app-release.apk`.
   Logs: `adb logcat -s DK64Recomp`.

## Native libraries only (no Gradle)

To cross-compile just the `.so` files with the NDK (e.g. in CI, or to package yourself),
on Linux, macOS or WSL:

```bash
ANDROID_NDK_HOME=/path/to/android-ndk-r30 android/build-native.sh
```

It does the same preparation as Gradle (SDL2 download, submodule patches), configures
with the NDK toolchain, builds the `DK64Recompiled` target and leaves stripped
`libmain.so`, `libSDL2.so` and `libc++_shared.so` in `build/android-libs/arm64-v8a/`.
Requires `ninja` on `PATH`.

### What the CMake changes do on Android

- `DK64Recompiled` becomes a shared library named `libmain.so` (what `SDLActivity` loads)
  instead of an executable; SDL2 is built in-tree as `libSDL2.so`.
- All static libraries are built position-independent, since they link into `libmain.so`.
- Tools that run during the build are chosen for the **build machine**, not the target:
  RT64's prebuilt `dxc` for shaders, and `android/cmake/file_to_c.cmake` in place of the
  compiled `file_to_c` (same output, no host compiler needed).
- The NDK toolchain that Gradle passes is routed through vcpkg automatically, so curl
  and FreeType are built for Android without extra Gradle configuration.
- `find_package(SDL2)` in RT64 is answered by `android/cmake/sdl2/SDL2Config.cmake`.
- X11, `-static-libstdc++` and the other desktop-Linux link settings are not used
  (Android's `CMAKE_SYSTEM_NAME` is `Android`, not `Linux`).

## On the device

- **First launch** asks where to keep saves, settings, mods and the ROM:
  - **Shared folder**: `DK64Recompiled` at the top of your storage. Easy to reach from a
    PC or file manager for mods, and kept if you uninstall. Needs file access: Android 11+
    opens the "All files access" setting (turn it on, then come back); Android 9–10 shows
    the usual permission dialog. If access is refused, the game falls back to app storage.
  - **App storage**: `Android/data/io.github.bownz.dk64recomp/files/`. No permission needed;
    removed on uninstall.

  To change your mind later, clear the app's data (or reinstall).
- **Pick the ROM** from the launcher as usual. Android's file picker opens. In shared-folder
  mode a ROM on your storage is used in place; otherwise it's copied into the app's folder.
  Use the normal (compressed) US ROM.
- **Controls**:
  - A physical gamepad, including a handheld's built-in controls, is used automatically and
    hides the touch controls.
  - With no gamepad, an on-screen N64 controller appears: analog stick, A, B, Z, R,
    C-buttons, D-pad, Start, and ☰ for the recomp menu. It works like a normal controller,
    so you can remap it in the game's settings. Touches outside the controls go to the game,
    so the launcher's menus work by touch.
  - On a device with a **second screen** (e.g. a dual-screen handheld), the controller goes on
    that screen at full size and the game keeps the main screen uncovered.
  - Connecting or disconnecting a controller or display switches this live.

## Known limitations

- **Leaving the app (including the ROM picker) may break rendering.** Android destroys
  the window surface when the activity is covered; RT64 keeps using the old one. The fix
  is to recreate RT64's swapchain on `SDL_APP_DIDENTERFOREGROUND`. Until then, if the
  screen stays black after picking the ROM, restart the app — the ROM is remembered.
- **Mod function patching** uses `mprotect` on code pages; recent Android versions may
  refuse this for app libraries. The base game doesn't need it.
- Folder and save dialogs are not available (Android's picker returns documents, not paths).
- Touch controls have a fixed layout; there's no editor for button positions or size yet.
- The mod browser's HTTPS downloads may need a CA bundle configured for libcurl.
