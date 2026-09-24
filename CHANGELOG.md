# Changelog

All notable changes to Markdown Editor are documented here. The format is
loosely based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [1.0.2] - 2026-09-24

### Fixed
- Correct `pip` flag (`--break-system-packages`) for installing
  `aqtinstall` on Ubuntu 24.04, unblocking the Linux release job
  (the `v1.0.1` release run failed on that step).

## [1.0.1] - 2026-09-24

### Added
- `Help → About` dialog showing the application version (from
  `project(VERSION)`) and license.
- CMake `install()` rules for the binary, `.desktop` file, and icon
  (`cmake --install`, distro/Flatpak friendly).
- `MARKDOWNEDITOR_SPELLCHECK` option (`AUTO`/`ON`/`OFF`); `ON` fails the
  configure step when Sonnet is missing instead of silently disabling.
- Ubuntu CI workflow (configure, build, test).
- Windows CI job (MSVC build, tests, offline installer artifact).
- Release workflow: pushing a `v*` tag builds the Windows, Linux, and
  macOS installers and attaches them to the GitHub Release.
- `MARKDOWNEDITOR_VERSION` override for the installer filenames.
- `MARKDOWNEDITOR_SKIP_BUILD` hatch for `windeploy.bat`.
- Windows helper scripts (`tools/windows/`) for the MSVC environment and
  the Sonnet/prefix provisioning, so the documented Windows build works
  from a fresh clone.

### Changed
- Core sources now build once into a `mdeditor_core` static library shared
  by the app and the test suite (was: every file compiled twice).
- Sonnet detection searches an optional local prefix via
  `CMAKE_PREFIX_PATH` ordering instead of a `PATHS`/`NO_DEFAULT_PATH`
  special case.
- Recovery snapshots are written atomically (`QSaveFile`); preview links
  are whitelisted to `http`/`https`/`mailto`.
- README packaging internals moved to `docs/packaging.md`.

### Fixed
- Test suite runs headless (`QT_QPA_PLATFORM=offscreen`), fixing CI and
  server runs without a display.
- Windows installer scripts no longer assume fixed `C:\Qt` paths: Visual
  Studio redist, `windeployqt`, and `binarycreator` are auto-detected with
  `QT_DIR` / `VC_REDIST_ROOT` / `IFW_BIN` overrides.
- Fixed a `cmd.exe` parse error (`. was unexpected`) from a `")."` echo
  inside a `windeploy.bat` block; CI invokes the scripts with `call`.

## [1.0.0] - 2026-09-23

Initial public release.

### Added
- Split-view Markdown editor with live preview, syntax highlighting,
  formatting commands, outline panel, find & replace, themes, HTML/PDF
  export, autosave/recovery, image paste, and optional Sonnet spell check.
- Offline installers via Qt Installer Framework 4.11 for Windows, macOS
  (DMG), and Ubuntu 24.04+ (`.run`), all presenting the GPL-3.0 license.
- Qt unit-test suite (`ctest --preset debug`).
