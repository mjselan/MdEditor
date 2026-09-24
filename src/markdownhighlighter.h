// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

#include <QColor>
#include <QHash>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class QTextDocument;
class SpellChecker;

// Live markdown syntax highlighting. Qt re-highlights only the changed block
// (plus neighbors when block states change), which keeps very large documents
// responsive. Fenced code blocks are tracked with block states so inline rules
// never leak into or out of a fence.
class MarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    // Block states: StateNone, or StateFencedCode combined with the opening
    // fence's marker character and length (CommonMark: a fence is closed by a
    // run of the SAME character with length >= the opening length).
    enum BlockState {
        StateNone = 0,
        StateFencedCode = 1,
    };

    // Details of a fence delimiter line.
    struct FenceInfo
    {
        QChar marker{ '`' };
        int length = 0; // 0 == not a fence
    };

    static int encodeFenceState(const FenceInfo &info);
    static FenceInfo decodeFenceState(int state);

    explicit MarkdownHighlighter(QTextDocument *document);
    // Defined in the .cpp (where Private is complete) to allow unique_ptr.
    ~MarkdownHighlighter() override;

    void setColors(const QHash<QString, QColor> &colors);

    // Attach an optional spell checker. When set and enabled, words it
    // reports as misspelled get the "misspelled" theme underline. Code
    // segments are never spell checked.
    void setSpellChecker(SpellChecker *checker);

    // Headings in document order as [level, text] pairs; used by the outline.
    QVector<QPair<int, QString>> headings() const;

    // Static parsing helpers shared with the outline panel and tests.
    static int headingLevel(const QString &text, QString *title = nullptr);
    static bool isOpenFence(const QString &text, FenceInfo *info = nullptr);

    // Character ranges of a line that must be excluded from spell checking:
    // the whole line for fenced lines, inline code spans, and link targets.
    static QVector<QPair<int, int>> codeSegments(const QString &text);

protected:
    void highlightBlock(const QString &text) override;

private:
    void highlightHeading(const QString &text);
    void highlightQuote(const QString &text);
    void highlightList(const QString &text);
    void highlightInline(const QString &text);
    void spellCheck(const QString &text);

    QHash<QString, QTextCharFormat> m_formats;
    QTextCharFormat m_misspelledFormat;
    SpellChecker *m_spell = nullptr;
    class Private;
    std::unique_ptr<Private> d;
};
