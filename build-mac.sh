#!/bin/sh
# RPVerb Reconstruction CLAP: build for Apple Silicon, install to ~/Library/Audio/Plug-Ins/CLAP and
# check the installed bundle the way a host loads it. Needs the Command Line Tools and CMake.
set -eu
cd "$(dirname "$0")"
BUILD=build-mac
cmake -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DBUILD_TESTING=ON
cmake --build "$BUILD" --parallel
(cd "$BUILD" && ctest --output-on-failure -R host_smoke)

DEST_DIR="$HOME/Library/Audio/Plug-Ins/CLAP"
DEST="$DEST_DIR/RPVerb Reconstruction.clap"
mkdir -p "$DEST_DIR"
rm -rf "$DEST"
cp -R "$BUILD/RPVerbReconstruction.clap" "$DEST"
codesign --force --sign - "$DEST" >/dev/null 2>&1 || true
plutil -lint "$DEST/Contents/Info.plist" >/dev/null
nm -gU "$DEST/Contents/MacOS/RPVerbReconstruction" | grep -q '_clap_entry' || { echo "error: clap_entry not exported" >&2; exit 1; }
codesign --verify "$DEST"
"$BUILD/rpverb_host_test" "$DEST/Contents/MacOS/RPVerbReconstruction"
echo "Installed: $DEST"
echo "Rescan plug-ins in the host if it does not appear. Uninstall: rm -rf \"$DEST\""
