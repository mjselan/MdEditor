#!/usr/bin/env bash
#
# macdeploy.sh - build and stage a self-contained macOS application bundle.
#
# The script is the macOS counterpart of Installer/Windows/windeploy.bat:
#   1. configure and build the Release CMake preset;
#   2. turn the executable into a real .app bundle (see CMakeLists.txt);
#   3. copy the bundle into the Qt Installer Framework package data directory;
#   4. run Qt's macdeployqt to add Qt frameworks, plugins, and translations.
#
# Usage:
#   QT_DIR=/path/to/Qt/6.x/macos ./Installer/MacOS/macdeploy.sh
#
# QT_DIR may be omitted when qmake6/qmake is on PATH.  Set
# MARKDOWNEDITOR_PRESET to use a different CMake preset (the default is release).
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd -P)"
INSTALLER_DIR="$SCRIPT_DIR"
PKG_DIR="$INSTALLER_DIR/packages"
DATA_DIR="$PKG_DIR/com.mdeditor.markdowneditor/data"
PRESET="${MARKDOWNEDITOR_PRESET:-release}"
BUILD_DIR="$REPO_ROOT/build/$PRESET"
APP_BUNDLE_NAME="MarkdownEditor.app"

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

info() {
    printf '%s\n' "$*"
}

app_executable() {
    local app_path="$1"
    local bundle_name
    bundle_name="$(basename -- "$app_path")"
    bundle_name="${bundle_name%.app}"

    if [[ -x "$app_path/Contents/MacOS/$bundle_name" ]]; then
        printf '%s\n' "$app_path/Contents/MacOS/$bundle_name"
    elif [[ -x "$app_path/Contents/MacOS/markdowneditor" ]]; then
        # Compatibility with a custom CMake target that keeps the old output
        # name inside the bundle.
        printf '%s\n' "$app_path/Contents/MacOS/markdowneditor"
    else
        local candidate
        for candidate in "$app_path/Contents/MacOS/"*; do
            if [[ -f "$candidate" && -x "$candidate" ]]; then
                printf '%s\n' "$candidate"
                return
            fi
        done
        return 1
    fi
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

    if command -v brew >/dev/null 2>&1; then
        brew --prefix qt 2>/dev/null || true
    fi
}

find_cmake() {
    if command -v cmake >/dev/null 2>&1; then
        command -v cmake
        return
    fi

    # Qt installations commonly ship CMake under Tools, but it is not on the
    # user's PATH.  This keeps the script usable with the same Qt kit that
    # supplied QT_DIR.
    if [[ -n "${QT_DIR:-}" ]]; then
        local candidate
        for candidate in \
            "$QT_DIR/../../Tools/CMake/CMake.app/Contents/bin/cmake" \
            "$QT_DIR/../Tools/CMake/CMake.app/Contents/bin/cmake" \
            "$QT_DIR/bin/cmake"; do
            if [[ -x "$candidate" ]]; then
                printf '%s\n' "$candidate"
                return
            fi
        done
    fi

    return 1
}

find_macdeployqt() {
    if [[ -n "${MACDEPLOYQT:-}" ]]; then
        [[ -x "$MACDEPLOYQT" ]] || die "MACDEPLOYQT is not executable: $MACDEPLOYQT"
        printf '%s\n' "$MACDEPLOYQT"
        return
    fi

    local candidate
    if [[ -n "${QT_DIR:-}" ]]; then
        for candidate in "$QT_DIR/bin/macdeployqt6" "$QT_DIR/bin/macdeployqt" \
                         "$QT_DIR/macdeployqt6" "$QT_DIR/macdeployqt"; do
            if [[ -x "$candidate" ]]; then
                printf '%s\n' "$candidate"
                return
            fi
        done
    fi

    for candidate in macdeployqt6 macdeployqt; do
        if command -v "$candidate" >/dev/null 2>&1; then
            command -v "$candidate"
            return
        fi
    done

    return 1
}

[[ "$(uname -s)" == "Darwin" ]] || die "macdeploy.sh must be run on macOS"

QT_DIR="$(find_qt_dir)"
[[ -n "$QT_DIR" ]] || die "Qt was not found; set QT_DIR to a Qt 6 macOS kit"
export QT_DIR

# CMakePresets.json uses Ninja.  Prefer the system binary, but also support
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
MACDEPLOYQT="$(find_macdeployqt || true)"
[[ -n "$MACDEPLOYQT" ]] || die "macdeployqt6 was not found under QT_DIR/bin or on PATH"

info "[1/4] Building the $PRESET preset..."
"$CMAKE" --preset "$PRESET"
"$CMAKE" --build --preset "$PRESET" --target markdowneditor

SOURCE_APP="$BUILD_DIR/$APP_BUNDLE_NAME"
if [[ ! -d "$SOURCE_APP" ]]; then
    # Keep the script tolerant of a deliberately customized OUTPUT_NAME while
    # still failing early if CMake did not produce a bundle at all.
    SOURCE_APP="$(find "$BUILD_DIR" -maxdepth 1 -type d -name '*.app' -print -quit 2>/dev/null || true)"
fi
[[ -n "$SOURCE_APP" && -d "$SOURCE_APP" ]] || \
    die "Release build did not produce an .app bundle in $BUILD_DIR"
SOURCE_EXECUTABLE="$(app_executable "$SOURCE_APP" || true)"
[[ -n "$SOURCE_EXECUTABLE" ]] || \
    die "app bundle is missing its Contents/MacOS executable: $SOURCE_APP"

info "[2/4] Cleaning the Installer Framework package data directory..."
rm -rf "$DATA_DIR"
mkdir -p "$DATA_DIR"

info "[3/4] Copying the application bundle..."
STAGED_APP="$DATA_DIR/$(basename -- "$SOURCE_APP")"
ditto "$SOURCE_APP" "$STAGED_APP"
[[ -d "$STAGED_APP" ]] || die "failed to stage $STAGED_APP"

info "[4/4] Deploying Qt frameworks and plugins with $MACDEPLOYQT..."
DEPLOY_OPTIONS=(-always-overwrite)
if [[ -n "${CODESIGN_IDENTITY:-}" ]]; then
    # macdeployqt accepts the identity in the form -codesign=<identity>.
    DEPLOY_OPTIONS+=("-codesign=$CODESIGN_IDENTITY")
fi
# macdeployqt expects the app bundle as its first positional argument, followed
# by options (the option spelling shown by its help is single-dash).
"$MACDEPLOYQT" "$STAGED_APP" "${DEPLOY_OPTIONS[@]}"

# A failed deployment can leave a partially written bundle.  Check the
# executable again so make-installer.sh never packages a broken payload.
STAGED_EXECUTABLE="$(app_executable "$STAGED_APP" || true)"
[[ -n "$STAGED_EXECUTABLE" ]] || \
    die "macdeployqt did not leave a runnable app at $STAGED_APP"
[[ -d "$STAGED_APP/Contents/Frameworks/QtCore.framework" ]] || \
    die "macdeployqt did not bundle QtCore.framework in $STAGED_APP"

info ""
info "macOS app bundle staged at:"
info "  $STAGED_APP"
info ""
info "Next: $INSTALLER_DIR/make-installer.sh"
