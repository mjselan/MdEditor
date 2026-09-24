// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QListWidget>
#include <QVector>

#include "markdownhighlighter.h"

class QListWidgetItem;

// Clickable table of contents built from document headings. Entries are
// indented by level; activating one jumps the editor to the heading line.
class OutlinePanel : public QListWidget
{
    Q_OBJECT

public:
    explicit OutlinePanel(QWidget *parent = nullptr);

    void setColors(const QColor &window, const QColor &text);

public slots:
    // Replace the outline with the given headings. Positions drive
    // jump/navigation and highlight tracking.
    void setHeadings(const QVector<MarkdownHighlighter::Heading> &headings);

    // Highlight the outline entry closest above the given cursor position.
    void setActiveHeading(int cursorPosition);

signals:
    void headingActivated(int cursorPosition);

private:
    void onItemActivated(QListWidgetItem *item);

    QVector<int> m_positions; // parallel to the item rows
};
