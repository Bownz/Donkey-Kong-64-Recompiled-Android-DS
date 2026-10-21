# Building for Android

The Android port builds the same top-level `CMakeLists.txt` as the desktop versions,
driven by Gradle and the NDK. The result is an `arm64-v8a` APK that runs the game
through SDL2 and RT64's Vulkan renderer.

## How the port is put together

| Piece | Where | What it does |
|---|---|---|
| Gradle project | `settings.gradle`, `build.gradle`, `app/build.gradle` | Builds the CMake project with the NDK, packages the APK |
| Manifest | `app/src/main/AndroidManifest.xml` | One landscape SDL activity, requires Vulkan 1.1 |
| Activity | `app/src/main/java/.../DK64Activity.java` | Extends `SDLActivity`; extracts UI assets on first run; hosts the file picker |
| Native entry | `android/jni/android_main.cpp` | `SDL_main` → sets paths and working directory → calls the desktop `main` |
| File dialogs | `android/jni/nfd_android.cpp` | Replaces nativefiledialog-extended with Android's document picker |
| CMake glue | `android/cmake/android.cmake` | Builds SDL2 from source, makes the game target `libmain.so` |
| Submodule patches | `android/patches/*.patch` | Small fixes to RT64 and RecompFrontend for Android (applied automatically) |

Desktop builds are unaffected: everything Android-specific is behind `if (ANDROID)`.

## Prerequisites

- Android Studio (recent), with **NDK 27.2.12479018** and **CMake 3.22.1** from the SDK Manager
- On the build machine: `git`, `cmake`, a C++ compiler, and the tools from `BUILDING.md`
  (`clang`, `ld.lld`, `make`) — host tools still run here during the build
- A Linux or macOS build machine (Windows: use WSL). RT64's shader compiler is picked
  for the host OS.
- The full submodule checkout, **not shallow** for `lib/vcpkg` (vcpkg needs its history
  to resolve the `builtin-baseline`)

## Steps

1. **Generate the recompiled code on the host**, exactly as for desktop
   (`BUILDING.md` steps 3–4). You need `RecompiledFuncs/`, `RecompiledPatches/` and
   `rsp/n_aspMain.cpp` before Gradle will build.

   ```bash
   ./N64Recomp us.toml
   ./RSPRecomp n_aspMain.toml
   ```

2. **Bootstrap vcpkg** once (it builds curl and freetype for Android):

   ```bash
   ./lib/vcpkg/bootstrap-vcpkg.sh
   ```

3. **Open the repository root in Android Studio** and let it sync. On the first sync,
   Gradle will:
   - download SDL2 2.30.3 into `android/third_party/` (SDLActivity's Java comes from here)
   - apply `android/patches/` to `lib/rt64` and `lib/RecompFrontend`
   - build a host copy of RT64's `file_to_c` into `build/android-host-tools/`

4. **Build** with *Build → Build APK(s)*, or from a terminal:

   ```bash
   gradle wrapper          # once, if you don't have gradlew yet
   ./gradlew assembleRelease
   ```

   The first native build is long; vcpkg builds curl, OpenSSL and freetype for arm64.

5. **Install and run**: `adb install -r app/build/outputs/apk/release/app-release.apk`.
   Logs: `adb logcat -s DK64Recomp`.

## Native libraries only (no Gradle)

To cross-compile just the `.so` files with the NDK (e.g. in CI, or to package yourself):

```bash
ANDROID_NDK_HOME=/path/to/android-ndk-r27c android/build-native.sh
```

It does the same preparation as Gradle (SDL2 download, submodule patches, host
`file_to_c`), configures with the NDK toolchain chained through vcpkg, builds the
`DK64Recompiled` target and leaves stripped `libmain.so`, `libSDL2.so` and
`libc++_shared.so` in `build/android-libs/arm64-v8a/`. Requires `ninja` on `PATH`.

### What the CMake changes do on Android

- `DK64Recompiled` becomes a shared library named `libmain.so` (what `SDLActivity` loads)
  instead of an executable; SDL2 is built in-tree as `libSDL2.so`.
- All static libraries are built position-independent, since they link into `libmain.so`.
- Tools that run during the build (`dxc` for shaders, `file_to_c`) are chosen for the
  **build machine**, not the target.
- `find_package(SDL2)` in RT64 is answered by `android/cmake/sdl2/SDL2Config.cmake`.
- X11, `-static-libstdc++` and the other desktop-Linux link settings are not used
  (Android's `CMAKE_SYSTEM_NAME` is `Android`, not `Linux`).

## On the device

- Pick the ROM from the launcher as usual. Android's file picker opens; the chosen file
  is copied into the app's folder. Use the normal (compressed) US ROM.
- Saves, config and mods live in `Android/data/io.github.bownz.dk64recomp/files/`,
  reachable over USB or with a file manager.

## Known limitations

- **Leaving the app (including the ROM picker) may break rendering.** Android destroys
  the window surface when the activity is covered; RT64 keeps using the old one. The fix
  is to recreate RT64's swapchain on `SDL_APP_DIDENTERFOREGROUND`. Until then, if the
  screen stays black after picking the ROM, restart the app — the ROM is remembered.
- **Mod function patching** uses `mprotect` on code pages; recent Android versions may
  refuse this for app libraries. The base game doesn't need it.
- Folder and save dialogs are not available (Android's picker returns documents, not paths).
- No touch controls yet; use a controller.
- The mod browser's HTTPS downloads may need a CA bundle configured for libcurl.
