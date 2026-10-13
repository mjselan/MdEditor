# Markdown Editor

A lightweight, offline Markdown editor with live preview, built with Qt 6 Widgets and CMake (C++20). Type Markdown on the left, see it rendered on the right. Runs natively on Windows, macOS, and Linux, follows your system light/dark theme, and exports to HTML and PDF. No account, telemetry, or network connection required.

Licensed under [GPL-3.0-or-later](LICENSE.md).

**Download:** Pre-built installers for Windows, macOS, and Linux are published on the [Releases](https://github.com/mjselan/MdEditor/releases) page. Every platform can also be built and packaged from source using the instructions below.

![Markdown Editor](Screenshot.png)

---

## Features

- **Split View with Live Preview:** Raw Markdown editor on the left and rendered preview on the right. Live rendering is throttled to 250 ms so typing remains responsive even on large documents.
- **Syntax Highlighting:** Headings, bold, italic, strikethrough, inline code, fenced code blocks (block-state tracked), blockquotes, lists, and links. Block-based incremental re-highlighting keeps 10,000+ line files fast.
- **Formatting Commands & Shortcuts:**
  - **Text:** Bold (`Ctrl+B`), Italic (`Ctrl+I`), Strikethrough (`Ctrl+Shift+K`)
  - **Headings:** Heading 1–3 (`Ctrl+1` .. `Ctrl+3`), Normal text (`Ctrl+0`)
  - **Elements:** Link (`Ctrl+K`), Inline Code (`Ctrl+Shift+C`), Fenced Code Block (`Ctrl+Shift+B`), Blockquote (`Ctrl+Shift+Q`)
  - **Lists:** Bulleted List (`Ctrl+Shift+U`), Numbered List (`Ctrl+Shift+N`)
- **Editor Conveniences:** Line-number gutter, active line highlighting, automatic list continuation on Enter, and multi-line Tab / Shift-Tab indent/unindent.
- **GFM & Math Preview:** Qt's built-in CommonMark + GFM engine renders tables, task lists, and strikethrough. Inline (`$...$`) and display (`$$...$$`) LaTeX math formulas are converted to clean Unicode and sub/superscripts. Synchronized vertical scrolling connects the editor and preview.
- **File Management & Associations:**
  - Standard New, Open, Save, Save As, and Recent Files menu.
  - Drag-and-drop `.md` file opening.
  - Native file association support: double-clicking or right-clicking `.md` / `.markdown` files ("Open with...") opens them directly in the editor across Windows, macOS, and Linux.
  - Unsaved modifications indicator (`*` in title bar) with safety prompts before closing or opening new documents.
- **Crash Recovery & Autosave:** Silently saves a recovery snapshot every 30 seconds for modified documents to `AppLocalDataLocation`. On startup after an unexpected crash or termination, prompts to restore unsaved work. Snapshots are cleaned up automatically on normal exit.
- **Find & Replace:** Search bar (`Ctrl+F`) and replace bar (`Ctrl+H`) with real-time match counting, case-matching toggle, and wrap-around searching.
- **Export & Print:** One-click export to HTML and PDF, plus system printing with high-contrast stylesheets for dark-mode sessions.
- **Outline Panel (`Ctrl+Shift+O`):** Collapsible sidebar listing document headings with click-to-jump navigation and active heading tracking as you scroll.
- **Image Paste:** Pasting an image from the clipboard automatically creates an `assets/` subfolder adjacent to the document, saves the PNG, and inserts the relative Markdown link (`![image](assets/pasted-....png)`).
- **Themes & UI Styling:** Follows system Light/Dark appearance automatically, with manual overrides in the View menu. Custom multi-resolution toolbar icons are drawn cleanly in code for sharp rendering on high-DPI displays.
- **Optional Spell Check:** Automatically enabled if KDE Frameworks 6 Sonnet (`KF6Sonnet`) is discovered at configure time. Includes misspelled word squiggles, context-menu suggestions, and personal dictionary management.

---

## Dependencies

### Core Requirements
- **C++20 compiler:**
  - Windows: Visual Studio 2026 (v18) or Visual Studio 2022 (v17)
  - Linux: GCC 13+ or Clang 16+
  - macOS: Apple Clang 15+ / Xcode 15+
- **CMake:** 3.21 or newer
- **Build system:** Ninja (recommended) or platform make
- **Qt 6.5+** (tested with Qt 6.11):
  - `Qt6::Core`, `Qt6::Gui`, `Qt6::Widgets`, `Qt6::PrintSupport`
  - `Qt6::Test` (for unit tests)

### Optional Dependencies
- **KF6Sonnet (KDE Frameworks 6):** Enables spell checking (`SonnetCore` + `SonnetUi`). If omitted, spell checking is cleanly compiled out without error.
- **Qt Installer Framework 4.x:** Required only for generating offline installers (`binarycreator`).

---

## Building from Source

This project uses standard `CMakePresets.json` presets (`debug` and `release`).

### Linux
```bash
# Ubuntu / Debian dependencies
sudo apt install qt6-base-dev cmake ninja-build

# Build and run
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/markdowneditor

# Using a standalone Qt installation (e.g. Qt 6.11):
QT_DIR=~/Qt/6.11.1/gcc_64 cmake --preset debug
cmake --build --preset debug
