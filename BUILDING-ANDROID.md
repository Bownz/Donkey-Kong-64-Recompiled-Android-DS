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
| CI | `.github/workflows/android.yml` | Builds the APK on GitHub Actions |

Desktop builds are unaffected: everything Android-specific is behind `if (ANDROID)`.

## Two ways to build

| | Option A: Android Studio on your PC | Option B: GitHub Actions |
|---|---|---|
| Where it runs | Your Windows/Linux/macOS machine | GitHub's Linux servers |
| You provide the ROM | As a local file | In a private GitHub repo, read via a token |
| Result | `app/build/outputs/apk/release/app-release.apk` | A zip in the workflow run's *Artifacts* |
| Best for | Iterating on the code, debugging | Repeatable builds; no local toolchain setup |

Option B mirrors the configuration that was test-built for this port (Linux host,
NDK r30, CMake 4.1.2). Building on a **Windows** host is supported by the build files but
hasn't been test-built yet; if vcpkg's OpenSSL build gives trouble there, use Option B or WSL.

Both need your ROM at build time: the NTSC-U 1.0 Donkey Kong 64 ROM in `.z64` format
(sha1 `cf806ff2603640a748fca5026ded28802f1f4a50`). The build translates the game's code
from it into C, so there is no way to build without it. The finished APK doesn't contain
the ROM; it asks for your ROM again on the device, through Android's file picker.

The code generator wants the ROM *decompressed*. The repository has a tool for that,
`decompressor.cpp`: given your normal ROM, it writes `donkeykong64.decompressed.us.z64`.
GitHub Actions runs it for you; locally, see step A3.

## Option A: Android Studio on your PC (Windows)

### A1. Install the tools (once)

1. **Android Studio**, then in *Tools → SDK Manager → SDK Tools*, tick "Show Package
   Details" and install:
   - **NDK (Side by side) 30.0.16248370**
   - **CMake 4.1.2**
   - and under *SDK Platforms*, **Android 37**
2. **Git for Windows**, with long paths enabled (vcpkg and the submodules nest deeply):
   ```
   git config --global core.longpaths true
   ```
3. **Visual Studio 2022** (Community is fine) with *Desktop development with C++* and
   *C++ Clang Compiler for Windows* and *C++ CMake tools for Windows*. This is only to
   build N64Recomp (step A3); the Android build itself needs no host compiler.
4. **LLVM** and **make**, for the step that compiles the game's MIPS patches:
   ```
   winget install LLVM.LLVM
   winget install ezwinports.make
   ```
   Make sure `clang`, `ld.lld` and `make` work in a *new* terminal. If not, add
   `C:\Program Files\LLVM\bin` to `PATH`. Restart Android Studio afterwards so it sees
   the new `PATH`.

### A2. Get the code

Clone into a **short path** (Windows has path-length limits):
```
cd C:\
git clone --recurse-submodules -b android-port https://github.com/Bownz/Donkey-Kong-64-Recompiled-Android-DS.git dk64
cd dk64
```
If you forgot `--recurse-submodules`: `git submodule update --init --recursive`.
Don't make `lib/vcpkg` shallow: vcpkg needs its history.

### A3. Generate the recompiled code

1. Decompress your ROM into the repository root. The decompressor needs zlib, which is
   simplest in WSL (Ubuntu). From WSL:
   ```
   sudo apt install g++ zlib1g-dev
   cd /mnt/c/dk64
   g++ -std=c++17 -O2 decompressor.cpp -o decompressor -lz
   ./decompressor "/mnt/c/path/to/Donkey Kong 64 (USA).z64"
   ```
   This writes `donkeykong64.decompressed.us.z64` next to it. (The DK64 decomp's Python
   script, mentioned in `BUILDING.md`, produces the same file.)
2. Build N64Recomp and RSPRecomp, at the commit the CI uses. In
   *Developer PowerShell for VS 2022*:
   ```
   cd C:\
   git clone --recurse-submodules https://github.com/Mr-Wiseguy/N64Recomp.git
   cd N64Recomp
   git checkout 2b6f05688de2abc7d86da5b4a89b84c2c6acbabe
   git submodule update --init --recursive
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build --target N64RecompCLI RSPRecomp
   copy build\N64Recomp.exe C:\dk64\
   copy build\RSPRecomp.exe C:\dk64\
   ```
3. Run them from the repository root:
   ```
   cd C:\dk64
   .\N64Recomp.exe us.toml
   .\RSPRecomp.exe n_aspMain.toml
   ```
   This creates `RecompiledFuncs\`, `RecompiledPatches\` and `rsp\n_aspMain.cpp`. Redo
   this step only when the game patches or N64Recomp change.

### A4. Open and build in Android Studio

1. *File → Open*, select `C:\dk64` (the folder containing `settings.gradle`), and trust
   the project.
2. Let **Gradle sync** finish. The first sync is slow: it downloads SDL2 2.32.10, patches
   two submodules, and vcpkg builds curl (with OpenSSL and zlib) and FreeType for Android.
3. Choose the release variant: *Build → Select Build Variant…*, set `app` to **release**.
   Debug builds work too, but are slower to run.
4. *Build → Generate App Bundles or APKs → Generate APKs*. Or in Android Studio's
   *Terminal*:
   ```
   .\gradlew.bat assembleRelease
   ```
5. The APK is `app\build\outputs\apk\release\app-release.apk`. Android Studio's
   "locate" link in the build notification opens the folder.

Without your own key, this APK is signed with the debug key (see *Release signing*).

### A5. Install it

- **USB**: enable *Developer options → USB debugging* on the device, then
  ```
  %LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe install -r app\build\outputs\apk\release\app-release.apk
  ```
- **Or** copy the APK to the device and open it in a file manager; allow "install unknown
  apps" for that file manager when asked.

Logs: `adb logcat -s DK64Recomp`.

## Option B: GitHub Actions

The workflow is `.github/workflows/android.yml`. It runs on every push to the
`android-port` branch and on demand. It uses the **same ROM secrets as the desktop builds**,
so if those are already set up, skip to B3.

### B1. Put the ROM in a private repository

1. On GitHub, create a new repository, e.g. `dk64-rom`, and make it **Private**.
2. Upload your normal ROM (`.z64`) to its root. The workflow checks its SHA-1 and
   decompresses it. (Uploading `donkeykong64.decompressed.us.z64` instead also works.)

Never put the ROM in the main repository.

### B2. Give the workflow access to it

1. Create a token: *GitHub → Settings → Developer settings → Personal access tokens →
   Fine-grained tokens → Generate new token*.
   - *Repository access*: **Only select repositories** → your `dk64-rom` repo.
   - *Permissions → Repository permissions → Contents*: **Read-only**.
2. In the **main** repository: *Settings → Secrets and variables → Actions → New repository
   secret*, add:
   - `SECRET_NAME` = `your-username/dk64-rom`
   - `SECRET_TOKEN` = the token from step 1

Without these, the workflow's build job is skipped with a notice explaining what's missing.

### B3. (Recommended) Add a signing key

So each CI build installs over the previous one, add the four `ANDROID_*` secrets described
under *Release signing* below. Without them the APK still builds, but with a throwaway key.

### B4. Run it

- Push to `android-port`, or open the **Actions** tab → **android** → **Run workflow**,
  pick the `android-port` branch, and run.
- The first run is the slowest: vcpkg builds its dependencies from source. Later runs reuse
  cached N64Recomp and vcpkg packages.

### B5. Download the APK

Open the finished run, scroll to **Artifacts**, and download
`DK64Recompiled-Android-arm64-v8a`. It's a zip containing the APK; install it as in A5.

**Artifacts contain code generated from the ROM.** On a public repository, anyone with access
to the repository can download them. The workflow keeps them for 7 days only; making the
repository private keeps them to you (GitHub then counts the build against your Actions
minutes).

## Release signing

Android only installs an update over an existing install when both are signed with the
same key; otherwise you have to uninstall first, which deletes app storage (saves included,
unless you chose the shared folder). Builds use your key when these are set, otherwise the
debug key:

| Gradle setting / environment variable | Value |
|---|---|
| `DK64_KEYSTORE_FILE` | Path to the keystore (use forward slashes on Windows) |
| `DK64_KEYSTORE_PASSWORD` | Keystore password |
| `DK64_KEY_ALIAS` | Key alias |
| `DK64_KEY_PASSWORD` | Key password (defaults to the keystore password) |

**Create a key once** (keep the file and passwords safe; losing them means users must
reinstall):
```
"C:\Program Files\Android\Android Studio\jbr\bin\keytool.exe" -genkeypair -v -keystore C:\keys\dk64-release.jks -keyalg RSA -keysize 2048 -validity 10000 -alias dk64
```

**Local builds**: put the four values in `%USERPROFILE%\.gradle\gradle.properties` (outside
the repository):
```
DK64_KEYSTORE_FILE=C:/keys/dk64-release.jks
DK64_KEYSTORE_PASSWORD=...
DK64_KEY_ALIAS=dk64
DK64_KEY_PASSWORD=...
```

**GitHub Actions**: add these repository secrets:
- `ANDROID_KEYSTORE_BASE64`: the keystore file, base64-encoded. In PowerShell:
  `[Convert]::ToBase64String([IO.File]::ReadAllBytes("C:\keys\dk64-release.jks")) | Set-Clipboard`
- `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS`, `ANDROID_KEY_PASSWORD`

Use the same key locally and in CI and you can switch between the two freely.

## Troubleshooting

- **"Recompiled sources are missing"**: run step A3 (CI: check the ROM secrets).
- **CI "No usable ROM"**: the ROM repository has neither the decompressed ROM nor a `.z64`
  matching the US 1.0 hash. `.n64`/`.v64` files are byte-swapped; convert them to `.z64`.
- **"Could not run 'git'"** or **"Could not apply …patch"** during sync: Git isn't on
  `PATH`, or a submodule was updated. Run `git submodule update --init --recursive` and
  sync again.
- **`make`, `clang` or `ld.lld` not found** in the `PatchesBin` step: see A1 step 4, then
  restart Android Studio.
- **vcpkg fails during sync**: the error names a log file under `lib\vcpkg\buildtrees`.
  Path-length problems: move the clone to a shorter path. Other failures building OpenSSL on
  Windows: use Option B, or WSL with `android/build-native.sh`.
- **"App not installed" over an older version**: signature mismatch; see *Release signing*.

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
