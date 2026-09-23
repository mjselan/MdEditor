#!/usr/bin/env bash
#
# linuxdeploy.sh - build and stage a self-contained Linux application tree.
#
# The script is the Linux counterpart of Installer/MacOS/macdeploy.sh and
# Installer/Windows/windeploy.bat:
#   1. configure and build the Release CMake preset;
#   2. copy the executable into the Qt Installer Framework package data dir;
#   3. bundle the Qt shared libraries it needs (ldd) next to it under lib/;
#   4. copy the required Qt plugins, write qt.conf, and point RPATH at
#      $ORIGIN/lib so the installed app runs without a system Qt;
#   5. stage the .desktop reference file and the PNG icon.
#
# Usage:
#   QT_DIR=$HOME/Qt/6.11.1/gcc_64 ./Installer/Linux/linuxdeploy.sh
#
# QT_DIR may be omitted when qmake6/qmake is on PATH. Set
# MARKDOWNEDITOR_PRESET to use a different CMake preset (default: release).
#
# Target: Ubuntu 24.04 and later (x86_64). Build on the oldest Ubuntu you
# ship to: a binary built on a newer release needs a newer glibc and will
# not start on 24.04. Qt Installer Framework 4.11 is used by
# Installer/Linux/make-installer.sh.
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd -P)"
INSTALLER_DIR="$SCRIPT_DIR"
PKG_DIR="$INSTALLER_DIR/packages"
DATA_DIR="$PKG_DIR/com.mdeditor.markdowneditor/data"
PRESET="${MARKDOWNEDITOR_PRESET:-release}"
BUILD_DIR="$REPO_ROOT/build/$PRESET"

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

info() {
    printf '%s\n' "$*"
}

find_qt_dir() {
    if [[ -n "${QT_DIR:-}" ]]; then
        printf '%s\n' "$QT_DIR"
        return
    fi

    local qmake
    for qmake in qmake6 qmake; do
        if command -v "$qmake" >/dev/null 2>&1; then
            "$qmake" -query QT_INSTALL_PREFIX
            return
        fi
    done

    # Well-known locations: Qt online installer default, then system Qt.
    local candidate
    for candidate in \
        "$HOME/Qt/6.11.1/gcc_64" \
        /usr/lib/qt6 \
        /usr/lib/x86_64-linux-gnu/qt6; do
        if [[ -d "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return
        fi
    done
}

find_cmake() {
    if command -v cmake >/dev/null 2>&1; then
        command -v cmake
        return
    fi

    # Qt online installations ship CMake under Tools, but it is not on PATH.
    if [[ -n "${QT_DIR:-}" ]]; then
        local candidate
        for candidate in \
            "$QT_DIR/../../Tools/CMake/bin/cmake" \
            "$QT_DIR/../Tools/CMake/bin/cmake" \
            "$QT_DIR/bin/cmake"; do
            if [[ -x "$candidate" ]]; then
                printf '%s\n' "$candidate"
                return
            fi
        done
    fi

    return 1
}

[[ "$(uname -s)" == "Linux" ]] || die "linuxdeploy.sh must be run on Linux"
[[ "$(uname -m)" == "x86_64" ]] || \
    die "only x86_64 builds are supported (got: $(uname -m))"

QT_DIR="$(find_qt_dir)"
[[ -n "$QT_DIR" ]] || die "Qt was not found; set QT_DIR to a Qt 6 gcc_64 kit"
export QT_DIR

# CMakePresets.json uses Ninja. Prefer the system binary, but also support
# the Ninja tool bundled with a Qt online installation when PATH is minimal.
if ! command -v ninja >/dev/null 2>&1; then
    for ninja_candidate in \
        "$QT_DIR/../../Tools/Ninja/ninja" \
        "$QT_DIR/../Tools/Ninja/ninja" \
        "$QT_DIR/Tools/Ninja/ninja"; do
        if [[ -x "$ninja_candidate" ]]; then
            PATH="$(dirname -- "$ninja_candidate"):$PATH"
            export PATH
            break
        fi
    done
fi

CMAKE="$(find_cmake || true)"
[[ -n "$CMAKE" ]] || die "CMake was not found; install CMake or set it on PATH"
command -v patchelf >/dev/null 2>&1 || \
    die "patchelf is required (sudo apt install patchelf)"
command -v ldd >/dev/null 2>&1 || die "ldd was not found"

# Resolve the Qt plugin and library roots for both the online installer
# layout (QT_DIR/plugins, QT_DIR/lib) and distro layouts.
QT_PLUGINS=""
for candidate in "$QT_DIR/plugins" "$QT_DIR/lib/qt6/plugins" \
                 "$QT_DIR/plugins/x86_64-linux-gnu" \
                 "/usr/lib/x86_64-linux-gnu/qt6/plugins"; do
    if [[ -d "$candidate/platforms" ]]; then
        QT_PLUGINS="$candidate"
        break
    fi
done
[[ -n "$QT_PLUGINS" ]] || die "Qt plugins not found under $QT_DIR"

# Copy one runtime library into lib/ unless already staged. Only the Qt,
# ICU, and Sonnet stacks are bundled; system libraries stay on the host.
# $1 = SONAME (e.g. libQt6XcbQpa.so.6), $2 = ldd-resolved path (may be
# empty or "not": staged objects have no RPATH yet, so ldd cannot resolve
# their Qt dependencies -- fall back to the Qt kit and Sonnet prefix).
# Returns 0 when a new file was staged.
stage_runtime_lib() {
    local lib_name="$1" lib_path="${2:-}"
    case "$lib_name" in
        libQt6*.so*|libicu*.so*|libKF6*.so*|libhunspell*.so*) ;;
        *) return 1 ;;
    esac
    [[ -f "$DATA_DIR/lib/$lib_name" ]] && return 1
    if [[ -z "$lib_path" || ! -f "$lib_path" ]]; then
        local d
        for d in "$QT_DIR/lib" \
                 "$REPO_ROOT/build/prefix/lib/x86_64-linux-gnu" \
                 "$REPO_ROOT/build/prefix/lib"; do
            if [[ -f "$d/$lib_name" ]]; then
                lib_path="$d/$lib_name"
                break
            fi
        done
    fi
    [[ -n "$lib_path" && -f "$lib_path" ]] || return 1
    cp -Lf "$lib_path" "$DATA_DIR/lib/$lib_name" 2>/dev/null || \
        cp -f "$lib_path" "$DATA_DIR/lib/$lib_name"
}

info "[1/5] Building the $PRESET preset..."
"$CMAKE" --preset "$PRESET"
"$CMAKE" --build --preset "$PRESET" --target markdowneditor

SOURCE_BIN="$BUILD_DIR/markdowneditor"
[[ -x "$SOURCE_BIN" ]] || \
    die "Release build did not produce $SOURCE_BIN"

info "[2/5] Cleaning the Installer Framework package data directory..."
rm -rf "$DATA_DIR"
mkdir -p "$DATA_DIR/lib" "$DATA_DIR/plugins"

info "[3/5] Staging the executable and bundled shared libraries..."
# The real binary ships as markdowneditor.bin; the `markdowneditor` launcher
# wrapper (preflight + exec) keeps that name so .desktop files and user
# habits are unaffected.
cp -f "$SOURCE_BIN" "$DATA_DIR/markdowneditor.bin"
chmod +x "$DATA_DIR/markdowneditor.bin"
if command -v strip >/dev/null 2>&1; then
    strip --strip-unneeded "$DATA_DIR/markdowneditor.bin" 2>/dev/null || true
fi
if [[ -f "$INSTALLER_DIR/markdowneditor.sh" ]]; then
    cp -f "$INSTALLER_DIR/markdowneditor.sh" "$DATA_DIR/markdowneditor"
    chmod +x "$DATA_DIR/markdowneditor"
else
    die "launcher wrapper is missing (Installer/Linux/markdowneditor.sh)"
fi

# Bundle every shared library the executable resolves to that comes from a
# Qt installation, the ICU stack Qt links, or the optional Sonnet stack.
# System libraries (/lib, /usr/lib outside Qt) stay on the host: Ubuntu
# 24.04 provides them (see README prerequisites) and bundling them would
# risk symbol conflicts with Mesa/libc.
bundled=0
while IFS= read -r line; do
    # ldd lines look like: libQt6Core.so.6 => /path/libQt6Core.so.6 (addr)
    # or, when unresolvable: libQt6XcbQpa.so.6 => not found
    lib_name="$(printf '%s\n' "$line" | sed 's/^[[:space:]]*//;s/ =>.*//')"
    lib_path="$(printf '%s\n' "$line" | sed -n 's/.*=> \([^ ]*\).*/\1/p')"
    [[ "$lib_path" == "not" ]] && lib_path=""
    if stage_runtime_lib "$lib_name" "$lib_path"; then
        bundled=$((bundled + 1))
    fi
done < <(ldd "$SOURCE_BIN" 2>/dev/null)

# Second pass: bundled Qt libraries have their own dependencies (ICU data,
# libraries shipped with Qt, ...). Repeat until no new files appear so the
# tree is closed under lib/.
for _pass in 1 2 3; do
    added=0
    for staged in "$DATA_DIR"/lib/*.so*; do
        [[ -f "$staged" ]] || continue
        while IFS= read -r line; do
            lib_name="$(printf '%s\n' "$line" | sed 's/^[[:space:]]*//;s/ =>.*//')"
            lib_path="$(printf '%s\n' "$line" | sed -n 's/.*=> \([^ ]*\).*/\1/p')"
            [[ "$lib_path" == "not" ]] && lib_path=""
            if stage_runtime_lib "$lib_name" "$lib_path"; then
                added=$((added + 1))
            fi
        done < <(ldd "$staged" 2>/dev/null)
    done
    (( added == 0 )) && break
done

[[ "$bundled" -gt 0 ]] || die "no Qt libraries were collected; is ldd working?"
[[ -f "$DATA_DIR/lib/libQt6Core.so.6" || -f "$DATA_DIR/lib/libQt6Core.so" ]] || \
    die "bundling did not capture libQt6Core; QT_DIR may be wrong ($QT_DIR)"
[[ -f "$DATA_DIR/lib/libQt6Gui.so.6" || -f "$DATA_DIR/lib/libQt6Gui.so" ]] || \
    die "bundling did not capture libQt6Gui"
[[ -f "$DATA_DIR/lib/libQt6Widgets.so.6" || -f "$DATA_DIR/lib/libQt6Widgets.so" ]] || \
    die "bundling did not capture libQt6Widgets"

info "[4/5] Staging Qt plugins, qt.conf, and RPATH..."
# Plugin sets needed by a Qt Widgets app on X11 and Wayland. eglfs/linuxfb/
# vnc/vkkhrdisplay are kiosk targets and are deliberately skipped to keep
# the package small, as is wayland-graphics-integration-server (it serves
# Wayland compositors, which this client app never loads -- bundling it
# would drag in the whole QtQuick/QML stack). Debug (.debug) files are
# skipped the same way. libqtiff/libqmng need external system codecs that
# Ubuntu desktops may lack; they fail gracefully, but skipping them keeps
# the tree clean (common formats stay: gif/ico/jpeg/svg/tga/wbmp/webp).
copy_plugin() {
    local src="$QT_PLUGINS/$1"
    local dst="$DATA_DIR/plugins/$1"
    [[ -d "$src" ]] || return 0
    mkdir -p "$dst"
    local f
    for f in "$src"/*.so; do
        [[ -f "$f" ]] || continue
        case "$(basename -- "$f")" in
            libqeglfs.so|libqlinuxfb.so|libqvnc.so|libqvkkhrdisplay.so|\
            libqminimalegl.so|libqtiff.so|libqmng.so|libqpdf.so|*.debug) continue ;;
        esac
        # Keep only the platform backends relevant on Ubuntu desktops.
        if [[ "$1" == "platforms" ]]; then
            case "$(basename -- "$f")" in
                libqxcb.so|libqwayland.so|libqminimal.so|libqoffscreen.so) ;;
                *) continue ;;
            esac
        fi
        cp -Lf "$f" "$dst/" 2>/dev/null || cp -f "$f" "$dst/"
    done
}

copy_plugin platforms
copy_plugin platformthemes
copy_plugin xcbglintegrations
copy_plugin wayland-graphics-integration-client
copy_plugin wayland-shell-integration
copy_plugin wayland-decoration-client
copy_plugin imageformats
copy_plugin iconengines
copy_plugin styles
copy_plugin networkinformation
copy_plugin tls
copy_plugin printsupport
copy_plugin generic
copy_plugin platforminputcontexts

[[ -f "$DATA_DIR/plugins/platforms/libqxcb.so" ]] || \
    die "platforms/libqxcb.so is missing; X11 systems could not start the app"

# Plugin closure: the main binary's ldd does not cover libraries that only
# plugins link (XcbQpa/WaylandClient for the platform plugins, OpenGL for
# the GL integrations, Network for the TLS/network plugins, Svg for the
# icon engine). Collect those into lib/ too, or the app dies at startup
# with "could not load the Qt platform plugin".
while IFS= read -r plugin; do
    while IFS= read -r line; do
        lib_name="$(printf '%s\n' "$line" | sed 's/^[[:space:]]*//;s/ =>.*//')"
        lib_path="$(printf '%s\n' "$line" | sed -n 's/.*=> \([^ ]*\).*/\1/p')"
        [[ "$lib_path" == "not" ]] && lib_path=""
        stage_runtime_lib "$lib_name" "$lib_path" || true
    done < <(ldd "$plugin" 2>/dev/null)
done < <(find "$DATA_DIR/plugins" "$DATA_DIR/kf6" -type f -name '*.so' -print 2>/dev/null)

# One more closure pass over lib/ for the newly added support libraries.
for _pass in 1 2; do
    added=0
    for staged in "$DATA_DIR"/lib/*.so*; do
        [[ -f "$staged" ]] || continue
        while IFS= read -r line; do
            lib_name="$(printf '%s\n' "$line" | sed 's/^[[:space:]]*//;s/ =>.*//')"
            lib_path="$(printf '%s\n' "$line" | sed -n 's/.*=> \([^ ]*\).*/\1/p')"
            [[ "$lib_path" == "not" ]] && lib_path=""
            if stage_runtime_lib "$lib_name" "$lib_path"; then
                added=$((added + 1))
            fi
        done < <(ldd "$staged" 2>/dev/null)
    done
    (( added == 0 )) && break
done

for required in libQt6XcbQpa libQt6WaylandClient libQt6OpenGL libQt6Network libQt6Svg; do
    if ! ls "$DATA_DIR/lib/$required.so"* >/dev/null 2>&1; then
        die "bundling did not capture $required (needed by the staged plugins)"
    fi
done

# qt.conf makes the staged tree relocatable: Qt resolves libraries and
# plugins relative to the executable instead of the build-time QT_DIR.
cat > "$DATA_DIR/qt.conf" <<'EOF'
[Paths]
Prefix = .
Libraries = lib
Plugins = plugins
EOF

# RPATH $ORIGIN/lib lets the dynamic linker find the bundled libraries
# without LD_LIBRARY_PATH hacks or a wrapper script.
patchelf --set-rpath '$ORIGIN/lib' "$DATA_DIR/markdowneditor.bin"
for staged in "$DATA_DIR"/lib/*.so*; do
    [[ -f "$staged" ]] || continue
    patchelf --set-rpath '$ORIGIN' "$staged" 2>/dev/null || true
done
while IFS= read -r plugin; do
    patchelf --set-rpath '$ORIGIN/../../lib' "$plugin" 2>/dev/null || true
done < <(find "$DATA_DIR/plugins" -type f -name '*.so' -print)

info "[5/5] Staging desktop metadata and optional spell-check data..."
if [[ -f "$INSTALLER_DIR/markdowneditor.png" ]]; then
    cp -f "$INSTALLER_DIR/markdowneditor.png" \
        "$DATA_DIR/markdowneditor.png"
elif [[ -f "$REPO_ROOT/packaging/linux/markdowneditor.png" ]]; then
    cp -f "$REPO_ROOT/packaging/linux/markdowneditor.png" \
        "$DATA_DIR/markdowneditor.png"
elif [[ -f "$REPO_ROOT/dist/AppDir/markdowneditor.png" ]]; then
    cp -f "$REPO_ROOT/dist/AppDir/markdowneditor.png" \
        "$DATA_DIR/markdowneditor.png"
else
    die "no application icon found (Installer/Linux/markdowneditor.png)"
fi
if [[ -f "$INSTALLER_DIR/markdowneditor.desktop" ]]; then
    cp -f "$INSTALLER_DIR/markdowneditor.desktop" \
        "$DATA_DIR/markdowneditor.desktop"
elif [[ -f "$REPO_ROOT/dist/AppDir/markdowneditor.desktop" ]]; then
    cp -f "$REPO_ROOT/dist/AppDir/markdowneditor.desktop" \
        "$DATA_DIR/markdowneditor.desktop"
fi

# Optional Sonnet dictionaries from a local prefix (same layout as the
# Windows windeploy step). System dictionaries (/usr/share/hunspell) are
# used when this tree is absent; install hunspell-en-us on the target.
if [[ -d "$REPO_ROOT/build/prefix/bin/data/hunspell" ]]; then
    mkdir -p "$DATA_DIR/data/hunspell"
    cp -f "$REPO_ROOT/build/prefix/bin/data/hunspell/"*.dic \
        "$DATA_DIR/data/hunspell/" 2>/dev/null || true
    cp -f "$REPO_ROOT/build/prefix/bin/data/hunspell/"*.aff \
        "$DATA_DIR/data/hunspell/" 2>/dev/null || true
    info "Sonnet: dictionaries staged from build/prefix."
else
    info "Sonnet: no build/prefix dictionaries; using system hunspell data."
fi
# Optional Sonnet client plugins: prefer a local prefix (Windows layout first,
# then the multi-arch Linux layout), then the system Qt plugin path. Absence
# is fine (spell check stays disabled at runtime).
sonnet_staged=0
for sonnet_plugin_dir in \
    "$REPO_ROOT/build/prefix/lib/plugins/kf6/sonnet" \
    "$REPO_ROOT"/build/prefix/lib/*/plugins/kf6/sonnet; do
    if [[ -d "$sonnet_plugin_dir" ]]; then
        mkdir -p "$DATA_DIR/kf6/sonnet"
        cp -f "$sonnet_plugin_dir/"*.so "$DATA_DIR/kf6/sonnet/" 2>/dev/null || true
        sonnet_staged=1
        break
    fi
done
if (( sonnet_staged == 0 )) && [[ -d /usr/lib/x86_64-linux-gnu/qt6/plugins/kf6/sonnet ]]; then
    if ldd "$SOURCE_BIN" 2>/dev/null | grep -q 'Sonnet'; then
        mkdir -p "$DATA_DIR/kf6/sonnet"
        cp -f /usr/lib/x86_64-linux-gnu/qt6/plugins/kf6/sonnet/*.so \
            "$DATA_DIR/kf6/sonnet/" 2>/dev/null || true
        sonnet_staged=1
    fi
fi
if (( sonnet_staged == 0 )); then
    info "Sonnet: no client plugins found; spell check will stay disabled."
fi

# The staged tree must be self-contained: with lib/ on the search path,
# nothing may be missing and no Qt library may resolve back to the build
# machine's Qt prefix. Check the binary and every staged plugin -- a plugin
# with an unbundled Qt dependency aborts the app at startup with
# "could not load the Qt platform plugin". System libraries (xcb, Wayland,
# GL, ...) are allowed to stay external; they come from Ubuntu and are
# preflighted by the launcher wrapper.
qt_missing=0
while IFS= read -r obj; do
    while IFS= read -r line; do
        lib_name="$(printf '%s\n' "$line" | sed 's/^[[:space:]]*//;s/ =>.*//')"
        case "$lib_name" in
            libQt6*.so*|libicu*.so*|libKF6*.so*|libhunspell*.so*)
                printf 'MISSING BUNDLED LIB: %s needs %s\n' "$obj" "$lib_name" >&2
                qt_missing=1
                ;;
        esac
    done < <(LD_LIBRARY_PATH="$DATA_DIR/lib" ldd "$obj" 2>/dev/null | grep 'not found' || true)
done < <(find "$DATA_DIR" -type f \( -name 'markdowneditor.bin' -o -name '*.so' \) -print)
(( qt_missing == 0 )) || die "staged tree still has unbundled Qt libraries (see above)"
if ldd "$DATA_DIR/markdowneditor.bin" 2>/dev/null \
        | grep -F "$REPO_ROOT/build" >/dev/null 2>&1; then
    die "staged app still depends on a build-tree path; bundling is incomplete"
fi

# Compatibility measurement, not a guess: Ubuntu 24.04 ships glibc 2.39, so
# a staged object needing newer GLIBC symbols will not start there no
# matter which kernel or release built it. (The kernel version is
# irrelevant here -- glibc abstracts it away.)
if command -v objdump >/dev/null 2>&1; then
    max_glibc="$(for obj in "$DATA_DIR/markdowneditor.bin" "$DATA_DIR"/lib/*.so*; do
        [[ -f "$obj" ]] || continue
        objdump -T "$obj" 2>/dev/null | grep -oE 'GLIBC_[0-9.]+'
    done | sort -Vu | tail -n 1)"
    if [[ -n "$max_glibc" ]]; then
        max_ver="${max_glibc#GLIBC_}"
        newest="$(printf '%s\n%s\n' "$max_ver" "2.39" | sort -V | tail -n 1)"
        if [[ "$newest" != "2.39" ]]; then
            info "WARNING: staged tree needs $max_glibc, newer than Ubuntu 24.04's glibc 2.39."
            info "WARNING: For a 24.04-compatible release, build on Ubuntu 24.04 itself."
        else
            info "glibc baseline: max $max_glibc <= 2.39, compatible with Ubuntu 24.04+."
        fi
    fi
fi

info ""
info "Linux app tree staged at:"
info "  $DATA_DIR"
info "  (binary, lib/Qt runtime, plugins, qt.conf, icon, dictionaries)"
info ""
info "Next: $INSTALLER_DIR/make-installer.sh"
