#pragma once

#include <QColor>
#include <QHash>
#include <QIcon>
#include <QPixmap>

class QString;

// Painterly icon factory: draws every icon with QPainter at multiple
// resolutions so toolbars stay crisp on fractional-DPI screens. No asset
// files, no icon theme, no QtSvg dependency — the icon *is* the code. All
// glyphs are line-drawn in a single accent color and re-render on theme
// switches via refresh().
namespace appicons {

enum class Icon {
    NewFile,
    Open,
    Save,
    ExportHtml,
    ExportPdf,
    Print,
    Undo,
    Redo,
    Bold,
    Italic,
    Strikethrough,
    H1,
    H2,
    H3,
    PlainParagraph,
    Link,
    InlineCode,
    CodeBlock,
    Blockquote,
    BulletList,
    NumberedList,
};

// The application brand icon: a rounded-square document page bearing a large
// markdown "M" with a bold bar ("M▀" evoking the markdown logo) on a two-tone
// gradient background. Rendered at 16-256 px; used for the window/taskbar icon
// and for generating resources/app.ico.
QIcon applicationIcon();

// Renders one square pixmap of the application icon at the requested size.
QPixmap applicationIconPixmap(int size);

// Returns a multi-resolution icon rendered for the given background color
// (used to derive the accent: dark background -> light glyph, and vice versa).
QIcon makeIcon(Icon which, const QColor &background);

// Re-renders the cache (call when the theme changes).
void refresh(const QColor &background);

// Icon for `which`, from cache when possible.
QIcon iconFor(Icon which);

} // namespace appicons
