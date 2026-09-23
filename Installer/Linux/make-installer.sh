#!/usr/bin/env bash
#
# make-installer.sh - create the Linux offline installer from the staged tree.
#
# Run Installer/Linux/linuxdeploy.sh first. This script wraps Qt Installer
# Framework 4.11 (binarycreator) and produces a self-contained offline
# installer for Ubuntu 24.04 and later:
#
#   Installer/Linux/MarkdownEditor-<version>-offline.run
#
# Examples:
#   ./Installer/Linux/make-installer.sh
#   MARKDOWNEDITOR_VERSION=1.0.0 ./Installer/Linux/make-installer.sh
#
# Optional environment variables:
#   IFW_BIN                path to binarycreator (or its containing directory)
#   IFW_VERSION            preferred Qt Installer Framework version (4.11)
#   MARKDOWNEDITOR_VERSION override the version in the output filename
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd -P)"
INSTALLER_DIR="$SCRIPT_DIR"
PACKAGES_DIR="$INSTALLER_DIR/packages"
DATA_DIR="$PACKAGES_DIR/com.mdeditor.markdowneditor/data"
CONFIG_FILE="$INSTALLER_DIR/config/config.xml"

IFW_VERSION="${IFW_VERSION:-4.11}"

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

info() {
    printf '%s\n' "$*"
}

[[ -d "$DATA_DIR" ]] || \
    die "staged package data is missing; run $REPO_ROOT/Installer/Linux/linuxdeploy.sh first"
[[ -f "$CONFIG_FILE" ]] || die "installer configuration is missing: $CONFIG_FILE"

VERSION="${MARKDOWNEDITOR_VERSION:-}"
if [[ -z "$VERSION" ]]; then
    VERSION="$(awk '/^[[:space:]]*VERSION[[:space:]]+[0-9]/ { print $2; exit }' "$REPO_ROOT/CMakeLists.txt")"
fi
[[ -n "$VERSION" ]] || VERSION="1.0.0"

APP_BIN="$DATA_DIR/markdowneditor"
[[ -x "$APP_BIN" ]] || \
    die "staged app binary is missing; run Installer/Linux/linuxdeploy.sh first"
[[ -f "$DATA_DIR/qt.conf" ]] || \
    die "qt.conf is missing in $DATA_DIR; rerun linuxdeploy.sh"
[[ -f "$DATA_DIR/plugins/platforms/libqxcb.so" ]] || \
    die "plugins/platforms/libqxcb.so is missing; rerun linuxdeploy.sh"
[[ -f "$DATA_DIR/markdowneditor.png" ]] || \
    die "markdowneditor.png is missing in $DATA_DIR; rerun linuxdeploy.sh"

# A deployed app must not retain a build-machine path in its main binary.
# This catches an accidentally linked non-bundled library (for example a
# build/prefix Sonnet that was not staged) before it is hidden in the .run.
if command -v ldd >/dev/null 2>&1; then
    dependencies="$(ldd "$APP_BIN" 2>/dev/null || true)"
    if printf '%s\n' "$dependencies" | grep -q 'not found'; then
        printf '%s\n' "$dependencies" >&2
        die "staged app has unresolved libraries; rerun linuxdeploy.sh"
    fi
    for forbidden_path in "$REPO_ROOT/build" "$REPO_ROOT/out"; do
        if printf '%s\n' "$dependencies" | grep -F "$forbidden_path" >/dev/null 2>&1; then
            die "staged app still depends on a build-tree path ($forbidden_path)"
        fi
    done
fi

# The bundled Qt must actually load. The app has no --help/--version flag
# (argv[1] is treated as a file path) and runs its event loop, so launch it
# headless under offscreen and kill it after a few seconds: exit code 124
# from `timeout` means it was still running, i.e. Qt initialized fine. A
# broken RPATH/qt.conf aborts immediately with a platform-plugin error.
if command -v timeout >/dev/null 2>&1; then
    smoke_out="$(LD_LIBRARY_PATH="$DATA_DIR/lib" QT_QPA_PLATFORM=offscreen \
        timeout 10s "$APP_BIN" 2>&1 || true)"
    if printf '%s\n' "$smoke_out" | grep -qiE \
            'qt\.qpa.*(could not|failed|error|fatal)|could not load.*platform|error while loading shared|cannot open shared'; then
        printf '%s\n' "$smoke_out" >&2
        die "staged app cannot start with the bundled Qt (RPATH/qt.conf broken)"
    fi
else
    info "WARNING: 'timeout' not found; skipping the staged-app smoke test."
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
        # Prefer IFW 4.11 (used by the Windows/macOS scripts), but allow a
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
    stat -c '%s' "$1" 2>/dev/null || wc -c < "$1"
}

binarycreator="$(find_binarycreator || true)"
[[ -n "$binarycreator" ]] || \
    die "Qt Installer Framework binarycreator was not found; install Qt Installer Framework 4.x (~/Qt/Tools/QtInstallerFramework/4.11) or set IFW_BIN"
ifw_dir="$(cd -- "$(dirname -- "$binarycreator")" && pwd -P)"
output="$INSTALLER_DIR/MarkdownEditor-$VERSION-offline.run"

args=(
    --config "$CONFIG_FILE"
    --packages "$PACKAGES_DIR"
    --offline-only
)
if [[ -x "$ifw_dir/installerbase" ]]; then
    args+=(--template "$ifw_dir/installerbase")
fi

rm -f "$output"
"$binarycreator" "${args[@]}" "$output"
[[ -f "$output" ]] || die "binarycreator reported success but $output is missing"
chmod +x "$output"

info "Created $output ($(file_size "$output") bytes)"
info ""
info "Install with:"
info "  chmod +x $output"
info "  $output"
info "Default target dir is ~/MarkdownEditor (no sudo needed); pass"
info "--help or use --root /opt/MarkdownEditor for a system-wide install."
