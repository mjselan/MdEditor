#pragma once

#include <QWidget>

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
    void replaceCurrent(const QString &findText, const QString &replaceText, bool matchCase);
    void replaceAll(const QString &findText, const QString &replaceText, bool matchCase);

public slots:
    void showForFind();
    void showForReplace();
    void setMatchCount(int visibleCount);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void emitFind(bool backward);
    void updateMatchCount();

    QLineEdit *m_find;
    QLineEdit *m_replace;
    QCheckBox *m_matchCase;
    QLabel *m_status;
    QPushButton *m_replaceButton;
    QPushButton *m_replaceAllButton;
};
