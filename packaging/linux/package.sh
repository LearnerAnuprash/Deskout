#!/usr/bin/env bash
# Builds the Linux release packages from a CMake build directory:
#   Deskout-<version>-x86_64.AppImage   runs on most distributions
#   deskout_<version>_amd64.deb         installs on Ubuntu and Debian
# Both bundle the Qt libraries, so the target system needs no Qt.
#
# Usage: packaging/linux/package.sh <build-dir> <output-dir>
# The qmake of the Qt used for the build must be on PATH.
set -euo pipefail

build=$(realpath "$1")
out=$(realpath -m "$2")
version=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build/CMakeCache.txt")
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
tools=$build/linuxdeploy
appdir=$build/AppDir
deb=$build/deb

mkdir -p "$tools" "$out"
fetch() {
    [ -x "$tools/$1" ] && return
    curl -fsSL -o "$tools/$1" "$2"
    chmod +x "$tools/$1"
}
fetch linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fetch linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage

qmake=$(command -v qmake)
export LD_LIBRARY_PATH=$("$qmake" -query QT_INSTALL_LIBS)${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}

# The Qt plugin bundles every SQL driver it finds, and all but SQLite need
# database client libraries we don't ship. Point it at a copy of the plugin
# directory that only has the SQLite driver.
plugins=$build/qt-plugins
rm -rf "$plugins"
cp -as "$("$qmake" -query QT_INSTALL_PLUGINS)" "$plugins"
find "$plugins/sqldrivers" ! -type d ! -name libqsqlite.so -delete
cat > "$tools/qmake" <<EOF
#!/bin/sh
"$qmake" "\$@" | sed "s|^QT_INSTALL_PLUGINS:.*|QT_INSTALL_PLUGINS:$plugins|"
EOF
chmod +x "$tools/qmake"

export PATH=$tools:$PATH
export QMAKE=$tools/qmake
export APPIMAGE_EXTRACT_AND_RUN=1 # build machines and containers often have no FUSE
export NO_STRIP=1                 # the bundled strip is too old for some toolchains
export LINUXDEPLOY_OUTPUT_VERSION=$version

# AppImage. linuxdeploy leaves the finished AppDir behind for the .deb below.
rm -rf "$appdir"
(cd "$out" && linuxdeploy-x86_64.AppImage \
    --appdir "$appdir" \
    --executable "$build/deskout" \
    --desktop-file "$here/deskout.desktop" \
    --icon-file "$root/resources/icons/deskout.svg" \
    --plugin qt \
    --output appimage)

# .deb: the AppDir goes to /opt/deskout; the launcher entry and icon go where
# the desktop looks for them.
rm -rf "$deb"
mkdir -p "$deb/DEBIAN" "$deb/opt/deskout" "$deb/usr/bin" "$deb/usr/share"
cp -a "$appdir/usr/." "$deb/opt/deskout/"
mv "$deb/opt/deskout/share/applications" "$deb/opt/deskout/share/icons" "$deb/usr/share/"
ln -s /opt/deskout/bin/deskout "$deb/usr/bin/deskout"

cat > "$deb/DEBIAN/control" <<EOF
Package: deskout
Version: $version
Architecture: amd64
Maintainer: LearnerAnuprash <LearnerAnuprash@users.noreply.github.com>
Installed-Size: $(du -sk "$deb" | cut -f1)
Depends: libc6 (>= 2.35), libstdc++6, libgcc-s1, zlib1g, libx11-6, libx11-xcb1, libxcb1, libgl1, libegl1, libopengl0, libfontconfig1, libfreetype6, libcom-err2, libgpg-error0
Section: utils
Priority: optional
Homepage: https://github.com/LearnerAnuprash/Deskout
Description: Desktop wellness and focus app
 Break, hydration and walk reminders, a focus timer, notes, topic
 documents, daily updates and stats. Lives in the system tray.
EOF
chmod -R u+rwX,go+rX,go-w "$deb"
dpkg-deb --root-owner-group --build "$deb" "$out/deskout_${version}_amd64.deb"

ls -l "$out"
