#!/bin/sh
# Markdown Editor launcher wrapper (POSIX sh).
#
# The Qt runtime ships inside the install dir (lib/ + plugins/ resolved via
# RPATH and qt.conf), but the Qt platform plugins additionally need a small
# set of *system* libraries (xcb, Wayland, EGL/GL, fontconfig, ...) that are
# part of Ubuntu and intentionally not bundled. When one of them is missing,
# Qt aborts with a cryptic "could not load the Qt platform plugin" error, so
# this wrapper preflights the critical SONAMEs first and prints the exact
# `apt install` fix.
#
# The check is advisory only: the real binary is always exec'd, so a false
# positive can never block a working setup.
#
# For tests, MARKDOWNEDITOR_LDCONFIG can point at a fake ldconfig binary.
set -u

APP_DIR="$(dirname "$(readlink -f "$0")")"
BIN="$APP_DIR/markdowneditor.bin"

# Sanity check the install tree itself: if the bundled Qt platform stack is
# incomplete (partial copy, failed install), Qt dies with "could not load
# the Qt platform plugin". Say so plainly instead.
bundled_ok=1
for required in \
    "$BIN" \
    "$APP_DIR/qt.conf" \
    "$APP_DIR/plugins/platforms/libqxcb.so" \
    "$APP_DIR"/lib/libQt6XcbQpa.so.* \
    "$APP_DIR"/lib/libQt6WaylandClient.so.* \
    "$APP_DIR"/lib/libQt6OpenGL.so.* \
    "$APP_DIR"/lib/libQt6Network.so.* \
    "$APP_DIR"/lib/libQt6Svg.so.*; do
    # Globs that match nothing stay unexpanded; compgen-free test:
    found=0
    for match in $required; do
        if [ -e "$match" ]; then found=1; break; fi
    done
    if [ "$found" -eq 0 ]; then bundled_ok=0; break; fi
done
if [ "$bundled_ok" -eq 0 ]; then
    cat >&2 <<EOF
Markdown Editor: the installation looks incomplete (missing bundled files).
Reinstall the application; if the problem persists, rerun
Installer/Linux/linuxdeploy.sh before rebuilding the installer.
EOF
fi

# SONAMEs the bundled xcb/Wayland platform plugins and QtGui need from the
# OS at startup. (Derived from ldd of plugins/platforms/*.so + libQt6Gui.)
CHECK_LIBS="libxcb-cursor.so.0 libxcb-icccm.so.4 libxcb-image.so.0
    libxcb-keysyms.so.1 libxcb-randr.so.0 libxcb-render.so.0
    libxcb-render-util.so.0 libxcb-shape.so.0 libxcb-shm.so.0
    libxcb-sync.so.1 libxcb-util.so.1 libxcb-xfixes.so.0 libxcb-xkb.so.1
    libxcb.so.1 libX11.so.6 libX11-xcb.so.1 libxkbcommon.so.0
    libxkbcommon-x11.so.0 libwayland-client.so.0 libwayland-cursor.so.0
    libwayland-egl.so.1 libEGL.so.1 libGL.so.1 libOpenGL.so.0 libdrm.so.2
    libfontconfig.so.1 libfreetype.so.6 libdbus-1.so.3 libglib-2.0.so.0"

LDCONFIG="${MARKDOWNEDITOR_LDCONFIG:-ldconfig}"
if command -v "$LDCONFIG" >/dev/null 2>&1; then
    cache="$("$LDCONFIG" -p 2>/dev/null)"
    missing=""
    for soname in $CHECK_LIBS; do
        case "$cache" in
            *"$soname"*) ;;
            *) missing="$missing $soname" ;;
        esac
    done
    if [ -n "$missing" ]; then
        cat >&2 <<EOF
Markdown Editor: missing system libraries:$missing
The bundled Qt platform plugins (xcb/Wayland) need these from Ubuntu.
Install them with:
  sudo apt install libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \\
    libxcb-randr0 libxcb-render0 libxcb-render-util0 libxcb-shape0 libxcb-shm0 \\
    libxcb-sync1 libxcb-util1 libxcb-xfixes0 libxcb-xkb1 libx11-6 libx11-xcb1 \\
    libxkbcommon0 libxkbcommon-x11-0 libwayland-client0 libwayland-cursor0 \\
    libwayland-egl1 libegl1 libgl1 libopengl0 libdrm2 libfontconfig1 \\
    libfreetype6 libdbus-1-3 libglib2.0-0t
Trying to start anyway...
EOF
    fi
fi

exec "$BIN" "$@"
