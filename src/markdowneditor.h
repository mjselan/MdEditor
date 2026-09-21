#pragma once

#include <QImage>
#include <QPlainTextEdit>
#include <QTextEdit>

#include <functional>

class QMimeData;
class QPaintEvent;
class QResizeEvent;
class QSize;
class QWidget;

// Raw markdown text pane: line numbers, current-line highlight, markdown
// formatting commands, list continuation on Enter, and clipboard image
// interception. Undo/redo come from QPlainTextEdit.
class MarkdownEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit MarkdownEditor(QWidget *parent = nullptr);

    void setEditorFont(const QFont &font);

    // Formatting commands (menu/toolbar/shortcut entry points).
    void toggleBold();
    void toggleItalic();
    void toggleStrikethrough();
    void toggleInlineCode();
    void toggleFencedCodeBlock();
    void setHeadingLevel(int level); // 0 removes heading markers
    void setHeadingLevel0() { setHeadingLevel(0); }
    void setHeadingLevel1() { setHeadingLevel(1); }
    void setHeadingLevel2() { setHeadingLevel(2); }
    void setHeadingLevel3() { setHeadingLevel(3); }
    void insertLink();
    void toggleBlockquote();
    void toggleBulletList();
    void toggleNumberedList();

signals:
    // Emitted when the clipboard contains an image and the user pastes.
    // The owner decides where to store it; call insertMarkdownImage() with
    // the resulting markdown text.
    void imagePasted(const QImage &image);

public slots:
    void insertMarkdownImage(const QString &markdownText);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void insertFromMimeData(const QMimeData *source) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // Widget drawing the line-number gutter.
    class LineNumberArea;

    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();
    void paintLineNumberArea(QPaintEvent *event);
    int lineNumberAreaWidth() const;

    // Wrap selection (or cursor) in symmetric markers, unwrapping when the
    // markers already surround the selection.
    void wrapSelection(const QString &marker);
    // Apply a per-line transformation over all lines touched by the cursor.
    void forEachSelectedLine(const std::function<QString(const QString &)> &transform);

    LineNumberArea *m_lineNumberArea;
    QColor m_currentLineColor;
    QColor m_lineNumberColor;
    QColor m_lineNumberActiveColor;
};
