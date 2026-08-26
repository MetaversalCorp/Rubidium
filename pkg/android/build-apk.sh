#!/usr/bin/env bash
# Build Rubidium for Android arm64 without Gradle.
#
# Required tools (all from Android SDK + NDK):
#   aapt2, d8, apksigner, zipalign  (Android build-tools)
#   javac, keytool                  (JDK)
#   cmake, ninja                    (build system)
#   cargo + aarch64-linux-android target (Wasmtime cross-compile)
#
# Environment overrides:
#   ANDROID_SDK   - path to Android SDK root   (default: ~/Android/Sdk)
#   ANDROID_NDK   - path to NDK root           (default: $ANDROID_SDK/ndk/<latest>)
#   BUILD_TOOLS   - path to build-tools dir    (default: auto-detected)
#   KEYSTORE      - path to signing keystore   (default: auto-generated debug key)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Script lives at pkg/android/build-apk.sh; repo root is two levels up.
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SNEEZE_DIR="${SNEEZE_DIR:-$(cd "$REPO_ROOT/../Sneeze" && pwd)}"

# ---------------------------------------------------------------------------
# Toolchain paths
# ---------------------------------------------------------------------------

ANDROID_SDK="${ANDROID_SDK:-$HOME/Android/Sdk}"

# Auto-detect NDK: prefer env var, then latest installed
if [ -z "${ANDROID_NDK:-}" ]; then
  NDK_ROOT="$ANDROID_SDK/ndk"
  if [ -d "$NDK_ROOT" ]; then
    ANDROID_NDK="$NDK_ROOT/$(ls "$NDK_ROOT" | sort -V | tail -1)"
  else
    echo "ERROR: ANDROID_NDK not set and $NDK_ROOT does not exist." >&2; exit 1
  fi
fi

# Auto-detect build-tools: prefer env var, then latest installed
if [ -z "${BUILD_TOOLS:-}" ]; then
  BT_ROOT="$ANDROID_SDK/build-tools"
  if [ -d "$BT_ROOT" ]; then
    BUILD_TOOLS="$BT_ROOT/$(ls "$BT_ROOT" | sort -V | tail -1)"
  else
    echo "ERROR: BUILD_TOOLS not set and $BT_ROOT does not exist." >&2; exit 1
  fi
fi

ABI=arm64-v8a
API_LEVEL=26
PLATFORM_JAR="$ANDROID_SDK/platforms/android-34/android.jar"

BUILD_DIR="$REPO_ROOT/build-android"
CMAKE_BUILD="$BUILD_DIR/cmake"
APK_STAGE="$BUILD_DIR/apk-stage"
SNEEZE_LIBS_DIR="${SNEEZE_LIBS_DIR:-$SNEEZE_DIR/libs-android}"

VERSION="$(cat "$REPO_ROOT/VERSION")"

mkdir -p "$CMAKE_BUILD" "$APK_STAGE/lib/$ABI"

# ---------------------------------------------------------------------------
# 1. Cross-compile Sneeze deps + Rubidium native .so
# ---------------------------------------------------------------------------
echo "==> Configuring CMake for $ABI..."
cmake -S "$REPO_ROOT/src" -B "$CMAKE_BUILD" \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ABI" \
  -DANDROID_PLATFORM="android-$API_LEVEL" \
  -DANDROID_STL=c++_shared \
  -DCMAKE_BUILD_TYPE=Release \
  -DSNEEZE_DIR="$SNEEZE_DIR" \
  -DSNEEZE_LIBS_DIR="$SNEEZE_LIBS_DIR" -DLIBS_DIR="$REPO_ROOT/libs" \
  -DSNEEZE_ENABLE_XR=OFF \
  -DSDL3_ROOT="${SDL3_ROOT:-$REPO_ROOT/sdl3-android-install}" \
  -DWASMTIME_CARGO_TARGET=aarch64-linux-android

echo "==> Building native library..."
cmake --build "$CMAKE_BUILD" -j"$(nproc)"

# ---------------------------------------------------------------------------
# 2. Compile SDL3 Java activity
# ---------------------------------------------------------------------------
SDL_JAVA_DIR="$REPO_ROOT/libs/SDL3/src/android-project/app/src/main/java"
CLASSES_DIR="$APK_STAGE/classes"
mkdir -p "$CLASSES_DIR"

echo "==> Compiling SDL3 Java activity + Rubidium MainActivity..."
{ find "$SDL_JAVA_DIR" -name "*.java"; find "$SCRIPT_DIR/java" -name "*.java"; } \
  | xargs javac -encoding UTF-8 -source 1.8 -target 1.8 \
      -cp "$PLATFORM_JAR" \
      -d "$CLASSES_DIR"

echo "==> Converting to DEX..."
"$BUILD_TOOLS/d8.bat" \
  --lib "$PLATFORM_JAR" \
  --output "$APK_STAGE" \
  $(find "$CLASSES_DIR" -name "*.class")

# ---------------------------------------------------------------------------
# 3. Package resources + manifest with aapt2
# ---------------------------------------------------------------------------
echo "==> Linking APK resources..."
AAPT2_ARGS=(
  -o "$APK_STAGE/unaligned.apk"
  -I "$PLATFORM_JAR"
  --manifest "$SCRIPT_DIR/AndroidManifest.xml"
  --min-sdk-version "$API_LEVEL"
  --target-sdk-version 34
  --version-code 1
  --version-name "$VERSION"
)

# Include compiled resources if a res/ dir exists
RES_FLAT="$APK_STAGE/res.zip"
if [ -d "$SCRIPT_DIR/res" ]; then
  "$BUILD_TOOLS/aapt2" compile --dir "$SCRIPT_DIR/res" -o "$RES_FLAT"
  AAPT2_ARGS+=("$RES_FLAT")
fi

"$BUILD_TOOLS/aapt2" link "${AAPT2_ARGS[@]}"

# ---------------------------------------------------------------------------
# 4. Add DEX + native libs into the APK zip
# ---------------------------------------------------------------------------
echo "==> Copying native libraries..."

# Rubidium main lib
find "$CMAKE_BUILD" -name "libRubidium.so" \
  | head -1 | xargs -I{} cp {} "$APK_STAGE/lib/$ABI/libmain.so"

# SDL3 shared lib (required for SDLActivity bootstrap)
SDL3_SO="$REPO_ROOT/libs/SDL3/install/lib/libSDL3.so"
[ -f "$SDL3_SO" ] && cp "$SDL3_SO" "$APK_STAGE/lib/$ABI/"

# Android C++ runtime (ANDROID_STL=c++_shared). NDK ships the prebuilt .so
# under toolchains/llvm/prebuilt/<host>/sysroot/...; locate it regardless of
# the host platform (Linux CI runner, Windows dev box, macOS runner).
LIBCXX_SO=$(find "$ANDROID_NDK/toolchains/llvm/prebuilt" \
   -path "*/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
   | head -1)
if [ -z "$LIBCXX_SO" ] || [ ! -f "$LIBCXX_SO" ]; then
  echo "ERROR: libc++_shared.so not found under $ANDROID_NDK" >&2; exit 1
fi
cp "$LIBCXX_SO" "$APK_STAGE/lib/$ABI/"

# Halogen ANARI library
cp "$SNEEZE_LIBS_DIR/ANARI-SDK/install/lib/libanari.so" "$APK_STAGE/lib/$ABI/"
cp "$SNEEZE_LIBS_DIR/Halogen/install/lib/libanari_library_halogen.so" "$APK_STAGE/lib/$ABI/"

# Wasmtime shared lib
WASMTIME_SO="$SNEEZE_LIBS_DIR/Wasmtime/install/lib/libwasmtime.so"
[ -f "$WASMTIME_SO" ] && cp "$WASMTIME_SO" "$APK_STAGE/lib/$ABI/"

echo "==> Assembling APK..."
cd "$APK_STAGE"
jar uf unaligned.apk classes.dex
jar uf unaligned.apk lib

# ---------------------------------------------------------------------------
# 5. Align + sign
# ---------------------------------------------------------------------------
ALIGNED="$BUILD_DIR/rubidium-$VERSION-android-arm64.apk"
"$BUILD_TOOLS/zipalign" -f 4 "$APK_STAGE/unaligned.apk" "$ALIGNED"

KEYSTORE="${KEYSTORE:-$BUILD_DIR/debug.keystore}"
if [ ! -f "$KEYSTORE" ]; then
  echo "==> Generating debug keystore..."
  keytool -genkeypair \
    -keystore "$KEYSTORE" -alias androiddebugkey \
    -keyalg RSA -keysize 2048 -validity 10000 \
    -storepass android -keypass android \
    -dname "CN=Rubidium Debug,O=Metaversal,C=US" 2>/dev/null
fi

SIGNED="$BUILD_DIR/rubidium-$VERSION-android-arm64-signed.apk"
"$BUILD_TOOLS/apksigner.bat" sign \
  --ks "$KEYSTORE" --ks-key-alias androiddebugkey \
  --ks-pass pass:android --key-pass pass:android \
  --out "$SIGNED" "$ALIGNED"

echo ""
echo "Done: $SIGNED"
echo "Install: adb install -r \"$SIGNED\""
