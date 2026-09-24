// SPDX-License-Identifier: GPL-3.0-or-later
#include "markdowneditor.h"

#include <QFontDatabase>
#include <QImage>
#include <QGuiApplication>
#include <QEvent>
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
    // System monospace instead of a bare "monospace" string, which may not
    // resolve on minimal installs.
    return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
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

void MarkdownEditor::setGutterColors(const QColor &line, const QColor &active)
{
    if (line.isValid())
        m_lineNumberColor = line;
    if (active.isValid())
        m_lineNumberActiveColor = active;
    updateLineNumberAreaWidth(0);
    viewport()->update();
}

void MarkdownEditor::changeEvent(QEvent *event)
{
    QPlainTextEdit::changeEvent(event);
    // The current-line tint is a palette blend, so recompute it when the
    // palette changes. Gutter colors follow the syntax scheme (see
    // setGutterColors) and are deliberately left alone here.
    if (event->type() == QEvent::PaletteChange) {
        m_currentLineColor = blend(palette().color(QPalette::Base),
                                   palette().color(QPalette::Highlight), 0.12);
        highlightCurrentLine();
    }
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

        // Toggle off when the selection is exactly marker + clean inner
        // text + marker. The inner text must not start/end with the marker
        // itself: "**bold**" with "*" unwraps nothing (it is a longer run,
        // handled below), while "**bold**" with "**" unwraps to "bold".
        if (selected.size() >= 2 * marker.size() && selected.startsWith(marker)
            && selected.endsWith(marker)) {
            const QString inner =
                selected.mid(marker.size(), selected.size() - 2 * marker.size());
            if (!inner.startsWith(marker) && !inner.endsWith(marker)) {
                cursor.setPosition(start);
                cursor.setPosition(end, QTextCursor::KeepAnchor);
                cursor.insertText(inner);
                // Leave the unwrapped text selected, mirroring the wrap path.
                cursor.setPosition(start);
                cursor.setPosition(start + inner.size(), QTextCursor::KeepAnchor);
                cursor.endEditBlock();
                setTextCursor(cursor);
                return;
            }
        }

        // Toggle off when the markers around the selection form an exact
        // run. Counting the run (instead of matching one character) keeps
        // Ctrl+I on "**bold**" from stripping it down to "*bold*": a run
        // of 2 with marker "*" wraps to "***bold***" instead.
        QTextDocument *doc = document();
        const auto runLength = [&](int pos, bool before) {
            int n = 0;
            const QChar want = marker.at(0);
            if (before) {
                for (int p = pos - 1; p >= 0 && doc->characterAt(p) == want; --p)
                    ++n;
            } else {
                for (int p = pos;; ++p) {
                    const QChar c = doc->characterAt(p);
                    if (c.isNull() || c != want)
                        break;
                    ++n;
                }
            }
            return n;
        };
        if (runLength(start, true) == marker.size()
            && runLength(end, false) == marker.size()) {
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
    const int firstPos = first.position();
    const int lastContentEnd = last.position() + last.length() - 1; // exclusive

    cursor.beginEditBlock();
    cursor.setPosition(firstPos);
    cursor.setPosition(lastContentEnd, QTextCursor::KeepAnchor);
    cursor.insertText(replaced);
    cursor.endEditBlock();

    // Reselect the transformed range so repeated invocations (e.g. Tab)
    // keep operating on the same lines instead of collapsing to one. The
    // block handle is stale after the split above; use the saved offset.
    cursor.setPosition(firstPos);
    cursor.setPosition(firstPos + replaced.size(), QTextCursor::KeepAnchor);
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
    static const QRegularExpression openFence(QStringLiteral("^\\s{0,3}(`{3,}|~{3,})"));
    bool unwrapPair = false;
    if (first == last && !cursor.hasSelection()) {
        // Bare cursor: toggle the enclosing fenced block, if any, instead
        // of nesting a new fence inside it. Replay open/close from the
        // document start (CommonMark: same marker, close length >= open).
        QTextBlock open;
        QChar openMarker;
        int openLength = 0;
        bool inside = false;
        for (QTextBlock b = document()->firstBlock(); b.isValid() && b != first;
             b = b.next()) {
            if (!inside) {
                const auto om = openFence.match(b.text());
                if (om.hasMatch()) {
                    inside = true;
                    open = b;
                    openMarker = om.captured(1).at(0);
                    openLength = om.captured(1).size();
                }
            } else {
                const auto cm = fence.match(b.text());
                if (cm.hasMatch() && cm.captured(1).at(0) == openMarker
                    && cm.captured(1).size() >= openLength)
                    inside = false;
            }
        }
        const auto firstOpen = openFence.match(first.text());
        if (inside) {
            if (fence.match(first.text()).hasMatch()) {
                // Cursor on the closing fence: unwrap [open, first].
                last = first;
                first = open;
                unwrapPair = true;
            } else {
                // Inside the block: unwrap through its close, if any.
                for (QTextBlock b = first.next(); b.isValid(); b = b.next()) {
                    const auto cm = fence.match(b.text());
                    if (cm.hasMatch() && cm.captured(1).at(0) == openMarker
                        && cm.captured(1).size() >= openLength) {
                        last = b;
                        first = open;
                        unwrapPair = true;
                        break;
                    }
                }
                // Unclosed fence: fall through to single-line behavior.
            }
        } else if (firstOpen.hasMatch()) {
            // Cursor on an opening fence: unwrap through its close, if any.
            // A lone fence line is left alone (see below).
            for (QTextBlock b = first.next(); b.isValid(); b = b.next()) {
                const auto cm = fence.match(b.text());
                if (cm.hasMatch() && cm.captured(1).at(0) == firstOpen.captured(1).at(0)
                    && cm.captured(1).size() >= firstOpen.captured(1).size()) {
                    last = b;
                    unwrapPair = true;
                    break;
                }
            }
        }
        // Otherwise not in or on a fence: fall through below.
    }
    // A lone fence line (first == last) unwraps to nothing: removing "both"
    // fence ranges from one block would eat the neighboring line, and
    // wrapping it in another fence is never what was asked.
    if (!unwrapPair) {
        if (fence.match(first.text()).hasMatch() && fence.match(last.text()).hasMatch()) {
            if (first == last)
                return;
            unwrapPair = true;
        }
    }
    if (unwrapPair) {
        // Unwrap: remove both fence lines. QTextBlock::length() covers the
        // block separator, which past the last block is not a valid cursor
        // position, so clamp the range.
        const int lastEnd = qMin(last.position() + last.length(),
                                 document()->characterCount() - 1);
        cursor.beginEditBlock();
        cursor.setPosition(last.position());
        cursor.setPosition(lastEnd, QTextCursor::KeepAnchor);
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
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && !event->modifiers()
        && !textCursor().hasSelection()) {
        static const QRegularExpression listItem(
            QStringLiteral("^(\\s*)([-*+]|(\\d{1,9})([.)]))(\\s+)(.*)$"));
        const auto m = listItem.match(textCursor().block().text(),
                                      0, QRegularExpression::NormalMatch,
                                      QRegularExpression::AnchoredMatchOption);
        if (m.hasMatch()) {
            const QString indent = m.captured(1);
            const QString marker = m.captured(2);
            const QString content = m.captured(6);

            QTextCursor cursor = textCursor();
            // At the very start of the line there is nothing to continue:
            // a plain split avoids producing "- - foo".
            if (cursor.position() == cursor.block().position()) {
                QPlainTextEdit::keyPressEvent(event);
                event->accept();
                return;
            }
            // Everything below runs in one edit block so a single undo
            // reverts the whole continuation (no base-class insertion).
            cursor.beginEditBlock();
            if (content.isEmpty()) {
                // Empty item: exit the list by dropping this line's marker,
                // then end the line.
                const int blockStart = cursor.block().position();
                const int blockEnd = blockStart + cursor.block().length() - 1;
                cursor.setPosition(blockStart);
                cursor.setPosition(blockEnd, QTextCursor::KeepAnchor);
                cursor.insertText(indent);
                cursor.movePosition(QTextCursor::EndOfBlock);
                cursor.insertBlock();
            } else {
                // Bullets repeat verbatim; numbers increment, keeping the
                // original delimiter (groups 3/4 only match on numbers, so
                // a bare toInt() on the marker — which includes "1." — can
                // never misfire here).
                QString next = marker;
                if (!m.captured(3).isEmpty()) {
                    bool ok = false;
                    const int number = m.captured(3).toInt(&ok);
                    if (ok)
                        next = QString::number(number + 1) + m.captured(4);
                }
                cursor.insertBlock();
                cursor.insertText(indent + next + QStringLiteral(" "));
            }
            cursor.endEditBlock();
            setTextCursor(cursor);
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
    // Office suites and browsers often put both text and an image on the
    // clipboard (e.g. a copied table plus its screenshot). Prefer the text:
    // the image path only runs when there is no usable text.
    const bool hasText = source && source->hasText()
        && !source->text().trimmed().isEmpty();
    if (source && source->hasImage() && !hasText) {
        const QImage image = qvariant_cast<QImage>(source->imageData());
        if (!image.isNull()) {
            emit imagePasted(image); // insertion completed by insertMarkdownImage()
            return;
        }
    }
    QPlainTextEdit::insertFromMimeData(source);
}

bool MarkdownEditor::canInsertFromMimeData(const QMimeData *source) const
{
    // URL drops bubble up to the main window (file open) instead of landing
    // as text in the editor.
    if (source && source->hasUrls())
        return false;
    return QPlainTextEdit::canInsertFromMimeData(source);
}

void MarkdownEditor::insertMarkdownImage(const QString &markdownText)
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.insertText(markdownText);
    cursor.endEditBlock();
}
