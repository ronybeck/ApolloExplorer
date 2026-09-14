#!/bin/bash
#
# Build an .rpm package of ApolloExplorer for Fedora.
#
# Usage: packaging/linux/build-rpm.sh [version]
#
# If [version] is omitted, it is extracted from VERSION_STRING in
# protocolTypes.h at the repository root.
#
# Packages the current working tree (including uncommitted changes), same as
# build-deb.sh.
#
# Produces: packaging/linux/dist/apolloexplorer-<version>-1.<dist>.<arch>.rpm
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
NAME="apolloexplorer"
TOPDIR="$SCRIPT_DIR/rpmbuild"
DIST_DIR="$SCRIPT_DIR/dist"

echo "########## ApolloExplorer .rpm builder (version ${VERSION}) ##########"

echo "==> 0. Checking prerequisites"
if ! grep -qi "fedora" /etc/os-release 2>/dev/null; then
    echo "WARNING: this script targets Fedora. Continuing anyway."
fi

if ! command -v rpmbuild >/dev/null 2>&1; then
    echo "==> Installing build dependencies (rpm-build, qt6 dev packages)"
    sudo dnf install -y rpm-build qt6-qtbase-devel qt6-qttools-devel gcc-c++ make git
fi

echo "==> 1. Preparing rpmbuild tree"
rm -rf "$TOPDIR" "$DIST_DIR"
mkdir -p "$TOPDIR"/{BUILD,RPMS,SOURCES,SPECS,SRPMS} "$DIST_DIR"

echo "==> 2. Archiving source (working tree) as ${NAME}-${VERSION}.tar.gz"
tar --create --gzip \
    --transform "s,^\.,${NAME}-${VERSION}," \
    --exclude-vcs \
    --exclude="./build" \
    --exclude="./packaging/linux/dist" \
    --exclude="./packaging/linux/rpmbuild" \
    --exclude="./packaging/linux/pkgroot" \
    --exclude="./packaging/macos/dist" \
    --exclude="./packaging/macos/pkgroot" \
    --exclude="./packaging/windows/dist" \
    --exclude="./packaging/windows/stage" \
    --file "$TOPDIR/SOURCES/${NAME}-${VERSION}.tar.gz" \
    -C "$REPO_ROOT" .

cp "$SCRIPT_DIR/apolloexplorer.spec" "$TOPDIR/SPECS/"

echo "==> 3. Building RPM"
rpmbuild --define "_topdir $TOPDIR" --define "app_version ${VERSION}" \
    -bb "$TOPDIR/SPECS/apolloexplorer.spec"

echo "==> 4. Collecting output"
find "$TOPDIR/RPMS" -name "*.rpm" -exec cp {} "$DIST_DIR/" \;

echo "==> 5. Cleanup"
rm -rf "$TOPDIR"

echo ""
echo "Done. Package(s) in: $DIST_DIR"
ls -1 "$DIST_DIR"
echo "Install with: sudo dnf install $DIST_DIR/${NAME}-${VERSION}-1.*.rpm"
