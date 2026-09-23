// SPDX-License-Identifier: GPL-3.0-or-later
#include "markdowneditor.h"

#include <QFontDatabase>
#include <QImage>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMimeData>
#include <QPaintEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBlock>

namespace {

// Monospace fallback chain; the first installed family wins.
QString editorFontFamily()
{
    const QStringList preferred = {
        QStringLiteral("Cascadia Code"),
        QStringLiteral("JetBrains Mono"),
        QStringLiteral("Consolas"),
        QStringLiteral("Menlo"),
        QStringLiteral("DejaVu Sans Mono"),
    };
    const auto families = QFontDatabase::families();
    for (const QString &candidate : preferred)
        if (families.contains(candidate))
            return candidate;
    return QStringLiteral("monospace");
}

QColor blend(const QColor &a, const QColor &b, qreal ratio)
{
    const qreal r = a.redF() * (1 - ratio) + b.redF() * ratio;
    const qreal g = a.greenF() * (1 - ratio) + b.greenF() * ratio;
    const qreal bl = a.blueF() * (1 - ratio) + b.blueF() * ratio;
    return QColor::fromRgbF(r, g, bl, a.alphaF());
}

} // namespace

// ---------------------------------------------------------------------------
// LineNumberArea
// ---------------------------------------------------------------------------

class MarkdownEditor::LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(MarkdownEditor *editor)
        : QWidget(editor)
        , m_editor(editor)
    {
    }

    QSize sizeHint() const override { return { m_editor->lineNumberAreaWidth(), 0 }; }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        m_editor->paintLineNumberArea(event);
    }

private:
    MarkdownEditor *m_editor;
};

// ---------------------------------------------------------------------------
// Construction / geometry
// ---------------------------------------------------------------------------

MarkdownEditor::MarkdownEditor(QWidget *parent)
    : QPlainTextEdit(parent)
    , m_lineNumberArea(new LineNumberArea(this))
{
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setTabStopDistance(4 * QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' ')));

    QFont font(editorFontFamily(), 11);
    font.setFixedPitch(true);
    setFont(font);
    setTabStopDistance(4 * QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')));

    // Derive gutter/current-line colors from the active palette so theme
    // switches flow through without extra API.
    m_currentLineColor = blend(palette().color(QPalette::Base),
                               palette().color(QPalette::Highlight), 0.12);
    m_lineNumberColor = blend(palette().color(QPalette::Window),
                              palette().color(QPalette::WindowText), 0.45);
    m_lineNumberActiveColor = palette().color(QPalette::WindowText);

    connect(this, &QPlainTextEdit::blockCountChanged, this,
            [this](int) { updateLineNumberAreaWidth(0); });
    connect(this, &QPlainTextEdit::updateRequest, this,
            [this](const QRect &rect, int dy) { updateLineNumberArea(rect, dy); });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this,
            &MarkdownEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

void MarkdownEditor::setEditorFont(const QFont &font)
{
    setFont(font);
    setTabStopDistance(4 * QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')));
    updateLineNumberAreaWidth(0);
}

int MarkdownEditor::lineNumberAreaWidth() const
{
    const int digits = QString::number(qMax(1, blockCount())).size();
    return 14 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void MarkdownEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void MarkdownEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void MarkdownEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height());
}

void MarkdownEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> selections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(m_currentLineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }
    setExtraSelections(selections);
}

void MarkdownEditor::paintLineNumberArea(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), palette().color(QPalette::Window));

    const QTextBlock currentBlock = textCursor().block();
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const QString number = QString::number(blockNumber + 1);
            painter.setPen(blockNumber == currentBlock.blockNumber()
                               ? m_lineNumberActiveColor
                               : m_lineNumberColor);
            painter.drawText(0, top, m_lineNumberArea->width() - 8,
                             fontMetrics().height(), Qt::AlignRight, number);
        }
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        block = block.next();
        ++blockNumber;
    }
}

// ---------------------------------------------------------------------------
// Selection helpers
// ---------------------------------------------------------------------------

void MarkdownEditor::wrapSelection(const QString &marker)
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();

    if (cursor.hasSelection()) {
        const int start = qMin(cursor.anchor(), cursor.position());
        const int end = qMax(cursor.anchor(), cursor.position());
        const QString selected = cursor.selectedText();

        // Toggle off when the markers are inside the selection.
        if (selected.size() >= 2 * marker.size() && selected.startsWith(marker)
            && selected.endsWith(marker)) {
            cursor.setPosition(start + marker.size());
            cursor.setPosition(end - marker.size(), QTextCursor::KeepAnchor);
            cursor.insertText(selected.mid(marker.size(), selected.size() - 2 * marker.size()));
            cursor.endEditBlock();
            setTextCursor(cursor);
            return;
        }

        // Toggle off when the markers sit immediately outside the selection.
        QTextDocument *doc = document();
        const auto adjacent = [&](int pos, bool before) {
            for (int i = 0; i < marker.size(); ++i) {
                const QChar c = doc->characterAt(before ? pos - marker.size() + i : pos + i);
                if (c.isNull() || c != marker.at(i))
                    return false;
            }
            return true;
        };
        if (adjacent(start, true) && adjacent(end, false)) {
            cursor.setPosition(end);
            cursor.setPosition(end + marker.size(), QTextCursor::KeepAnchor);
            cursor.insertText(QString());
            cursor.setPosition(start - marker.size());
            cursor.setPosition(start, QTextCursor::KeepAnchor);
            cursor.insertText(QString());
            cursor.endEditBlock();
            setTextCursor(cursor);
            return;
        }

        cursor.setPosition(end);
        cursor.insertText(marker);
        cursor.setPosition(start);
        cursor.insertText(marker);
        cursor.setPosition(start + marker.size());
        cursor.setPosition(end + marker.size(), QTextCursor::KeepAnchor);
    } else {
        const int pos = cursor.position();
        cursor.insertText(marker + marker);
        cursor.setPosition(pos + marker.size());
    }

    cursor.endEditBlock();
    setTextCursor(cursor);
}

void MarkdownEditor::forEachSelectedLine(
    const std::function<QString(const QString &)> &transform)
{
    QTextCursor cursor = textCursor();
    const int from = cursor.selectionStart();
    const int to = qMax(from, cursor.selectionEnd());

    QTextBlock first = document()->findBlock(from);
    if (!first.isValid())
        return;
    // The last affected block is the one holding the final selected character.
    QTextBlock last = document()->findBlock(qMax(from, to - 1));
    if (!last.isValid())
        last = document()->lastBlock();

    QStringList lines;
    for (QTextBlock b = first; b.isValid() && b.blockNumber() <= last.blockNumber();
         b = b.next()) {
        lines << b.text();
    }

    // The transform operates on one line at a time; joining/rejoining keeps the
    // separator count identical, so document structure cannot shift.
    QStringList replacedLines;
    replacedLines.reserve(lines.size());
    for (const QString &line : lines)
        replacedLines << transform(line);

    const QString joined = lines.join(QLatin1Char('\n'));
    const QString replaced = replacedLines.join(QLatin1Char('\n'));
    if (replaced == joined)
        return;

    // Always replace through the end of the last affected block (exclusive of
    // its separator). Never cap at the selection end: the transformed text
    // spans whole lines, so a shorter range would duplicate the unselected tail
    // of the final line. The per-line transform keeps one separator per line,
    // so the separator itself is never consumed and trailing blocks survive.
    const int lastContentEnd = last.position() + last.length() - 1; // exclusive

    cursor.beginEditBlock();
    cursor.setPosition(first.position());
    cursor.setPosition(lastContentEnd, QTextCursor::KeepAnchor);
    cursor.insertText(replaced);
    cursor.endEditBlock();

    // Leave the cursor at the end of the first affected line.
    const int firstLineLength = replaced.section(QLatin1Char('\n'), 0, 0).size();
    cursor.setPosition(qMin(first.position() + firstLineLength,
                            document()->characterCount() - 1));
    setTextCursor(cursor);
}

// ---------------------------------------------------------------------------
// Formatting commands
// ---------------------------------------------------------------------------

void MarkdownEditor::toggleBold() { wrapSelection(QStringLiteral("**")); }
void MarkdownEditor::toggleItalic() { wrapSelection(QStringLiteral("*")); }
void MarkdownEditor::toggleStrikethrough() { wrapSelection(QStringLiteral("~~")); }
void MarkdownEditor::toggleInlineCode() { wrapSelection(QStringLiteral("`")); }

void MarkdownEditor::toggleFencedCodeBlock()
{
    QTextCursor cursor = textCursor();
    const int from = cursor.selectionStart();
    const int to = qMax(from, cursor.selectionEnd());

    QTextBlock first = document()->findBlock(from);
    if (!first.isValid())
        return;
    QTextBlock last = document()->findBlock(qMax(from, to - 1));
    if (!last.isValid())
        last = document()->lastBlock();

    static const QRegularExpression fence(QStringLiteral("^\\s*(```|~~~)\\s*$"));
    if (fence.match(first.text()).hasMatch() && fence.match(last.text()).hasMatch()) {
        // Unwrap: remove both fence lines.
        cursor.beginEditBlock();
        cursor.setPosition(last.position());
        cursor.setPosition(last.position() + last.length(), QTextCursor::KeepAnchor);
        cursor.insertText(QString());
        cursor.setPosition(first.position());
        cursor.setPosition(first.position() + first.length(), QTextCursor::KeepAnchor);
        cursor.insertText(QString());
        cursor.endEditBlock();
        setTextCursor(cursor);
        return;
    }

    // Insert the closing fence after the last block's content (before its
    // separator), and the opening fence before the first block.
    const int lastContentEnd = last.position() + qMax(0, last.length() - 1);
    cursor.beginEditBlock();
    cursor.setPosition(lastContentEnd);
    cursor.insertText(QStringLiteral("\n```"));
    cursor.setPosition(first.position());
    cursor.insertText(QStringLiteral("```\n"));
    cursor.endEditBlock();

    cursor.setPosition(first.position() + 4); // inside the new fence
    setTextCursor(cursor);
}

void MarkdownEditor::setHeadingLevel(int level)
{
    static const QRegularExpression headingPrefix(QStringLiteral("^\\s{0,3}#{1,6}\\s+"));
    const QString prefix = level > 0 ? QString(level, QLatin1Char('#')) + QLatin1Char(' ')
                                     : QString();
    forEachSelectedLine([&prefix](const QString &line) {
        QString stripped = line;
        stripped.remove(headingPrefix);
        return prefix + stripped;
    });
}

void MarkdownEditor::insertLink()
{
    QTextCursor cursor = textCursor();
    const QString selected = cursor.selectedText();
    const int pos = qMin(cursor.anchor(), cursor.position());

    cursor.beginEditBlock();
    if (selected.isEmpty()) {
        cursor.insertText(QStringLiteral("[](url)"));
        cursor.setPosition(pos + 1);
    } else {
        cursor.insertText(QStringLiteral("[") + selected + QStringLiteral("](url)"));
        cursor.setPosition(pos + selected.size() + 3);
        cursor.setPosition(pos + selected.size() + 6, QTextCursor::KeepAnchor);
    }
    cursor.endEditBlock();
    setTextCursor(cursor);
}

void MarkdownEditor::toggleBlockquote()
{
    static const QRegularExpression quotePrefix(QStringLiteral("^\\s{0,3}>\\s?"));
    forEachSelectedLine([](const QString &line) {
        // Empty lines get the prefix too so the block structure survives the
        // selection-based rewrite (a skipped empty line would vanish).
        if (line.trimmed().isEmpty())
            return QStringLiteral("> ");
        return line.contains(quotePrefix) ? QString(line).remove(quotePrefix)
                                          : QStringLiteral("> ") + line;
    });
}

void MarkdownEditor::toggleBulletList()
{
    static const QRegularExpression bulletPrefix(QStringLiteral("^\\s*[-*+]\\s+"));
    forEachSelectedLine([](const QString &line) {
        return line.contains(bulletPrefix) ? QString(line).remove(bulletPrefix)
                                           : QStringLiteral("- ") + line;
    });
}

void MarkdownEditor::toggleNumberedList()
{
    static const QRegularExpression numberPrefix(QStringLiteral("^\\s*\\d{1,9}[.)]\\s+"));
    int index = 0;
    forEachSelectedLine([&index](const QString &line) {
        ++index;
        if (line.trimmed().isEmpty())
            return QStringLiteral("1. "); // same rationale as toggleBlockquote
        if (line.contains(numberPrefix))
            return QString(line).remove(numberPrefix);
        return QStringLiteral("%1. %2").arg(index).arg(line);
    });
}

// ---------------------------------------------------------------------------
// Key handling
// ---------------------------------------------------------------------------

void MarkdownEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return && !event->modifiers()
        && !textCursor().hasSelection()) {
        static const QRegularExpression listItem(
            QStringLiteral("^(\\s*)([-*+]|\\d{1,9}[.)])(\\s+)(.*)$"));
        const auto m = listItem.match(textCursor().block().text(),
                                      0, QRegularExpression::NormalMatch,
                                      QRegularExpression::AnchoredMatchOption);
        if (m.hasMatch()) {
            const QString indent = m.captured(1);
            const QString marker = m.captured(2);
            const QString content = m.captured(4);

            QPlainTextEdit::keyPressEvent(event); // insert the newline
            if (content.isEmpty()) {
                // Empty item: drop the marker instead of nesting forever.
                QTextCursor cursor = textCursor();
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
                cursor.insertText(indent);
            } else {
                QString next = marker;
                bool ok = false;
                const int number = marker.toInt(&ok);
                if (ok)
                    next = QString::number(number + 1);
                insertPlainText(indent + next + QStringLiteral(" "));
            }
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
        const bool indent = event->key() == Qt::Key_Tab;
        static const QRegularExpression lineStartMarkup(
            QStringLiteral("^\\s*([-*+>|]|\\d{1,9}[.)])\\s+|^\\s{2,}"));
        const QString line = textCursor().block().text();
        const bool markupLine = lineStartMarkup.match(line).hasMatch();

        if (markupLine || textCursor().hasSelection()) {
            forEachSelectedLine([indent](const QString &l) {
                if (indent)
                    return QStringLiteral("    ") + l;
                int cut = 0;
                while (cut < 4 && cut < l.size() && l.at(cut) == QLatin1Char(' '))
                    ++cut;
                return l.mid(cut);
            });
            event->accept();
            return;
        }
        if (indent && !textCursor().hasSelection()) {
            insertPlainText(QStringLiteral("    "));
            event->accept();
            return;
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}

void MarkdownEditor::insertFromMimeData(const QMimeData *source)
{
    if (source && source->hasImage()) {
        const QImage image = qvariant_cast<QImage>(source->imageData());
        if (!image.isNull()) {
            emit imagePasted(image); // insertion completed by insertMarkdownImage()
            return;
        }
    }
    QPlainTextEdit::insertFromMimeData(source);
}

void MarkdownEditor::insertMarkdownImage(const QString &markdownText)
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.insertText(markdownText);
    cursor.endEditBlock();
}
