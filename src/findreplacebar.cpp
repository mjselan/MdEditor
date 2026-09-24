// SPDX-License-Identifier: GPL-3.0-or-later
#include "findreplacebar.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

FindReplaceBar::FindReplaceBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("findReplaceBar"));
    hide(); // the bar is opt-in (Ctrl+F / Ctrl+H); never visible at startup

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    m_find = new QLineEdit(this);
    m_find->setPlaceholderText(tr("Find"));
    m_find->setClearButtonEnabled(true);

    m_replace = new QLineEdit(this);
    m_replace->setPlaceholderText(tr("Replace with"));
    m_replace->setClearButtonEnabled(true);

    m_matchCase = new QCheckBox(tr("Match case"), this);

    auto *findNext = new QPushButton(tr("Next"), this);
    auto *findPrev = new QPushButton(tr("Previous"), this);
    m_replaceButton = new QPushButton(tr("Replace"), this);
    m_replaceAllButton = new QPushButton(tr("Replace All"), this);

    m_status = new QLabel(this);
    m_status->setMinimumWidth(90);

    layout->addWidget(m_find, 1);
    layout->addWidget(findNext);
    layout->addWidget(findPrev);
    layout->addWidget(m_matchCase);
    layout->addWidget(m_replace, 1);
    layout->addWidget(m_replaceButton);
    layout->addWidget(m_replaceAllButton);
    layout->addWidget(m_status);

    connect(m_find, &QLineEdit::textChanged, this, [this](const QString &) {
        emitSearchChanged();
    });
    connect(m_matchCase, &QCheckBox::toggled, this, [this](bool) { emitSearchChanged(); });
    connect(m_find, &QLineEdit::returnPressed, this, [this] { emitFind(false); });
    connect(findNext, &QPushButton::clicked, this, [this] { emitFind(false); });
    connect(findPrev, &QPushButton::clicked, this, [this] { emitFind(true); });
    connect(m_replaceButton, &QPushButton::clicked, this, [this] {
        emit replaceCurrent(m_find->text(), m_replace->text(), m_matchCase->isChecked());
    });
    connect(m_replaceAllButton, &QPushButton::clicked, this, [this] {
        emit replaceAll(m_find->text(), m_replace->text(), m_matchCase->isChecked());
    });

    // Counting matches scans the whole document; debounce keystrokes so a
    // burst of typing costs a single scan.
    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(kSearchDebounceMs);
    connect(&m_searchDebounce, &QTimer::timeout, this, [this] {
        emit searchTextChanged(m_find->text(), m_matchCase->isChecked());
    });
}

void FindReplaceBar::setFindText(const QString &text)
{
    if (!text.isEmpty() && text != m_find->text()) {
        m_find->setText(text);
        m_find->selectAll();
    }
}

void FindReplaceBar::showForFind()
{
    m_replace->hide();
    m_replaceButton->hide();
    m_replaceAllButton->hide();
    show();
    raise();
    m_find->setFocus();
    m_find->selectAll();
    emitSearchChanged();
}

void FindReplaceBar::showForReplace()
{
    m_replace->show();
    m_replaceButton->show();
    m_replaceAllButton->show();
    show();
    raise();
    m_find->setFocus();
    m_find->selectAll();
    emitSearchChanged();
}

void FindReplaceBar::setMatchCount(int visibleCount)
{
    if (m_find->text().isEmpty()) {
        m_status->clear();
    } else if (visibleCount <= 0) {
        m_status->setText(tr("Not found"));
        m_status->setStyleSheet(QStringLiteral("color: #e06c75; font-weight: bold;"));
    } else {
        m_status->setText(tr("%1 match(es)").arg(visibleCount));
        m_status->setStyleSheet(QString());
    }
}

void FindReplaceBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        emit escapePressed();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void FindReplaceBar::emitFind(bool backward)
{
    if (m_find->text().isEmpty())
        return;
    emit findNext(m_find->text(), m_matchCase->isChecked(), backward);
}

void FindReplaceBar::emitSearchChanged()
{
    if (m_find->text().isEmpty()) {
        // Clearing the box must clear the status immediately, not after the
        // debounce delay.
        m_searchDebounce.stop();
        emit searchTextChanged(m_find->text(), m_matchCase->isChecked());
        return;
    }
    m_searchDebounce.start(); // restart the quota on every keystroke
}
