// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

#include <QTimer>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

// Hidden bar with find/replace controls for the editor. Standard shortcuts
// (Ctrl+F / Ctrl+H) are owned by MainWindow and call showForFind/showForReplace.
class FindReplaceBar : public QWidget
{
    Q_OBJECT

public:
    explicit FindReplaceBar(QWidget *parent = nullptr);

    void setFindText(const QString &text);

signals:
    void findNext(const QString &text, bool matchCase, bool backward);
    // Emitted while the user types or toggles case; must NOT move the caret.
    // Debounced (see kSearchDebounceMs): rapid keystrokes yield one emission.
    void searchTextChanged(const QString &text, bool matchCase);
    void replaceCurrent(const QString &findText, const QString &replaceText, bool matchCase);
    void replaceAll(const QString &findText, const QString &replaceText, bool matchCase);
    // Emitted when the bar is dismissed with Escape so the owner can return
    // focus to the editor (the bar must not guess the focus target itself).
    void escapePressed();

public slots:
    void showForFind();
    void showForReplace();
    void setMatchCount(int visibleCount);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void emitFind(bool backward);
    void emitSearchChanged();

    static constexpr int kSearchDebounceMs = 150;

    QLineEdit *m_find;
    QLineEdit *m_replace;
    QCheckBox *m_matchCase;
    QLabel *m_status;
    QPushButton *m_replaceButton;
    QPushButton *m_replaceAllButton;
    QTimer m_searchDebounce;
};
