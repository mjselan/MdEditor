#!/usr/bin/env bash
#
# make-installer.sh - create a macOS installer from the staged app bundle.
#
# Run Installer/MacOS/macdeploy.sh first.  By default this script creates a
# conventional drag-and-drop DMG with hdiutil, which works with a stock Qt
# installation.  Pass --ifw (or set INSTALLER_FORMAT=ifw) to use Qt Installer
# Framework when an offline installer wizard is preferred.
#
# Examples:
#   ./Installer/MacOS/make-installer.sh                 # native DMG
#   INSTALLER_FORMAT=ifw ./Installer/MacOS/make-installer.sh
#   ./Installer/MacOS/make-installer.sh --dmg
#
# Optional environment variables:
#   INSTALLER_FORMAT       dmg (default), auto, or ifw
#   IFW_BIN                path to binarycreator (or its containing directory)
#   IFW_VERSION            preferred Qt Installer Framework version (4.11)
#   IFW_SIGN_IDENTITY      code-signing identity for the IFW installer app
#   FREEBUFF_VERSION       override the version in the output filename
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd -P)"
INSTALLER_DIR="$SCRIPT_DIR"
PACKAGES_DIR="$INSTALLER_DIR/packages"
DATA_DIR="$PACKAGES_DIR/com.freebuff.markdowneditor/data"
CONFIG_FILE="$INSTALLER_DIR/config/config.xml"

FORMAT="${INSTALLER_FORMAT:-dmg}"
IFW_VERSION="${IFW_VERSION:-4.11}"

usage() {
    cat <<'EOF'
Usage: make-installer.sh [--ifw|--dmg]

  --ifw   Create a Qt Installer Framework DMG (requires binarycreator).
  --dmg   Create a native drag-and-drop DMG using hdiutil (default).
  --help  Show this help.

Set INSTALLER_FORMAT=ifw to select the IFW wizard without passing --ifw.
The native DMG is the default because it works with a stock Qt installation;
Qt Installer Framework is an optional alternative for an offline wizard.
EOF
}

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

while (($# > 0)); do
    case "$1" in
        --ifw)
            FORMAT="ifw"
            shift
            ;;
        --dmg)
            FORMAT="dmg"
            shift
            ;;
        --format)
            [[ $# -ge 2 ]] || die "--format requires a value"
            FORMAT="$2"
            shift 2
            ;;
        --format=*)
            FORMAT="${1#*=}"
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1 (use --help)"
            ;;
    esac
done

case "$FORMAT" in
    auto|ifw|dmg) ;;
    *) die "INSTALLER_FORMAT must be auto, ifw, or dmg (got: $FORMAT)" ;;
esac

[[ -d "$DATA_DIR" ]] || \
    die "staged package data is missing; run $REPO_ROOT/Installer/MacOS/macdeploy.sh first"

VERSION="${FREEBUFF_VERSION:-}"
if [[ -z "$VERSION" ]]; then
    VERSION="$(awk '/^[[:space:]]*VERSION[[:space:]]+[0-9]/ { print $2; exit }' "$REPO_ROOT/CMakeLists.txt")"
fi
[[ -n "$VERSION" ]] || VERSION="1.0.0"

APP_PATH=""
for candidate in "$DATA_DIR"/*.app; do
    if [[ -d "$candidate" ]]; then
        APP_PATH="$candidate"
        break
    fi
done
[[ -n "$APP_PATH" ]] || \
    die "no .app bundle found in $DATA_DIR; run Installer/MacOS/macdeploy.sh first"
APP_EXECUTABLE="$(app_executable "$APP_PATH" || true)"
[[ -n "$APP_EXECUTABLE" ]] || \
    die "staged app is missing its Contents/MacOS executable: $APP_PATH"

# A deployed app must not retain a build-machine or package-manager path in
# its main executable.  This catches an accidentally linked non-Qt library
# (for example a system Sonnet) before it is hidden inside a DMG.
if command -v otool >/dev/null 2>&1; then
    # otool's first line is the inspected file path, not a dependency.
    dependencies="$(otool -L "$APP_EXECUTABLE" 2>/dev/null | sed '1d' || true)"
    for forbidden_path in "$REPO_ROOT" "/Users/" "/opt/" "/usr/local/"; do
        if printf '%s\n' "$dependencies" | grep -F "$forbidden_path" >/dev/null 2>&1; then
            die "staged app still depends on a build-machine path ($forbidden_path); rebuild without that dependency or deploy it into the bundle"
        fi
    done
fi
if command -v plutil >/dev/null 2>&1; then
    plutil -lint "$APP_PATH/Contents/Info.plist" >/dev/null || \
        die "staged app has an invalid Info.plist"
fi
if command -v codesign >/dev/null 2>&1; then
    codesign --verify --deep --strict "$APP_PATH" >/dev/null 2>&1 || \
        die "staged app is not code-signed; rerun macdeploy.sh"
fi

find_binarycreator() {
    local candidate root

    if [[ -n "${IFW_BIN:-}" ]]; then
        if [[ -x "$IFW_BIN" && ! -d "$IFW_BIN" ]]; then
            printf '%s\n' "$IFW_BIN"
            return
        fi
        if [[ -d "$IFW_BIN" && -x "$IFW_BIN/binarycreator" ]]; then
            printf '%s\n' "$IFW_BIN/binarycreator"
            return
        fi
        return 1
    fi

    if command -v binarycreator >/dev/null 2>&1; then
        command -v binarycreator
        return
    fi

    # Qt online installations put the framework in a sibling Tools directory.
    local roots=()
    if [[ -n "${QT_DIR:-}" ]]; then
        roots+=("$QT_DIR/../../Tools/QtInstallerFramework")
        roots+=("$QT_DIR/../Tools/QtInstallerFramework")
        roots+=("$QT_DIR/Tools/QtInstallerFramework")
    fi
    if [[ -n "${HOME:-}" ]]; then
        roots+=("$HOME/Qt/Tools/QtInstallerFramework")
    fi

    for root in "${roots[@]}"; do
        [[ -d "$root" ]] || continue
        # Prefer the version used by the Windows scripts, but allow a
        # different installed 4.x framework when that is all the user has.
        candidate="$(find "$root" -type f -path "*/$IFW_VERSION/bin/binarycreator" -perm -111 -print 2>/dev/null | sort | tail -n 1)"
        if [[ -z "$candidate" ]]; then
            candidate="$(find "$root" -type f -path '*/bin/binarycreator' -perm -111 -print 2>/dev/null | sort | tail -n 1)"
        fi
        if [[ -n "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return
        fi
    done

    return 1
}

file_size() {
    # BSD stat is used by macOS; wc is a portable fallback for unusual shells.
    stat -f '%z' "$1" 2>/dev/null || wc -c < "$1"
}

STAGE_DIR=""

cleanup() {
    if [[ -n "$STAGE_DIR" && -d "$STAGE_DIR" ]]; then
        rm -rf "$STAGE_DIR"
    fi
}
trap cleanup EXIT

make_native_dmg() {
    command -v hdiutil >/dev/null 2>&1 || \
        die "hdiutil is required for the native DMG format"
    command -v ditto >/dev/null 2>&1 || \
        die "ditto is required for the native DMG format"

    STAGE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/freebuff-markdown-editor.XXXXXX")"
    local app_name output
    app_name="$(basename -- "$APP_PATH")"
    output="$INSTALLER_DIR/FreebuffMarkdownEditor-$VERSION.dmg"

    # Keep the app relocatable and use the usual Applications drag target.
    ditto "$APP_PATH" "$STAGE_DIR/$app_name"
    ln -s /Applications "$STAGE_DIR/Applications"
    rm -f "$output"
    hdiutil create \
        -volname "Freebuff Markdown Editor" \
        -srcfolder "$STAGE_DIR" \
        -format UDZO \
        -ov \
        "$output"

    [[ -f "$output" ]] || die "hdiutil reported success but $output is missing"
    hdiutil verify "$output" >/dev/null || die "hdiutil verification failed for $output"
    info "Created $output ($(file_size "$output") bytes)"
    info "Mount the DMG and drag Freebuff Markdown Editor to Applications."
}

make_ifw_dmg() {
    [[ -f "$CONFIG_FILE" ]] || die "installer configuration is missing: $CONFIG_FILE"

    local binarycreator ifw_dir output
    binarycreator="$(find_binarycreator || true)"
    [[ -n "$binarycreator" ]] || \
        die "Qt Installer Framework binarycreator was not found; install Qt Installer Framework 4.x or use --dmg"
    ifw_dir="$(cd -- "$(dirname -- "$binarycreator")" && pwd -P)"
    output="$INSTALLER_DIR/FreebuffMarkdownEditor-$VERSION-offline.dmg"

    local args=(
        --config "$CONFIG_FILE"
        --packages "$PACKAGES_DIR"
        --offline-only
    )
    if [[ -x "$ifw_dir/installerbase" ]]; then
        args+=(--template "$ifw_dir/installerbase")
    fi
    if [[ -n "${IFW_SIGN_IDENTITY:-}" ]]; then
        args+=(--sign "$IFW_SIGN_IDENTITY")
    fi

    rm -f "$output"
    "$binarycreator" "${args[@]}" "$output"
    [[ -f "$output" ]] || die "binarycreator reported success but $output is missing"
    command -v hdiutil >/dev/null 2>&1 || \
        die "hdiutil is required to verify the IFW DMG"
    hdiutil verify "$output" >/dev/null || \
        die "hdiutil verification failed for $output"
    info "Created $output ($(file_size "$output") bytes)"
    info "The DMG contains the Qt Installer Framework wizard for Freebuff Markdown Editor."
}

if [[ "$FORMAT" == "auto" ]]; then
    if BINARYCREATOR="$(find_binarycreator || true)" && [[ -n "$BINARYCREATOR" ]]; then
        FORMAT="ifw"
        info "Qt Installer Framework found; creating an IFW DMG."
    else
        FORMAT="dmg"
        info "Qt Installer Framework not found; creating a native drag-and-drop DMG."
    fi
fi

case "$FORMAT" in
    ifw)
        make_ifw_dmg
        ;;
    dmg)
        make_native_dmg
        ;;
esac
