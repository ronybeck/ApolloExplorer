#!/bin/bash
#
# Build a native macOS .pkg installer for ApolloExplorer.
#
# Usage: packaging/macos/build-pkg.sh [version]
#
# If [version] is omitted, it is extracted from VERSION_STRING in
# protocolTypes.h at the repository root.
#
# Optional signing: export APPLE_INSTALLER_IDENTITY="Developer ID Installer: ..."
# to have the resulting package signed with productsign. Without it an
# unsigned .pkg is produced (fine for local installs / Gatekeeper will warn).
#
# Note: for notarized signed .app + .dmg distribution, see
# ../../build_macos_client.sh at the repository root. This script instead
# produces the native Installer.app package (.pkg) format.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VERSION_HEADER="$REPO_ROOT/protocolTypes.h"

if [ -n "${1:-}" ]; then
    VERSION="$1"
else
    VERSION="$(sed -n 's/^#define VERSION_STRING "\(.*\)"/\1/p' "$VERSION_HEADER")"
    if [ -z "$VERSION" ]; then
        echo "ERROR: could not extract VERSION_STRING from $VERSION_HEADER" >&2
        exit 1
    fi
fi
IDENTIFIER="com.ronybeck.apolloexplorer"
STAGE_DIR="$SCRIPT_DIR/pkgroot"
DIST_DIR="$SCRIPT_DIR/dist"
APP_BUNDLE="$REPO_ROOT/ApolloExplorerPC/ApolloExplorer.app"
ACP_BIN="$REPO_ROOT/acp/acp"
ASH_BIN="$REPO_ROOT/ash/ash"

echo "########## ApolloExplorer .pkg builder (version ${VERSION}) ##########"

echo "==> 0. Checking prerequisites"
if ! sw_vers | grep -q "macOS"; then
    echo "ERROR: this script can only be run on macOS." >&2
    exit 1
fi

if ! command -v qmake >/dev/null 2>&1; then
    echo "ERROR: qmake not found on PATH. Install Qt 6 (homebrew: brew install qt@6) and try again." >&2
    exit 1
fi

echo "==> 1. Clean previous build output"
rm -rf "$STAGE_DIR" "$DIST_DIR"
( cd "$REPO_ROOT" && make distclean >/dev/null 2>&1 || true )
rm -rf "$APP_BUNDLE"

echo "==> 2. Configuring project (qmake)"
( cd "$REPO_ROOT" && qmake -recursive MACOSX_DEPLOYMENT_TARGET="12.7.6" )

echo "==> 3. Building (make)"
( cd "$REPO_ROOT" && make -j"$(sysctl -n hw.ncpu)" )

if [ ! -d "$APP_BUNDLE" ]; then
    echo "ERROR: expected app bundle missing: $APP_BUNDLE" >&2
    exit 1
fi
if [ ! -x "$ACP_BIN" ]; then
    echo "ERROR: expected build output missing: $ACP_BIN" >&2
    exit 1
fi
if [ ! -x "$ASH_BIN" ]; then
    echo "ERROR: expected build output missing: $ASH_BIN" >&2
    exit 1
fi

echo "==> 4. Adding application icon"
mkdir -p "$APP_BUNDLE/Contents/Resources/"
cp "$REPO_ROOT/ApolloExplorerPC/icons/ApolloExplorer.icns" "$APP_BUNDLE/Contents/Resources/"

echo "==> 5. Bundling Qt frameworks (macdeployqt)"
macdeployqt "$APP_BUNDLE" -verbose=1

echo "==> 6. Staging package root"
mkdir -p "$STAGE_DIR/Applications"
mkdir -p "$STAGE_DIR/usr/local/bin"
cp -R "$APP_BUNDLE" "$STAGE_DIR/Applications/"
cp "$ACP_BIN" "$STAGE_DIR/usr/local/bin/acp"
cp "$ASH_BIN" "$STAGE_DIR/usr/local/bin/ash"
chmod 0755 "$STAGE_DIR/usr/local/bin/acp" "$STAGE_DIR/usr/local/bin/ash"

echo "==> 7. Building .pkg"
mkdir -p "$DIST_DIR"
PKG_FILE="$DIST_DIR/ApolloExplorer-${VERSION}.pkg"
pkgbuild \
    --root "$STAGE_DIR" \
    --identifier "$IDENTIFIER" \
    --version "$VERSION" \
    --install-location / \
    "$PKG_FILE"

if [ -n "${APPLE_INSTALLER_IDENTITY:-}" ]; then
    echo "==> 8. Signing package"
    SIGNED_PKG="$DIST_DIR/ApolloExplorer-${VERSION}-signed.pkg"
    productsign --sign "$APPLE_INSTALLER_IDENTITY" "$PKG_FILE" "$SIGNED_PKG"
    mv -f "$SIGNED_PKG" "$PKG_FILE"
fi

echo "==> 9. Cleanup"
rm -rf "$STAGE_DIR"
( cd "$REPO_ROOT" && make distclean >/dev/null 2>&1 || true )

echo ""
echo "Done: $PKG_FILE"
