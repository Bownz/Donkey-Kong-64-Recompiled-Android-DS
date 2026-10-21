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
#   ANDROID_NDK_HOME=/path/to/android-ndk-r27c android/build-native.sh
# Optional: ANDROID_ABI (arm64-v8a), ANDROID_PLATFORM (26), BUILD_TYPE (Release),
#           BUILD_DIR, OUT_DIR, VCPKG_ROOT.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to your Android NDK (r27c recommended)}"
ABI="${ANDROID_ABI:-arm64-v8a}"
API="${ANDROID_PLATFORM:-26}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
SDL_VERSION="2.30.3"
BUILD_DIR="${BUILD_DIR:-$ROOT/build/android-$ABI}"
OUT_DIR="${OUT_DIR:-$ROOT/build/android-libs/$ABI}"
HOST_TOOLS="$ROOT/build/android-host-tools"
SDL_DIR="$ROOT/android/third_party/SDL2-$SDL_VERSION"
VCPKG_ROOT="${VCPKG_ROOT:-$ROOT/lib/vcpkg}"

case "$ABI" in
    arm64-v8a) TRIPLET=arm64-android; NDK_TRIPLE=aarch64-linux-android ;;
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
    mkdir -p "$HOST_TOOLS" "$(dirname "$SDL_DIR")"
    archive="$HOST_TOOLS/SDL2-$SDL_VERSION.tar.gz"
    curl -fL -o "$archive" "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL2-$SDL_VERSION.tar.gz"
    tar -xzf "$archive" -C "$(dirname "$SDL_DIR")"
fi

# 3. Submodule patches (idempotent).
step "Applying Android patches to submodules"
apply_patch() {
    local submodule="$1" patch="$ROOT/$2"
    if git -C "$ROOT/$submodule" apply --reverse --check "$patch" 2>/dev/null; then
        echo "  $2: already applied"
    else
        git -C "$ROOT/$submodule" apply --whitespace=nowarn "$patch"
        echo "  $2: applied"
    fi
}
apply_patch lib/rt64 android/patches/rt64-android.patch
apply_patch lib/RecompFrontend android/patches/recompfrontend-android.patch

# 4. file_to_c runs during the build, so it is compiled for this machine.
if [ ! -x "$HOST_TOOLS/file_to_c" ]; then
    step "Building host file_to_c"
    cmake -S "$ROOT/lib/rt64/src/tools/file_to_c" -B "$HOST_TOOLS/file_to_c-build" -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "$HOST_TOOLS/file_to_c-build" --config Release >/dev/null
    cp "$(find "$HOST_TOOLS/file_to_c-build" -type f -name file_to_c -perm -u+x | head -n1)" "$HOST_TOOLS/file_to_c"
fi

# 5. Configure and build with the NDK. vcpkg is the toolchain and chain-loads the NDK's,
#    so curl and freetype are built for Android too.
step "Configuring ($ABI, android-$API, $BUILD_TYPE)"
export ANDROID_NDK_HOME
cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DVCPKG_TARGET_TRIPLET="$TRIPLET" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="android-$API" \
    -DANDROID_STL=c++_shared \
    -DDK64_ANDROID_SDL2_DIR="$SDL_DIR" \
    -DRT64_HOST_FILE_TO_C="$HOST_TOOLS/file_to_c"

step "Building libmain.so"
cmake --build "$BUILD_DIR" --target DK64Recompiled

# 6. Collect and strip the libraries an APK needs under lib/<abi>/.
step "Collecting libraries into $OUT_DIR"
mkdir -p "$OUT_DIR"
cp "$BUILD_DIR/libmain.so" "$BUILD_DIR/libSDL2.so" "$OUT_DIR/"
cp "$NDK_TOOLCHAIN/sysroot/usr/lib/$NDK_TRIPLE/libc++_shared.so" "$OUT_DIR/"
for lib in "$OUT_DIR"/*.so; do
    "$NDK_TOOLCHAIN/bin/llvm-strip" --strip-unneeded "$lib"
done
ls -lh "$OUT_DIR"
