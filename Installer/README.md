# Installer layout

Installer sources are separated by target platform:

- `Windows/` contains the MSVC build/deployment scripts, Windows IFW
  configuration, package metadata, and generated payload.
- `MacOS/` contains the macOS `macdeployqt`/DMG scripts, macOS IFW
  configuration, package metadata, and generated payload.
- `Linux/` contains the Linux build/deployment scripts (`linuxdeploy.sh`
  bundles the Qt runtime via `ldd` + `patchelf`, mirroring `macdeploy.sh`),
  Linux IFW configuration, package metadata, and generated payload.

Each platform has its own `packages/.../data` directory, so staging one
platform does not overwrite the other platform's payload. Keep the
`config.xml` and `package.xml` versions synchronized when bumping a release.
Run these commands from the repository root:

```text
tools\windows\devcmd.bat Installer\Windows\windeploy.bat
tools\windows\devcmd.bat Installer\Windows\make-installer.bat
./Installer/MacOS/macdeploy.sh
./Installer/MacOS/make-installer.sh
# Universal Intel + Apple Silicon:
# MARKDOWNEDITOR_PRESET=macos-universal ./Installer/MacOS/macdeploy.sh
# ./Installer/MacOS/make-installer.sh
QT_DIR=$HOME/Qt/6.11.1/gcc_64 ./Installer/Linux/linuxdeploy.sh
./Installer/Linux/make-installer.sh
```

Generated installer payloads and disk images are ignored by Git.

CI builds the Windows installer on every push (`build-test-windows` in
`.github/workflows/ci.yml`) and pushing a `v*` tag builds all three
platform installers and attaches them to the matching GitHub Release
(`.github/workflows/release.yml`); see `docs/packaging.md`.
