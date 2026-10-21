#!/usr/bin/env bash
# Cross-compile the Android native libraries with the NDK, without Gradle.
#
# Produces, in build/android-libs/<abi>/:
#   libmain.so        the game (recompiled code, runtime, RT64, UI)
#   libSDL2.so        SDL2, built from source
#   libc++_shared.so  the NDK's C++ runtime, which both of the above link against
#
# Gradle (app/build.gradle) performs the same preparation and packages these into the
# APK; keep the two in step (SDL version, CMake arguments).
#
# Usage:
#   ANDROID_NDK_HOME=/path/to/android-ndk-r30 android/build-native.sh
# Optional: ANDROID_ABI (arm64-v8a), ANDROID_PLATFORM (28), BUILD_TYPE (Release),
#           BUILD_DIR, OUT_DIR, VCPKG_ROOT.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to your Android NDK (r30 recommended)}"
ABI="${ANDROID_ABI:-arm64-v8a}"
API="${ANDROID_PLATFORM:-28}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
SDL_VERSION="2.32.10"  # keep in step with app/build.gradle
BUILD_DIR="${BUILD_DIR:-$ROOT/build/android-$ABI}"
OUT_DIR="${OUT_DIR:-$ROOT/build/android-libs/$ABI}"
SDL_DIR="$ROOT/android/third_party/SDL2-$SDL_VERSION"
VCPKG_ROOT="${VCPKG_ROOT:-$ROOT/lib/vcpkg}"

case "$ABI" in
    arm64-v8a) NDK_TRIPLE=aarch64-linux-android ;;
    *) echo "error: only arm64-v8a is supported (RT64 and the recompiled code target 64-bit ARM)" >&2; exit 1 ;;
esac

case "$(uname -s)" in
    Linux)  NDK_HOST=linux-x86_64 ;;
    Darwin) NDK_HOST=darwin-x86_64 ;;
    *) echo "error: build on Linux or macOS (on Windows, use WSL)" >&2; exit 1 ;;
esac
NDK_TOOLCHAIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$NDK_HOST"

step() { printf '\n==> %s\n' "$*"; }

for tool in cmake ninja git; do
    command -v "$tool" >/dev/null || { echo "error: '$tool' not found on PATH" >&2; exit 1; }
done

# 1. Recompiled sources are generated on the host from the ROM (BUILDING.md steps 3-4).
step "Checking generated sources"
if [ -z "$(ls -A "$ROOT/RecompiledFuncs" 2>/dev/null)" ] || [ ! -f "$ROOT/rsp/n_aspMain.cpp" ]; then
    echo "error: RecompiledFuncs/ or rsp/n_aspMain.cpp missing. Generate them first:" >&2
    echo "  ./N64Recomp us.toml && ./RSPRecomp n_aspMain.toml" >&2
    exit 1
fi

# 2. SDL2 source (its Java glue must match, so Gradle uses this same tree).
if [ ! -f "$SDL_DIR/CMakeLists.txt" ]; then
    step "Downloading SDL2 $SDL_VERSION"
    mkdir -p "$ROOT/build/android-downloads" "$(dirname "$SDL_DIR")"
    archive="$ROOT/build/android-downloads/SDL2-$SDL_VERSION.tar.gz"
    curl -fL -o "$archive" "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL2-$SDL_VERSION.tar.gz"
    tar -xzf "$archive" -C "$(dirname "$SDL_DIR")"
fi

# SDL fixes. SDL sits inside this work tree (git-ignored), so apply from the repository root
# with --directory; from inside the SDL folder git apply silently does nothing.
sdl_rel="${SDL_DIR#"$ROOT"/}"
if git -C "$ROOT" apply --reverse --check --ignore-whitespace --directory="$sdl_rel" "$ROOT/android/patches/sdl2-android.patch" 2>/dev/null; then
    echo "  sdl2-android.patch: already applied"
else
    git -C "$ROOT" apply --ignore-whitespace --whitespace=nowarn --directory="$sdl_rel" "$ROOT/android/patches/sdl2-android.patch"
    echo "  sdl2-android.patch: applied"
fi

# 3. Submodule patches (idempotent).
step "Applying Android patches to submodules"
apply_patch() {
    local submodule="$1" patch="$ROOT/$2"
    if git -C "$ROOT/$submodule" apply --reverse --check --ignore-whitespace "$patch" 2>/dev/null; then
        echo "  $2: already applied"
    else
        git -C "$ROOT/$submodule" apply --ignore-whitespace --whitespace=nowarn "$patch"
        echo "  $2: applied"
    fi
}
apply_patch lib/rt64 android/patches/rt64-android.patch
apply_patch lib/rt64/src/contrib/plume android/patches/plume-android.patch
apply_patch lib/RecompFrontend android/patches/recompfrontend-android.patch

# 4. Configure and build with the NDK. The top-level CMakeLists.txt routes the NDK
#    toolchain through vcpkg (so curl and freetype are built for Android too) and runs
#    file_to_c as a CMake script, so no host compiler is needed.
step "Configuring ($ABI, android-$API, $BUILD_TYPE)"
cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="android-$API" \
    -DANDROID_STL=c++_shared \
    -DDK64_ANDROID_SDL2_DIR="$SDL_DIR"

step "Building libmain.so"
cmake --build "$BUILD_DIR" --target DK64Recompiled

# 5. Collect and strip the libraries an APK needs under lib/<abi>/.
step "Collecting libraries into $OUT_DIR"
mkdir -p "$OUT_DIR"
cp "$BUILD_DIR/libmain.so" "$BUILD_DIR/libSDL2.so" "$OUT_DIR/"
cp "$NDK_TOOLCHAIN/sysroot/usr/lib/$NDK_TRIPLE/libc++_shared.so" "$OUT_DIR/"
for lib in "$OUT_DIR"/*.so; do
    "$NDK_TOOLCHAIN/bin/llvm-strip" --strip-unneeded "$lib"
done
ls -lh "$OUT_DIR"
