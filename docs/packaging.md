# Packaging details

This page holds the implementation details behind the installer flows
summarized in the [README](../README.md). Build the installers with the
commands there; come here to understand (or debug) what gets staged.

All three platforms use **Qt Installer Framework 4.11** with the same
component id (`com.mdeditor.markdowneditor`) and separate `packages/.../data`
payload directories, so staging one platform never overwrites another's.
Each installer presents the GPL-3.0 license (from the package `meta/`
directory) for acceptance during setup.

## Windows: what `windeploy.bat` stages

Into `Installer\Windows\packages\com.mdeditor.markdowneditor\data` (gitignored):

| Content | Source |
|---|---|
| `markdowneditor.exe` + `Qt6*.dll`, platform/style/imageformat plugins | Release build + `windeployqt6` |
| MSVC CRT DLLs | VS redist under the Visual Studio install |
| `KF6SonnetCore/Ui.dll`, `kf6\sonnet\sonnet_hunspell.dll` (+ native `sonnet_ispellchecker.dll`) | `build\prefix` Sonnet stack (skipped gracefully if absent → no spell check) |
| `data\hunspell\en_US.{dic,aff}` | `build\prefix` dictionaries |

The layout matches Sonnet's runtime search paths (`<exeDir>/kf6/sonnet` for client plugins, `<exeDir>/data/hunspell` for dictionaries), so the installed app needs no environment variables. No `vc_redist` bootstrapper is needed — the CRT DLLs (`vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll`) are bundled next to the exe.

## macOS: `macdeploy.sh` steps and formats

`macdeploy.sh` performs these steps:

1. Configures and builds the `release` CMake preset.
2. Stages the `MarkdownEditor.app` bundle in
   `Installer/MacOS/packages/com.mdeditor.markdowneditor/data/`.
3. Runs `macdeployqt6` to copy the Qt frameworks, plugins, and platform
   dependencies into the bundle. The resulting app is self-contained and
   does not require a Qt installation on the target Mac. The Windows-only
   `build/prefix` Sonnet layout is not reused on macOS; use a matching native
   Sonnet stack or leave spell check disabled.

`make-installer.sh` has two supported formats:

* **Native drag-and-drop DMG (default)** — use `--dmg` or
  `INSTALLER_FORMAT=dmg`. It uses macOS's built-in `hdiutil` and `ditto`
  tools, with no Qt Installer Framework requirement, and produces
  `Installer/MacOS/MarkdownEditor-<version>.dmg`. The disk image contains the
  app and an `/Applications` symlink.
* **Qt Installer Framework DMG (optional)** — use `--ifw` or
  `INSTALLER_FORMAT=ifw`. It requires Qt Installer Framework 4.x
  (`binarycreator`); the output is
  `Installer/MacOS/MarkdownEditor-<version>-offline.dmg`. Set `IFW_BIN` if
  `binarycreator` is not on `PATH` (the script prefers IFW 4.11; override
  with `IFW_VERSION`), and set `IFW_SIGN_IDENTITY` to sign the generated
  installer application with a Developer ID identity. Without that variable
  the IFW wrapper is unsigned; the native DMG is the convenient local-test
  path. This is an IFW wizard wrapped in a DMG, not an Apple `.pkg`; the app
  is installed under the target directory
  (`/Applications/MarkdownEditor` by
  default).

Both formats contain the same deployed app bundle. Windows and macOS use
separate IFW package data directories, so their staging jobs do not overwrite
one another. Build on the architecture being distributed (for example,
configure an arm64 Qt kit for Apple Silicon); a single build is not
automatically a universal binary. Set
`CMAKE_OSX_DEPLOYMENT_TARGET` to the oldest macOS release you intend to
support when configuring the build. Ad-hoc signing is sufficient for local
testing; public distribution needs a Developer ID signature and notarization
(`CODESIGN_IDENTITY` is passed through to `macdeployqt6` when set).

## Linux: what `linuxdeploy.sh` stages

Into `Installer/Linux/packages/com.mdeditor.markdowneditor/data` (gitignored):

| Content | Source |
|---|---|
| `markdowneditor` (launcher wrapper: system-lib preflight + exec), `markdowneditor.bin` (real binary), `lib/libQt6*.so*` (Core/Gui/Widgets/DBus/PrintSupport + XcbQpa/WaylandClient/WlShellIntegration/OpenGL/Network/Svg plugin support libs), `lib/libicu*`, `lib/libKF6Sonnet*` (when linked) | Release build + `ldd` collection over the binary *and* every staged plugin |
| `plugins/platforms` (`qxcb`, `qwayland`, `qminimal`, `qoffscreen`), `platformthemes`, `xcbglintegrations`, wayland integrations, `imageformats`, `iconengines`, `styles`, `networkinformation`, `tls`, `printsupport`, `generic`, `platforminputcontexts` | `$QT_DIR/plugins` (kiosk backends and `.debug` files skipped) |
| `qt.conf` (`Prefix=.`, `Libraries=lib`, `Plugins=plugins`) + `RPATH=$ORIGIN/lib` via `patchelf` | Generated |
| `markdowneditor.png`, `markdowneditor.desktop` (reference copy) | `Installer/Linux/` |
| `data/hunspell/*.{dic,aff}`, `kf6/sonnet/*.so` | `build/prefix` or system Sonnet plugin path (skipped gracefully if absent → no spell check) |

System libraries (`/lib`, `/usr/lib`: xcb, fontconfig, Mesa/GL, `libssl3`)
are intentionally *not* bundled — Ubuntu 24.04 provides them (see the `apt`
line in the README). System hunspell dictionaries (`/usr/share/hunspell`,
package `hunspell-en-us`) are used when no staged dictionaries exist.

## Linux troubleshooting

`qt.qpa.plugin: Could not load the Qt platform plugin "xcb" ... From 6.5.0,
xcb-cursor0 or libxcb-cursor0 is needed` (or the same for `"wayland"`) means
a target machine is missing the system libraries the bundled platform
plugins need. They are intentionally not bundled — install the `apt` line
from the README on the target machine. The `markdowneditor` launcher wrapper
detects this first and prints the exact fix instead of Qt's message. To
confirm on any machine:

```bash
ldd ~/MarkdownEditor/plugins/platforms/libqxcb.so | grep "not found"
ldd ~/MarkdownEditor/markdowneditor.bin | grep "not found"
```

(empty output = all good).

## Linux compatibility notes (Ubuntu 24.04 onwards)

- Supported arch is **x86_64**. Wayland and X11 sessions are both covered
  via the bundled `qxcb`/`qwayland` platform plugins.
- **Build on the oldest Ubuntu you ship to.** A binary needing glibc symbols
  newer than 24.04's glibc 2.39 will not start on 24.04 (the kernel version
  does not matter for this).
  `linuxdeploy.sh` measures the staged tree's max `GLIBC_*` requirement
  with `objdump` and reports whether it fits the 2.39 baseline, so a future
  toolchain or Qt upgrade that breaks 24.04 compatibility fails visibly
  instead of silently.
- Qt 6.11's online-installer libraries target 22.04+ and run on 24.04+;
  no `LD_LIBRARY_PATH` wrapper is needed thanks to `RPATH` + `qt.conf`.
- Set `MARKDOWNEDITOR_PRESET` to build a different CMake preset and
  `MARKDOWNEDITOR_VERSION` to override the version in the `.run` filename;
  set `IFW_BIN` (or `QT_DIR`) if `binarycreator` is not found automatically.
