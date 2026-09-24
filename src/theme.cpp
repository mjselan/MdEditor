// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme.h"

#include <memory>

#include <QGuiApplication>
#include <QJsonObject>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

namespace theme {

namespace {

bool systemIsDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (const auto *hints = QGuiApplication::styleHints()) {
        if (hints->colorScheme() != Qt::ColorScheme::Unknown)
            return hints->colorScheme() == Qt::ColorScheme::Dark;
    }
#endif
    // Fallback heuristic for platforms that do not report a color scheme:
    // a dark window color means the OS palette is dark.
    return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
}

bool isDark(Mode mode)
{
    return mode == Mode::Dark || (mode == Mode::Auto && systemIsDark());
}

struct SyntaxScheme
{
    QColor heading;
    QColor bold;
    QColor italic;
    QColor strikethrough;
    QColor code;
    QColor fencedCode;
    QColor link;
    QColor quote;
    QColor listMarker;
    QColor lineNumber;
    QColor lineNumberActive;
    QColor misspelled;
};

SyntaxScheme lightScheme()
{
    return SyntaxScheme{
        .heading = QColor(0x1a, 0x4f, 0x8b),
        .bold = QColor(0x10, 0x10, 0x10),
        .italic = QColor(0x33, 0x33, 0x44),
        .strikethrough = QColor(0x8a, 0x8a, 0x8a),
        .code = QColor(0xa3, 0x15, 0x15),
        .fencedCode = QColor(0x0b, 0x54, 0x0b),
        .link = QColor(0x0b, 0x5c, 0xad),
        .quote = QColor(0x5a, 0x5a, 0x5a),
        .listMarker = QColor(0x0b, 0x6a, 0x4f),
        .lineNumber = QColor(0xa0, 0xa6, 0xad),
        .lineNumberActive = QColor(0x4a, 0x4f, 0x55),
        .misspelled = QColor(0xcc, 0x29, 0x36),
    };
}

SyntaxScheme darkScheme()
{
    return SyntaxScheme{
        .heading = QColor(0x7c, 0xb2, 0xf0),
        .bold = QColor(0xf2, 0xf2, 0xf2),
        .italic = QColor(0xd8, 0xd8, 0xe0),
        .strikethrough = QColor(0x8a, 0x8a, 0x8a),
        .code = QColor(0xf2, 0x8c, 0x8c),
        .fencedCode = QColor(0x9c, 0xd8, 0x9c),
        .link = QColor(0x74, 0xb6, 0xf2),
        .quote = QColor(0xa8, 0xb0, 0xb8),
        .listMarker = QColor(0x7f, 0xd1, 0xb0),
        .lineNumber = QColor(0x55, 0x5c, 0x63),
        .lineNumberActive = QColor(0xb0, 0xb8, 0xc0),
        .misspelled = QColor(0xf2, 0x6d, 0x76),
    };
}

QHash<QString, QColor> tableFor(const SyntaxScheme &s)
{
    return QHash<QString, QColor>{
        { QStringLiteral("heading"), s.heading },
        { QStringLiteral("bold"), s.bold },
        { QStringLiteral("italic"), s.italic },
        { QStringLiteral("strikethrough"), s.strikethrough },
        { QStringLiteral("code"), s.code },
        { QStringLiteral("fencedCode"), s.fencedCode },
        { QStringLiteral("link"), s.link },
        { QStringLiteral("quote"), s.quote },
        { QStringLiteral("listMarker"), s.listMarker },
        { QStringLiteral("lineNumber"), s.lineNumber },
        { QStringLiteral("lineNumberActive"), s.lineNumberActive },
        { QStringLiteral("misspelled"), s.misspelled },
    };
}

} // namespace

QPalette paletteFor(Mode mode)
{
    // Deterministic base: a default-constructed QPalette copies the current
    // application palette, so it is dark on a dark OS (breaking Light mode)
    // and leaks light OS roles into dark mode. Fusion's standard palette is
    // always light and available wherever QtWidgets is.
    const std::unique_ptr<QStyle> fusion(
        QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette pal = fusion ? fusion->standardPalette() : QPalette();
    if (!isDark(mode))
        return pal;

    pal.setColor(QPalette::Window, QColor(0x1e, 0x21, 0x24));
    pal.setColor(QPalette::WindowText, QColor(0xd6, 0xda, 0xde));
    pal.setColor(QPalette::Base, QColor(0x16, 0x19, 0x1c));
    pal.setColor(QPalette::AlternateBase, QColor(0x1e, 0x21, 0x24));
    pal.setColor(QPalette::ToolTipBase, QColor(0x2a, 0x2e, 0x32));
    pal.setColor(QPalette::ToolTipText, QColor(0xd6, 0xda, 0xde));
    pal.setColor(QPalette::Text, QColor(0xd6, 0xda, 0xde));
    pal.setColor(QPalette::PlaceholderText, QColor(0x7a, 0x82, 0x8a));
    pal.setColor(QPalette::Button, QColor(0x2a, 0x2e, 0x32));
    pal.setColor(QPalette::ButtonText, QColor(0xd6, 0xda, 0xde));
    pal.setColor(QPalette::BrightText, QColor(0xff, 0xff, 0xff));
    pal.setColor(QPalette::Highlight, QColor(0x2f, 0x6f, 0xd0));
    pal.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    pal.setColor(QPalette::Link, QColor(0x74, 0xb6, 0xf2));

    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x6a, 0x72, 0x7a));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6a, 0x72, 0x7a));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6a, 0x72, 0x7a));
    return pal;
}

QHash<QString, QColor> syntaxColors(Mode mode)
{
    return tableFor(isDark(mode) ? darkScheme() : lightScheme());
}

QJsonObject toJson(Mode mode)
{
    QJsonObject json;
    const auto colors = syntaxColors(mode);
    for (auto it = colors.constBegin(); it != colors.constEnd(); ++it)
        json.insert(it.key(), it.value().name(QColor::HexRgb));
    return json;
}

void applyJson(const QJsonObject &json, Mode mode)
{
    Q_UNUSED(json);
    Q_UNUSED(mode);
    // Custom user schemes are not persisted yet; reserved for future use.
}

} // namespace theme
