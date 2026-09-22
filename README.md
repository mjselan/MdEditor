# Freebuff Markdown Editor

A cross-platform desktop Markdown editor built with **Qt 6 Widgets (6.5+)** and **CMake**, targeting Windows, macOS, and Linux.

## Features

- **Split view**: raw Markdown editor (left) with live-rendered preview (right), updated on a 250 ms throttle (at most one render per interval, anchored to the first keystroke)
- **Syntax highlighting**: headings, bold, italic, strikethrough, inline code, fenced code blocks (block-state tracked), blockquotes, lists, links — block-based rehighlighting keeps 10k+ line documents responsive
- **Formatting commands**: Bold (`Ctrl+B`), Italic (`Ctrl+I`), strikethrough, headings 1–3 / normal (`Ctrl+1..3`, `Ctrl+0`), link (`Ctrl+K`), inline code, fenced code block, blockquote, bulleted & numbered lists
- **Editor conveniences**: line-number gutter, current-line highlight, list continuation on Enter, Tab/Shift-Tab indent of list/quote lines
- **Preview**: Qt's built-in CommonMark + GFM renderer (tables, task lists, strikethrough, fenced code) in a `QTextBrowser`; external links open in the system browser; synchronized scrolling between panes
- **File management**: New / Open / Save / Save As, Recent Files, drag-and-drop `.md` opening, unsaved-changes indicator (`*` in title) with save prompt on close
- **Autosave**: recovery snapshot every 30 s (plus on exit) under `QStandardPaths::AppLocalDataLocation`; restore prompt on next launch
- **Find & Replace** (`Ctrl+F` / `Ctrl+H`) with match-case and wrap-around
- **Status bar**: word / character / line counts
- **Light/Dark theme** following the OS setting by default, with manual override (persisted)
- **Export** to HTML and PDF
- **Configurable editor font** (`Ctrl++` / `Ctrl+-` / font dialog), persisted via `QSettings`
- **Outline panel** (`Ctrl+Shift+O`): clickable table of contents built from headings
- **Image paste**: clipboard images are saved to `assets/` next to the document and referenced with relative paths
- **Spell check** (optional): enabled automatically when KDE Frameworks **Sonnet** is found at configure time. Misspelled words get a wavy underline that skips inline code and fenced blocks; toggle via Tools → Spell Check. Sonnet's hunspell backend loads dictionaries from `share/hunspell`, `<prefix>/bin/data/hunspell`, or the OS package location.

### Renderer note

The preview uses Qt's built-in Markdown support; fenced code blocks are rendered as plain code blocks (no per-language token coloring). If GitHub-faithful rendering with JS syntax highlighting is ever needed, the plan is a `QWebEngineView` variant with a bundled renderer in a Qt resource — intentionally not used here to keep the dependency footprint small.

## Dependencies

- Qt 6.5 or newer (tested with Qt 6.11): `Core`, `Gui`, `Widgets`, `PrintSupport`, `Test` (tests only)
- CMake 3.21+, Ninja (or any CMake generator), a C++20 compiler
- *Optional:* KDE Frameworks Sonnet 6 (`KF6Sonnet`, components `SonnetUi` + `SonnetCore`) for spell check. The build degrades gracefully: without Sonnet everything works except spell check.

  A self-contained Sonnet stack (ECM + static hunspell + KF6 Sonnet + en_US dictionaries) can be provisioned into `build/prefix` with the helper scripts in `build/`: `deps-ecm.bat`, `hunspell-cl.bat`, `deps-sonnet.bat`, `deps-dicts.bat` (Windows/MSVC; the Linux equivalents are `cmake` invocations of the same projects). CMake auto-detects that prefix and links against it; the runtime DLLs and the hunspell client plugin are copied next to the built binary automatically.

## Build

### Linux

```bash
# Debian/Ubuntu example
sudo apt install qt6-base-dev cmake ninja-build

cmake --preset debug          # or: cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build --preset debug
ctest --preset debug          # run the unit tests
./build/debug/markdowneditor
```

If Qt is in a non-standard location, set `CMAKE_PREFIX_PATH` (the presets read the `QT_DIR` environment variable):

```bash
QT_DIR=~/Qt/6.11.1/gcc_64 cmake --preset debug
```

### Windows (MSVC)

1. Install Qt 6.x (MSVC flavor), Visual Studio 2022+ with C++ tools, and CMake/Ninja (bundled with Qt under `Tools`).
2. From a shell with the MSVC environment initialized:

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set QT_DIR=C:\Qt\6.11.1\msvc2022_64
set PATH=C:\Qt\Tools\Ninja;%PATH%

cmake --preset debug
cmake --build --preset debug
ctest --preset debug
build\debug\markdowneditor.exe
```

The repository also ships two local helper scripts (not tracked, under the gitignored `build/` tree pattern): `build\devcmd.bat` wraps any command with the MSVC + Qt environment, e.g. `build\devcmd.bat cmake --build --preset debug`.

### macOS

```bash
brew install qt cmake ninja
QT_DIR="$(brew --prefix qt)" cmake --preset debug
cmake --build --preset debug
./build/debug/markdowneditor
```

## Project layout

```
CMakeLists.txt          Root build file (AUTOMOC/UIC/RCC enabled, no manual moc calls)
CMakePresets.json       debug / release presets (Ninja, tests on in debug)
src/
  main.cpp              Entry point
  mainwindow.*          Menus, toolbar, status bar, file management, export, autosave, settings
  markdowneditor.*      QPlainTextEdit subclass: formatting commands, gutter, image paste
  markdownhighlighter.* QSyntaxHighlighter subclass: markdown rules + block states
  markdownpreview.*     QTextBrowser subclass: debounced render, scroll sync, relative images
  outlinepanel.*        Heading outline (QDockWidget contents)
  findreplacebar.*      Find & replace bar
  theme.*               Light/dark palettes and syntax colors
  spellchecker.*        Optional Sonnet facade (no-op when compiled without)
tests/
  test_core.cpp         QtTest suite (parsing helpers, block states, theme)
```

## Configuration

Settings persist via `QSettings` (org `Freebuff`, app `MarkdownEditor`): window geometry, theme mode, editor font, outline visibility, recent files. Autosave/recovery files live under `QStandardPaths::AppLocalDataLocation` (e.g. `%LOCALAPPDATA%\Freebuff\MarkdownEditor` on Windows).
