You are an expert C++/Qt developer. Build a desktop Markdown editor application using Qt 6 (6.5+) and CMake. Target Windows, macOS, and Linux.

Architecture

Use Qt Widgets (QMainWindow-based), not QML.
Separate concerns into distinct classes:
MainWindow — menus, toolbars, status bar, file management, layout
MarkdownEditor — subclass of QPlainTextEdit for the raw text pane
MarkdownHighlighter — subclass of QSyntaxHighlighter for live syntax coloring (headers, bold, italic, code spans/blocks, links, blockquotes, lists)
MarkdownPreview — rendered output pane
Use a split view (QSplitter) with the raw editor on the left and live-rendered preview on the right, updated on a short debounce timer (~150–300ms) rather than on every keystroke.

Markdown rendering

Prefer Qt's built-in Markdown support: QTextDocument::setMarkdown() / toMarkdown() (CommonMark + GitHub extensions) for the preview pane via QTextBrowser.
If GitHub-flavored features are needed beyond Qt's built-in support (tables, task lists, strikethrough, fenced code with language hints), fall back to QWebEngineView with a bundled JS renderer (e.g., marked.js + highlight.js), loaded from local resources (no network dependency).
Support synchronized scrolling between editor and preview panes.

Core features

New / Open / Save / Save As / Recent Files, with unsaved-changes indicator (* in title) and prompt-to-save on close
Drag-and-drop .md file opening
Toolbar + menu + standard shortcuts: Bold (Ctrl+B), Italic (Ctrl+I), Heading levels, Link (Ctrl+K), Code span/block, Bulleted/numbered list, Blockquote
Find & Replace (Ctrl+F / Ctrl+H)
Word/character/line count in the status bar
Light/Dark theme toggle, respecting the OS setting by default
Export to HTML and PDF (QTextDocument::print() to a QPrinter in PDF mode)
Configurable editor font/size, persisted via QSettings
Basic autosave (timer-based, to a temp/recovery file)

Non-functional requirements

Clean, commented, idiomatic modern C++ (C++20+), using Qt's parent-ownership model for memory management (no manual delete where avoidable)
Responsive with large documents (10k+ lines) — avoid re-parsing/re-highlighting the whole document on every keystroke; use QSyntaxHighlighter's block-based rehighlighting
Proper error handling for file I/O (permissions, missing files, encoding)
No hardcoded absolute paths; use QStandardPaths for recovery/config files

Deliverables

Full CMakeLists.txt (Qt6::Widgets, and Qt6::WebEngineWidgets only if used)
Organized source tree (src/, include/ or paired .h/.cpp)
A README.md with build instructions (CMake configure/build commands) for at least Linux; note any platform-specific steps for Windows/macOS
If any external dependency is used (e.g., a JS markdown/highlight library), bundle it as a Qt resource (.qrc) rather than requiring internet access at runtime

Nice-to-haves (implement if time allows, clearly mark as optional)

Outline/Table-of-Contents side panel generated from headings, clickable to jump
Image paste/embed support
Basic spell-check integration

Ask clarifying questions only if a requirement is genuinely ambiguous; otherwise proceed with sensible defaults and note the assumptions you made.