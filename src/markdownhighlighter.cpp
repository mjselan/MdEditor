// SPDX-License-Identifier: GPL-3.0-or-later
#include "markdownhighlighter.h"

#include "spellchecker.h"

#include <QRegularExpression>
#include <QTextBlock>
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
        // Unanchored search: next backtick run at or after i. (An
        // AnchoredMatchOption would only find a span starting exactly at i,
        // silently missing every mid-line code span.)
        const auto open = backtick.match(text, i);
        if (!open.hasMatch())
            break;
        const QString ticks = open.captured(0);
        int j = open.capturedStart() + ticks.size();
        int close = -1;
        while (j < text.size()) {
            const auto candidate = backtick.match(text, j);
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

// Inline links, images, and autolinks. Groups: 1 = visible text,
// 2 = link target, 3 = autolink URL. Shared by the visual link rule and
// codeSegments() (QRegularExpression is implicitly shared, so copies
// are cheap).
const QRegularExpression &linkPattern()
{
    static const QRegularExpression re(QStringLiteral(
        "!?\\[([^\\]]*)\\]\\(([^)\\s]+)(?:\\s+\\\"[^\\\"]*\\\")?\\)|<([^\\s>]+)>"));
    return re;
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
    QRegularExpression link{ linkPattern() };
};

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
    , d(std::make_unique<Private>())
{
    m_misspelledFormat.setFontUnderline(true);
    m_misspelledFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    m_misspelledFormat.setToolTip(QObject::tr("Spelling mistake"));
}

MarkdownHighlighter::~MarkdownHighlighter() = default;

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

    const QColor misspelled = color("misspelled");
    if (misspelled.isValid())
        m_misspelledFormat.setUnderlineColor(misspelled);

    rehighlight();
}

void MarkdownHighlighter::setSpellChecker(SpellChecker *checker)
{
    if (m_spell == checker)
        return;
    // Toggling dictionaries or enabled state must repaint immediately;
    // without this the underlines only appear after each block is edited.
    if (m_spell)
        disconnect(m_spell, &SpellChecker::stateChanged, this,
                   &QSyntaxHighlighter::rehighlight);
    m_spell = checker;
    if (m_spell)
        connect(m_spell, &SpellChecker::stateChanged, this,
                &QSyntaxHighlighter::rehighlight);
    rehighlight();
}

namespace {

// A closing fence must be bare (no info string), use the same marker
// character, and be at least as long as the opening fence (CommonMark).
bool isBareFenceClose(const QString &text, const MarkdownHighlighter::FenceInfo &open)
{
    static const QRegularExpression re(QStringLiteral("^\\s{0,3}(`{3,}|~{3,})\\s*$"));
    const auto m = re.match(text);
    if (!m.hasMatch())
        return false;
    const QString run = m.captured(1);
    return run.at(0) == open.marker && run.size() >= open.length;
}

} // namespace

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    const FenceInfo open = decodeFenceState(previousBlockState());

    if (open.length > 0) {
        // Inside a fenced block: only a matching bare fence closes it.
        setCurrentBlockState(isBareFenceClose(text, open) ? StateNone
                                                          : previousBlockState());
        setFormat(0, text.size(), m_formats.value(QStringLiteral("fencedCode")));
        return;
    }

    FenceInfo line;
    if (isOpenFence(text, &line)) {
        setCurrentBlockState(encodeFenceState(line));
        setFormat(0, text.size(), m_formats.value(QStringLiteral("fencedCode")));
        return;
    }

    setCurrentBlockState(StateNone);

    highlightHeading(text);
    highlightQuote(text);
    highlightList(text);
    highlightInline(text);
    spellCheck(text);
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

QVector<QPair<int, int>> MarkdownHighlighter::codeSegments(const QString &text)
{
    QVector<QPair<int, int>> spans;
    if (isOpenFence(text))
        spans.append({ 0, text.size() });
    spans.append(inlineCodeSpans(text));
    // Link targets and autolinks are never spell checked (only the visible
    // link text is). The pattern is shared with the visual link rule below.
    auto it = linkPattern().globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        if (!m.captured(2).isEmpty())
            spans.append({ m.capturedStart(2), m.capturedLength(2) });
        else if (!m.captured(3).isEmpty())
            spans.append({ m.capturedStart(3), m.capturedLength(3) });
    }
    return spans;
}

void MarkdownHighlighter::spellCheck(const QString &text)
{
    if (!m_spell || !m_spell->enabled())
        return;

    const auto spans = codeSegments(text);
    static const QRegularExpression wordPattern(QStringLiteral("[\\w]+"),
                                                QRegularExpression::UseUnicodePropertiesOption);

    auto it = wordPattern.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        const int start = m.capturedStart();
        const int length = m.capturedLength();
        bool inCode = false;
        for (const auto &span : spans) {
            if (start < span.first + span.second && span.first < start + length) {
                inCode = true;
                break;
            }
        }
        if (!inCode && !m_spell->isWordCorrect(m.captured())) {
            // Merge the underline into the existing per-character formats:
            // a plain setFormat() would replace them and strip colors,
            // bold/heading styles, and link underlines.
            for (int i = start; i < start + length; ++i) {
                QTextCharFormat merged = format(i);
                merged.setFontUnderline(true);
                merged.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
                merged.setUnderlineColor(m_misspelledFormat.underlineColor());
                merged.setToolTip(m_misspelledFormat.toolTip());
                setFormat(i, 1, merged);
            }
        }
    }
}

int MarkdownHighlighter::headingLevel(const QString &text, QString *title)
{
    // A closing hash run needs preceding whitespace (CommonMark), so
    // "## Learning C#" keeps its "#".
    static const QRegularExpression re(
        QStringLiteral("^\\s{0,3}(#{1,6})(\\s+)(.+?)(?:\\s+#+)?\\s*$"));
    const auto m = re.match(text);
    if (!m.hasMatch())
        return 0;
    if (title)
        *title = m.captured(3);
    return m.captured(1).size();
}

bool MarkdownHighlighter::isOpenFence(const QString &text, FenceInfo *info)
{
    static const QRegularExpression re(QStringLiteral("^\\s{0,3}(`{3,}|~{3,})"));
    const auto m = re.match(text);
    if (!m.hasMatch())
        return false;
    if (info) {
        info->marker = m.captured(1).at(0);
        info->length = m.captured(1).size();
    }
    return true;
}

int MarkdownHighlighter::encodeFenceState(const FenceInfo &info)
{
    if (info.length <= 0 || info.length > 63)
        return StateNone;
    const int markerBit = info.marker == QLatin1Char('~') ? 2 : 0;
    return StateFencedCode | markerBit | (info.length << 2);
}

MarkdownHighlighter::FenceInfo MarkdownHighlighter::decodeFenceState(int state)
{
    FenceInfo info;
    // previousBlockState() returns -1 before the first block; -1 & flags is
    // truthy, so guard explicitly to avoid a phantom "inside a fence".
    if (state <= StateNone || !(state & StateFencedCode))
        return info;
    info.length = (state >> 2) & 0x3F;
    info.marker = (state & 2) ? QLatin1Char('~') : QLatin1Char('`');
    return info;
}

QVector<MarkdownHighlighter::Heading> MarkdownHighlighter::headings() const
{
    QVector<Heading> result;
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        // Fenced code content (e.g. "# comment" in Python) is not a heading.
        // userState() is -1 for never-highlighted blocks; those keep the
        // old behavior rather than being skipped blindly.
        const int state = block.userState();
        if (state > StateNone && (state & StateFencedCode)) {
            block = block.next();
            continue;
        }
        QString title;
        const int level = headingLevel(block.text(), &title);
        if (level > 0)
            result.append({ level, title, block.position() });
        block = block.next();
    }
    return result;
}
