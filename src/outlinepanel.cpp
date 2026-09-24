// SPDX-License-Identifier: GPL-3.0-or-later
#include "outlinepanel.h"

#include <QFont>
#include <QListWidgetItem>

OutlinePanel::OutlinePanel(QWidget *parent)
    : QListWidget(parent)
{
    setFrameShape(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setWordWrap(false);

    connect(this, &QListWidget::itemActivated, this, &OutlinePanel::onItemActivated);
    connect(this, &QListWidget::itemClicked, this, &OutlinePanel::onItemActivated);
}

void OutlinePanel::setColors(const QColor &window, const QColor &text)
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, window);
    pal.setColor(QPalette::Base, window);
    pal.setColor(QPalette::Text, text);
    setPalette(pal);
}

void OutlinePanel::setHeadings(const QVector<MarkdownHighlighter::Heading> &headings)
{
    const int previousActive = currentRow();

    clear();
    m_positions.clear();

    QFont base = font();
    for (const auto &heading : headings) {
        const int level = heading.level;
        const QString &title = heading.title;

        auto *item = new QListWidgetItem(QString(4 * (level - 1), QLatin1Char(' ')) + title,
                                         this);
        item->setData(Qt::UserRole, level);
        item->setToolTip(title);

        QFont itemFont = base;
        itemFont.setBold(level <= 2);
        itemFont.setPointSizeF(base.pointSizeF() - qMin(2, level - 1) * 0.5);
        item->setFont(itemFont);

        m_positions.append(heading.position);
    }

    if (previousActive >= 0 && previousActive < count())
        setCurrentRow(previousActive);
}

void OutlinePanel::setActiveHeading(int cursorPosition)
{
    int best = -1;
    for (int i = 0; i < m_positions.size(); ++i) {
        if (m_positions[i] >= 0 && m_positions[i] <= cursorPosition)
            best = i;
        else if (m_positions[i] > cursorPosition)
            break;
    }
    if (best >= 0 && best != currentRow()) {
        setCurrentRow(best);
        scrollToItem(item(best), QAbstractItemView::PositionAtCenter);
    }
}

void OutlinePanel::onItemActivated(QListWidgetItem *item)
{
    const int row = indexFromItem(item).row();
    if (row >= 0 && row < m_positions.size() && m_positions[row] >= 0)
        emit headingActivated(m_positions[row]);
}
