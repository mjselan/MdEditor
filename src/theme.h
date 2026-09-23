// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QHash>
#include <QPalette>

class QJsonObject;
class QString;

namespace theme {

enum class Mode { Auto, Light, Dark };

// Fully populate a QPalette for the requested mode; Auto resolves via the
// current style hints. Safe to call before the QApplication style is final.
QPalette paletteFor(Mode mode);

// Syntax color scheme keyed by logical role ("heading", "bold", "italic",
// "strikethrough", "code", "fencedCode", "link", "quote", "listMarker",
// "lineNumber", "lineNumberActive", "misspelled").
QHash<QString, QColor> syntaxColors(Mode mode);

// Serialization helpers so a future release can persist a custom scheme.
QJsonObject toJson(Mode mode);
void applyJson(const QJsonObject &json, Mode mode);

} // namespace theme
