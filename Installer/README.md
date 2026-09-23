# Installer layout

Installer sources are separated by target platform:

- `Windows/` contains the MSVC build/deployment scripts, Windows IFW
  configuration, package metadata, and generated payload.
- `MacOS/` contains the macOS `macdeployqt`/DMG scripts, macOS IFW
  configuration, package metadata, and generated payload.

Each platform has its own `packages/.../data` directory, so staging one
platform does not overwrite the other platform's payload. Keep the two
`config.xml` and `package.xml` versions synchronized when bumping a release.
Run these commands from the repository root:

```text
build\devcmd.bat Installer\Windows\windeploy.bat
build\devcmd.bat Installer\Windows\make-installer.bat
./Installer/MacOS/macdeploy.sh
./Installer/MacOS/make-installer.sh
```

Generated installer payloads and disk images are ignored by Git.
