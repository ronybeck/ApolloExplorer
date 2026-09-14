#!/bin/bash
#
# Build a .deb package of ApolloExplorer for Debian-based distributions (Ubuntu).
#
# Usage: packaging/linux/build-deb.sh [version]
#
# If [version] is omitted, it is extracted from VERSION_STRING in
# protocolTypes.h at the repository root.
#
# Produces: packaging/linux/dist/apolloexplorer_<version>_<arch>.deb
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
ARCH="$(dpkg --print-architecture)"
DIST_DIR="$SCRIPT_DIR/dist"
STAGE_DIR="$SCRIPT_DIR/pkgroot"

echo "########## ApolloExplorer .deb builder (version ${VERSION}, ${ARCH}) ##########"

echo "==> 0. Checking prerequisites"
if ! grep -qi "debian\|ubuntu" /etc/os-release 2>/dev/null; then
    echo "WARNING: this script targets Debian/Ubuntu. Continuing anyway."
fi

if ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "ERROR: dpkg-deb not found. Install the 'dpkg-dev' package." >&2
    exit 1
fi

if ! command -v qmake6 >/dev/null 2>&1; then
    echo "==> Installing build dependencies (build-essential, qt6 dev packages)"
    sudo apt-get update -qq
    sudo apt-get install -qy build-essential qt6-base-dev qt6-tools-dev qt6-tools-dev-tools dpkg-dev
fi

echo "==> 1. Clean previous build output"
rm -rf "$STAGE_DIR" "$DIST_DIR"
( cd "$REPO_ROOT" && make distclean >/dev/null 2>&1 || true )

echo "==> 2. Configuring project (qmake6)"
( cd "$REPO_ROOT" && qmake6 -recursive )

echo "==> 3. Building (make)"
( cd "$REPO_ROOT" && make -j"$(nproc)" )

APP_BIN="$REPO_ROOT/ApolloExplorerPC/ApolloExplorer"
ACP_BIN="$REPO_ROOT/acp/acp"
ASH_BIN="$REPO_ROOT/ash/ash"

for f in "$APP_BIN" "$ACP_BIN" "$ASH_BIN"; do
    if [ ! -x "$f" ]; then
        echo "ERROR: expected build output missing: $f" >&2
        exit 1
    fi
done

echo "==> 4. Staging package tree"
install -d "$STAGE_DIR/DEBIAN"
install -d "$STAGE_DIR/usr/bin"
install -d "$STAGE_DIR/usr/share/applications"
install -d "$STAGE_DIR/usr/share/pixmaps"
install -d "$STAGE_DIR/usr/share/doc/apolloexplorer"

install -m 0755 "$APP_BIN" "$STAGE_DIR/usr/bin/ApolloExplorer"
install -m 0755 "$ACP_BIN" "$STAGE_DIR/usr/bin/acp"
install -m 0755 "$ASH_BIN" "$STAGE_DIR/usr/bin/ash"
install -m 0644 "$SCRIPT_DIR/apolloexplorer.desktop" "$STAGE_DIR/usr/share/applications/apolloexplorer.desktop"
install -m 0644 "$REPO_ROOT/ApolloExplorerPC/icons/Apollo_Explorer_icon.png" "$STAGE_DIR/usr/share/pixmaps/apolloexplorer.png"
install -m 0644 "$REPO_ROOT/LICENSE" "$STAGE_DIR/usr/share/doc/apolloexplorer/copyright"

sed -e "s/@VERSION@/${VERSION}/" -e "s/@ARCH@/${ARCH}/" \
    "$SCRIPT_DIR/debian/control.in" > "$STAGE_DIR/DEBIAN/control"

install -m 0755 "$SCRIPT_DIR/debian/postinst" "$STAGE_DIR/DEBIAN/postinst"
install -m 0755 "$SCRIPT_DIR/debian/postrm" "$STAGE_DIR/DEBIAN/postrm"

echo "==> 5. Building .deb"
mkdir -p "$DIST_DIR"
DEB_FILE="$DIST_DIR/apolloexplorer_${VERSION}_${ARCH}.deb"
dpkg-deb --root-owner-group --build "$STAGE_DIR" "$DEB_FILE"

echo "==> 6. Cleanup"
rm -rf "$STAGE_DIR"
( cd "$REPO_ROOT" && make distclean >/dev/null 2>&1 || true )

echo ""
echo "Done: $DEB_FILE"
echo "Install with: sudo apt install $DEB_FILE"
