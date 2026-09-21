#include "markdownhighlighter.h"

#include <QRegularExpression>
#include <QTextDocument>

namespace {

// A line of inline text with any code spans (or an entire fence line) masked
// out so emphasis rules cannot match across markup boundaries. Masking
// replaces characters one-for-one, so match offsets stay valid.
QString maskCodeSpans(const QString &text, const QVector<QPair<int, int>> &spans)
{
    QString out = text;
    for (const auto &span : spans) {
        const int start = span.first;
        const int len = qMin(span.second, out.size() - start);
        if (start < 0 || len <= 0)
            continue;
        for (int i = start; i < start + len; ++i) {
            QChar &ch = out[i];
            if (ch != QLatin1Char(' '))
                ch = u'\u0001'; // placeholder no rule will match
        }
    }
    return out;
}

// Inline code runs: a backtick run plus a closing run of the same length.
QVector<QPair<int, int>> inlineCodeSpans(const QString &text)
{
    static const QRegularExpression backtick(QStringLiteral("`+"));

    QVector<QPair<int, int>> spans;
    int i = 0;
    while (i < text.size()) {
        const auto open = backtick.match(text, i, QRegularExpression::NormalMatch,
                                         QRegularExpression::AnchoredMatchOption);
        if (!open.hasMatch())
            break;
        const QString ticks = open.captured(0);
        int j = open.capturedStart() + ticks.size();
        int close = -1;
        while (j < text.size()) {
            const auto candidate = backtick.match(text, j, QRegularExpression::NormalMatch,
                                                  QRegularExpression::AnchoredMatchOption);
            if (!candidate.hasMatch())
                break;
            if (candidate.captured(0).size() == ticks.size()) {
                close = candidate.capturedStart() + ticks.size();
                break;
            }
            j = candidate.capturedStart() + candidate.captured(0).size();
        }
        if (close >= 0) {
            spans.append(QPair<int, int>(open.capturedStart(), close - open.capturedStart()));
            i = close;
        } else {
            i = open.capturedStart() + ticks.size();
        }
    }
    return spans;
}

QTextCharFormat makeFormat(const QColor &color, bool bold = false, bool italic = false,
                           bool strike = false, bool underline = false)
{
    QTextCharFormat f;
    if (color.isValid())
        f.setForeground(color);
    if (bold)
        f.setFontWeight(QFont::Bold);
    if (italic)
        f.setFontItalic(true);
    if (strike)
        f.setFontStrikeOut(true);
    if (underline)
        f.setFontUnderline(true);
    return f;
}

} // namespace

class MarkdownHighlighter::Private
{
public:
    QRegularExpression quote{ QStringLiteral("^\\s{0,3}(>{1,})\\s?") };
    QRegularExpression list{ QStringLiteral("^(\\s*)([-*+]|\\d{1,9}[.)])(\\s+)") };
    QRegularExpression bold{ QStringLiteral("\\*\\*(?=\\S)(.+?)(?<=\\S)\\*\\*|__(?=\\S)(.+?)(?<=\\S)__") };
    QRegularExpression italic{ QStringLiteral(
        "(?<![\\*\\w])\\*(?=[^\\s\\*])(.+?)(?<=[^\\s\\*])\\*(?![\\*\\w])|(?<![\\w_])_(?=[^\\s_])(.+?)(?<=[^\\s_])_(?![\\w_])") };
    QRegularExpression strike{ QStringLiteral("~~(?=\\S)(.+?)(?<=\\S)~~") };
    QRegularExpression link{ QStringLiteral(
        "!?\\[([^\\]]*)\\]\\(([^)\\s]+)(?:\\s+\\\"[^\\\"]*\\\")?\\)|<([^\\s>]+)>") };
};

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
    , d(new Private)
{
}

void MarkdownHighlighter::setColors(const QHash<QString, QColor> &colors)
{
    const auto color = [&](const char *key) {
        return colors.value(QLatin1String(key));
    };

    m_formats.clear();
    m_formats.insert(QStringLiteral("heading"), makeFormat(color("heading"), true));
    m_formats.insert(QStringLiteral("bold"), makeFormat(color("bold"), true));
    m_formats.insert(QStringLiteral("italic"), makeFormat(color("italic"), false, true));
    m_formats.insert(QStringLiteral("strikethrough"),
                     makeFormat(color("strikethrough"), false, false, true));
    m_formats.insert(QStringLiteral("code"), makeFormat(color("code")));
    m_formats.insert(QStringLiteral("fencedCode"), makeFormat(color("fencedCode")));
    m_formats.insert(QStringLiteral("link"), makeFormat(color("link"), false, false, false, true));
    m_formats.insert(QStringLiteral("quote"), makeFormat(color("quote"), false, true));
    m_formats.insert(QStringLiteral("listMarker"), makeFormat(color("listMarker"), true));

    rehighlight();
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    const bool inFence = previousBlockState() == StateFencedCode;

    if (inFence) {
        // An identical fence run at the left margin closes the block.
        if (isOpenFence(text)) {
            setCurrentBlockState(StateNone);
        } else {
            setCurrentBlockState(StateFencedCode);
        }
        setFormat(0, text.size(), m_formats.value(QStringLiteral("fencedCode")));
        return;
    }

    if (isOpenFence(text)) {
        setCurrentBlockState(StateFencedCode);
        setFormat(0, text.size(), m_formats.value(QStringLiteral("fencedCode")));
        return;
    }

    setCurrentBlockState(StateNone);

    highlightHeading(text);
    highlightQuote(text);
    highlightList(text);
    highlightInline(text);
}

void MarkdownHighlighter::highlightHeading(const QString &text)
{
    QString title;
    const int level = headingLevel(text, &title);
    if (level <= 0)
        return;
    // Color the whole line (marker + trailing #'s included) for scannability.
    const int length = qMin(text.indexOf(title) + title.size(), text.size());
    setFormat(0, length, m_formats.value(QStringLiteral("heading")));
}

void MarkdownHighlighter::highlightQuote(const QString &text)
{
    const auto m = d->quote.match(text);
    if (!m.hasMatch())
        return;
    setFormat(m.capturedStart(1), m.capturedLength(1), m_formats.value(QStringLiteral("quote")));
}

void MarkdownHighlighter::highlightList(const QString &text)
{
    const auto m = d->list.match(text);
    if (!m.hasMatch())
        return;
    const int start = m.capturedStart(2);
    const int length = m.capturedLength(2) + m.capturedLength(3);
    setFormat(start, length, m_formats.value(QStringLiteral("listMarker")));
}

void MarkdownHighlighter::highlightInline(const QString &text)
{
    const auto codeSpans = inlineCodeSpans(text);

    for (const auto &span : codeSpans)
        setFormat(span.first, span.second, m_formats.value(QStringLiteral("code")));

    const QString masked = maskCodeSpans(text, codeSpans);

    // Emphasis rules operate on the masked text; capture offsets are identical
    // to the original because masking preserves length. For alternatives, the
    // first non-empty group carries the user-visible text.
    const auto apply = [&](const QRegularExpression &re, const QString &format) {
        auto it = re.globalMatch(masked);
        while (it.hasNext()) {
            const auto m = it.next();
            int g = 1;
            while (g <= m.lastCapturedIndex() && m.captured(g).isEmpty())
                ++g;
            if (g > m.lastCapturedIndex())
                continue;
            setFormat(m.capturedStart(g), m.capturedLength(g), m_formats.value(format));
        }
    };

    apply(d->bold, QStringLiteral("bold"));
    apply(d->italic, QStringLiteral("italic"));
    apply(d->strike, QStringLiteral("strikethrough"));

    auto linkIt = d->link.globalMatch(masked);
    while (linkIt.hasNext()) {
        const auto m = linkIt.next();
        setFormat(m.capturedStart(0), m.capturedLength(0), m_formats.value(QStringLiteral("link")));
    }
}

int MarkdownHighlighter::headingLevel(const QString &text, QString *title)
{
    static const QRegularExpression re(QStringLiteral("^\\s{0,3}(#{1,6})(\\s+)(.+?)\\s*#*\\s*$"));
    const auto m = re.match(text);
    if (!m.hasMatch())
        return 0;
    if (title)
        *title = m.captured(3);
    return m.captured(1).size();
}

bool MarkdownHighlighter::isOpenFence(const QString &text, QChar *marker)
{
    static const QRegularExpression re(QStringLiteral("^\\s{0,3}(`{3,}|~{3,})"));
    const auto m = re.match(text);
    if (!m.hasMatch())
        return false;
    if (marker)
        *marker = m.captured(1).at(0);
    return true;
}

QVector<QPair<int, QString>> MarkdownHighlighter::headings() const
{
    QVector<QPair<int, QString>> result;
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        QString title;
        const int level = headingLevel(block.text(), &title);
        if (level > 0)
            result.append({ level, title });
        block = block.next();
    }
    return result;
}
