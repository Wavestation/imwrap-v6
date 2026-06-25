#!/usr/bin/env zsh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/.build/debug"
DIST_DIR="$ROOT_DIR/dist"

FLUID_BUILD_DIR="$ROOT_DIR/third_party/fluidsynth-build"
FLUID_SOURCE_DIR="$ROOT_DIR/third_party/fluidsynth-master"

mkdir -p "$DIST_DIR"

# Build fluidsynth dependency
cmake -S "$FLUID_SOURCE_DIR" -B "$FLUID_BUILD_DIR" \
  -DCMAKE_PREFIX_PATH=/opt/homebrew \
  -DCMAKE_CXX_STANDARD=17 \
  -DBUILD_SHARED_LIBS=OFF \
  -Denable-libinstpatch=OFF \
  -Denable-libsndfile=OFF \
  -Denable-readline=OFF \
  -Denable-dbus=OFF \
  -Denable-jack=OFF \
  -Denable-pipewire=OFF \
  -Denable-pulseaudio=OFF \
  -Denable-portaudio=OFF \
  -Denable-sdl3=OFF \
  -Denable-oss=OFF \
  -Denable-coremidi=OFF \
  -Denable-ladspa=OFF \
  -Denable-systemd=OFF \
  -Denable-aufile=OFF \
  -Denable-ipv6=OFF \
  -Denable-network=OFF \
  -Denable-floats=ON \
  -Denable-limiter=OFF \
  -Denable-openmp=OFF \
  -Denable-framework=OFF \
  -Dosal=embedded \
  -Denable-coreaudio=ON

cmake --build "$FLUID_BUILD_DIR" -j4

# Build mt32emu dependency
MT32_SOURCE_DIR="$ROOT_DIR/third_party/munt/mt32emu"
MT32_BUILD_DIR="$ROOT_DIR/third_party/mt32emu-build"
cmake -S "$MT32_SOURCE_DIR" -B "$MT32_BUILD_DIR" \
  -DBUILD_SHARED_LIBS=OFF \
  -Dlibmt32emu_SHARED=OFF \
  -DBUILD_TESTING=OFF

cmake --build "$MT32_BUILD_DIR" -j4

# Build libADLMIDI dependency (OPL2 emulator for AdLib backend)
ADLMIDI_SOURCE_DIR="$ROOT_DIR/third_party/libadlmidi"
ADLMIDI_BUILD_DIR="$ROOT_DIR/third_party/adlmidi-build"
cmake -S "$ADLMIDI_SOURCE_DIR" -B "$ADLMIDI_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DWITH_MIDIPLAY=OFF \
  -DWITH_ADLMIDI2=OFF \
  -DWITH_OLD_UTILS=OFF \
  -DWITH_MUS2MID=OFF \
  -DWITH_XMI2MID=OFF \
  -DWITH_GENADLDATA=OFF \
  -DEXAMPLE_SDL2_AUDIO=OFF

cmake --build "$ADLMIDI_BUILD_DIR" --target ADLMIDI_static -j4
# Create a plain libADLMIDI.a alias that the Swift linker can find with -lADLMIDI
cp "$ADLMIDI_BUILD_DIR/libADLMIDI.a" "$ADLMIDI_BUILD_DIR/libADLMIDI.a" 2>/dev/null || true

# Compile all Swift executable targets
echo "Building Swift executables..."
swift build

# Helper function to package executable into .app bundle
package_app() {
    local target_name="$1"
    local display_name="$2"
    local bundle_id="$3"
    local include_resources="$4"
    
    local app_dir="$DIST_DIR/$target_name.app"
    local contents_dir="$app_dir/Contents"
    local macos_dir="$contents_dir/MacOS"
    local resources_dir="$contents_dir/Resources"
    
    rm -rf "$app_dir"
    mkdir -p "$macos_dir"
    
    cp "$BUILD_DIR/$target_name" "$macos_dir/$target_name"
    
    if [ "$include_resources" = "true" ]; then
        mkdir -p "$resources_dir"
        cp "$ROOT_DIR/samples/openquest-lite.ims" "$resources_dir/openquest-lite.ims"
        cp "$ROOT_DIR/samples/openquest-demo.ims" "$resources_dir/openquest-demo.ims"
        cp "$ROOT_DIR/baka/arachno.sf2" "$resources_dir/arachno.sf2"
    fi
    
    cat > "$contents_dir/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>fr</string>
  <key>CFBundleDisplayName</key>
  <string>$display_name</string>
  <key>CFBundleExecutable</key>
  <string>$target_name</string>
  <key>CFBundleIdentifier</key>
  <string>$bundle_id</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundleName</key>
  <string>$target_name</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleShortVersionString</key>
  <string>0.1</string>
  <key>CFBundleVersion</key>
  <string>1</string>
  <key>LSMinimumSystemVersion</key>
  <string>13.0</string>
  <key>NSHighResolutionCapable</key>
  <true/>
</dict>
</plist>
PLIST

    echo "Created $app_dir"
}

# Package the 3 apps
package_app "IMWrapAuthoringApp" "iMWrap Player" "org.scummtools.imwrap.player" "true"
package_app "IMWrapSysExTool" "iMWrap SysEx Tool" "org.scummtools.imwrap.sysex" "false"
package_app "IMWrapPackerTool" "iMWrap Packer Tool" "org.scummtools.imwrap.packer" "false"

echo "All apps packaged successfully in $DIST_DIR!"
