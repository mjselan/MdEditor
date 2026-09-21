#pragma once

#include <QColor>
#include <QHash>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class QTextDocument;

// Live markdown syntax highlighting. Qt re-highlights only the changed block
// (plus neighbors when block states change), which keeps very large documents
// responsive. Fenced code blocks are tracked with block states so inline rules
// never leak into or out of a fence.
class MarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    enum BlockState {
        StateNone = 0,
        StateFencedCode = 1,
    };

    explicit MarkdownHighlighter(QTextDocument *document);

    void setColors(const QHash<QString, QColor> &colors);

    // Headings in document order as [level, text] pairs; used by the outline.
    QVector<QPair<int, QString>> headings() const;

    // Static parsing helpers shared with the outline panel and tests.
    static int headingLevel(const QString &text, QString *title = nullptr);
    static bool isOpenFence(const QString &text, QChar *marker = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    void highlightHeading(const QString &text);
    void highlightQuote(const QString &text);
    void highlightList(const QString &text);
    void highlightInline(const QString &text);

    QHash<QString, QTextCharFormat> m_formats;
    class Private;
    Private *d;
};
