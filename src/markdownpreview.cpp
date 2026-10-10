// SPDX-License-Identifier: GPL-3.0-or-later
#include "markdownpreview.h"

#include <QDesktopServices>
#include <QDir>
#include <QImage>
#include <QRegularExpression>
#include <QScrollBar>
#include <QUrl>
#include <QVariant>

#include <algorithm>

namespace {

// Pragmatic LaTeX subset for the preview: inline $...$ and display
// $$...$$ are extracted before CommonMark parsing (so '_' in subscripts
// cannot start emphasis), converted to Unicode + <sub>/<sup> HTML, and
// reinserted after QTextDocument::setMarkdown(). Full LaTeX is out of
// scope; this covers Greek, common operators, \frac, \sqrt, sub/superscripts.
bool isLikelyMath(const QString &expr)
{
    // Avoid treating currency ("$10 to $20") as math.
    if (expr.contains(QLatin1Char('\\')) || expr.contains(QLatin1Char('_'))
        || expr.contains(QLatin1Char('^')))
        return true;
    if (expr.contains(QLatin1Char('=')) || expr.contains(QLatin1Char('+'))
        || expr.contains(QLatin1Char('-')) || expr.contains(QLatin1Char('/'))
        || expr.contains(QLatin1Char('*')) || expr.contains(QLatin1Char('<'))
        || expr.contains(QLatin1Char('>')))
        return true;
    const QString trimmed = expr.trimmed();
    return trimmed.size() == 1 && trimmed.at(0).isLetter();
}

QString formatLatexFormula(const QString &latex, bool isDisplayBlock)
{
    // Escape user content first so "<img>" inside $...$ cannot inject tags;
    // the replacements below then insert the only allowed tags.
    QString expr = latex.trimmed().toHtmlEscaped();

    // Fractions: \frac{a}{b} -> (a / b). No nesting support (documented).
    static const QRegularExpression fracRegex(QStringLiteral(R"(\\frac\{([^}]+)\}\{([^}]+)\})"));
    expr.replace(fracRegex, QStringLiteral("(\\1 / \\2)"));

    // Square roots: \sqrt{x} -> √(x), \sqrt[n]{x} -> ⁿ√(x).
    static const QRegularExpression sqrtNRegex(QStringLiteral(R"(\\sqrt\[([^\]]+)\]\{([^}]+)\})"));
    expr.replace(sqrtNRegex, QStringLiteral("<sup>\\1</sup>√(\\2)"));
    static const QRegularExpression sqrtRegex(QStringLiteral(R"(\\sqrt\{([^}]+)\})"));
    expr.replace(sqrtRegex, QStringLiteral("√(\\1)"));

    // Sub/superscripts whose content is a wrapper command (A_\text{OL}).
    // Must run before the wrappers below strip the braces.
    static const QRegularExpression subCmdBrace(
        QStringLiteral(R"(_\{\\(?:text|mathrm|mathbf|mathit|operatorname)\{([^}]+)\}\})"));
    expr.replace(subCmdBrace, QStringLiteral("<sub>\\1</sub>"));
    static const QRegularExpression subCmd(
        QStringLiteral(R"(_\\(?:text|mathrm|mathbf|mathit|operatorname)\{([^}]+)\})"));
    expr.replace(subCmd, QStringLiteral("<sub>\\1</sub>"));
    static const QRegularExpression supCmdBrace(
        QStringLiteral(R"(\^\{\\(?:text|mathrm|mathbf|mathit|operatorname)\{([^}]+)\}\})"));
    expr.replace(supCmdBrace, QStringLiteral("<sup>\\1</sup>"));
    static const QRegularExpression supCmd(
        QStringLiteral(R"(\^\\(?:text|mathrm|mathbf|mathit|operatorname)\{([^}]+)\})"));
    expr.replace(supCmd, QStringLiteral("<sup>\\1</sup>"));

    // Text wrappers: \text{...}, \mathrm{...}, \operatorname{...}.
    static const QRegularExpression textRegex(
        QStringLiteral(R"(\\(?:text|mathrm|operatorname)\{([^}]+)\})"));
    expr.replace(textRegex, QStringLiteral("<span style='font-style:normal;'>\\1</span>"));

    // Style wrappers.
    static const QRegularExpression bRegex(QStringLiteral(R"(\\mathbf\{([^}]+)\})"));
    expr.replace(bRegex, QStringLiteral("<b>\\1</b>"));
    static const QRegularExpression iRegex(QStringLiteral(R"(\\mathit\{([^}]+)\})"));
    expr.replace(iRegex, QStringLiteral("<i>\\1</i>"));

    // Delimiters: \left( \right) etc.
    expr.replace(QLatin1String("\\left("), QStringLiteral("("));
    expr.replace(QLatin1String("\\right)"), QStringLiteral(")"));
    expr.replace(QLatin1String("\\left["), QStringLiteral("["));
    expr.replace(QLatin1String("\\right]"), QStringLiteral("]"));
    expr.replace(QLatin1String("\\left\\{"), QStringLiteral("{"));
    expr.replace(QLatin1String("\\right\\}"), QStringLiteral("}"));
    expr.replace(QLatin1String("\\left|"), QStringLiteral("|"));
    expr.replace(QLatin1String("\\right|"), QStringLiteral("|"));

    // Spacing commands and thin comma before units ("4.7,\Omega" -> "4.7 \Omega").
    expr.replace(QRegularExpression(QStringLiteral(R"(\\,|\\;|\\!|\\quad|\\qquad)")),
                 QStringLiteral(" "));
    expr.replace(QRegularExpression(QStringLiteral(R"(,(\\[a-zA-Z]))")),
                 QStringLiteral(" \\1"));

    // Token tables are applied longest-key-first so "\leq" is not clobbered
    // by "\le", and "\infty"/"\int" survive the "\in" replacement.
    static const QList<QPair<QString, QString>> tokens = [] {
        QList<QPair<QString, QString>> list = {
            // Greek
            { QStringLiteral("\\Omega"), QStringLiteral("Ω") },
            { QStringLiteral("\\omega"), QStringLiteral("ω") },
            { QStringLiteral("\\beta"), QStringLiteral("β") },
            { QStringLiteral("\\alpha"), QStringLiteral("α") },
            { QStringLiteral("\\gamma"), QStringLiteral("γ") },
            { QStringLiteral("\\Gamma"), QStringLiteral("Γ") },
            { QStringLiteral("\\delta"), QStringLiteral("δ") },
            { QStringLiteral("\\Delta"), QStringLiteral("Δ") },
            { QStringLiteral("\\epsilon"), QStringLiteral("ε") },
            { QStringLiteral("\\theta"), QStringLiteral("θ") },
            { QStringLiteral("\\Theta"), QStringLiteral("Θ") },
            { QStringLiteral("\\lambda"), QStringLiteral("λ") },
            { QStringLiteral("\\Lambda"), QStringLiteral("Λ") },
            { QStringLiteral("\\mu"), QStringLiteral("µ") },
            { QStringLiteral("\\pi"), QStringLiteral("π") },
            { QStringLiteral("\\Pi"), QStringLiteral("Π") },
            { QStringLiteral("\\sigma"), QStringLiteral("σ") },
            { QStringLiteral("\\Sigma"), QStringLiteral("Σ") },
            { QStringLiteral("\\tau"), QStringLiteral("τ") },
            { QStringLiteral("\\phi"), QStringLiteral("φ") },
            { QStringLiteral("\\Phi"), QStringLiteral("Φ") },
            { QStringLiteral("\\psi"), QStringLiteral("ψ") },
            { QStringLiteral("\\Psi"), QStringLiteral("Ψ") },
            { QStringLiteral("\\eta"), QStringLiteral("η") },
            { QStringLiteral("\\zeta"), QStringLiteral("ζ") },
            { QStringLiteral("\\rho"), QStringLiteral("ρ") },
            // Operators and relations
            { QStringLiteral("\\approx"), QStringLiteral("≈") },
            { QStringLiteral("\\sim"), QStringLiteral("∼") },
            { QStringLiteral("\\propto"), QStringLiteral("∝") },
            { QStringLiteral("\\pm"), QStringLiteral("±") },
            { QStringLiteral("\\mp"), QStringLiteral("∓") },
            { QStringLiteral("\\times"), QStringLiteral("×") },
            { QStringLiteral("\\cdot"), QStringLiteral("·") },
            { QStringLiteral("\\div"), QStringLiteral("÷") },
            { QStringLiteral("\\leq"), QStringLiteral("≤") },
            { QStringLiteral("\\le"), QStringLiteral("≤") },
            { QStringLiteral("\\geq"), QStringLiteral("≥") },
            { QStringLiteral("\\ge"), QStringLiteral("≥") },
            { QStringLiteral("\\neq"), QStringLiteral("≠") },
            { QStringLiteral("\\ne"), QStringLiteral("≠") },
            { QStringLiteral("\\equiv"), QStringLiteral("≡") },
            { QStringLiteral("\\infty"), QStringLiteral("∞") },
            { QStringLiteral("\\partial"), QStringLiteral("∂") },
            { QStringLiteral("\\nabla"), QStringLiteral("∇") },
            { QStringLiteral("\\sum"), QStringLiteral("∑") },
            { QStringLiteral("\\prod"), QStringLiteral("∏") },
            { QStringLiteral("\\int"), QStringLiteral("∫") },
            { QStringLiteral("\\degree"), QStringLiteral("°") },
            { QStringLiteral("\\circ"), QStringLiteral("°") },
            { QStringLiteral("\\rightarrow"), QStringLiteral("→") },
            { QStringLiteral("\\leftarrow"), QStringLiteral("←") },
            { QStringLiteral("\\Rightarrow"), QStringLiteral("⇒") },
            { QStringLiteral("\\Leftarrow"), QStringLiteral("⇐") },
            { QStringLiteral("\\leftrightarrow"), QStringLiteral("↔") },
            { QStringLiteral("\\notin"), QStringLiteral("∉") },
            { QStringLiteral("\\to"), QStringLiteral("→") },
            { QStringLiteral("\\in"), QStringLiteral("∈") },
        };
        std::sort(list.begin(), list.end(), [](const auto &a, const auto &b) {
            return a.first.size() > b.first.size();
        });
        return list;
    }();
    for (const auto &pair : tokens)
        expr.replace(pair.first, pair.second);

    // Subscripts: A_{OL} -> A<sub>OL</sub>, h_FE -> h<sub>FE</sub>.
    static const QRegularExpression subBrace(QStringLiteral(R"(_\{([^}]+)\})"));
    expr.replace(subBrace, QStringLiteral("<sub>\\1</sub>"));
    static const QRegularExpression subSingle(QStringLiteral(R"(_([a-zA-Z0-9]+))"));
    expr.replace(subSingle, QStringLiteral("<sub>\\1</sub>"));

    // Superscripts: x^{2} -> x<sup>2</sup>.
    static const QRegularExpression supBrace(QStringLiteral(R"(\^\{([^}]+)\})"));
    expr.replace(supBrace, QStringLiteral("<sup>\\1</sup>"));
    static const QRegularExpression supSingle(QStringLiteral(R"(\^([a-zA-Z0-9]+))"));
    expr.replace(supSingle, QStringLiteral("<sup>\\1</sup>"));

    // Clean up stray braces left over from unknown commands.
    expr.replace(QLatin1Char('{'), QString());
    expr.replace(QLatin1Char('}'), QString());

    if (isDisplayBlock)
        return QStringLiteral("<div align=\"center\" style=\"margin: 10px 0; font-size: 105%;\">%1</div>")
            .arg(expr);
    return QStringLiteral("<span>%1</span>").arg(expr);
}

QString extractRanges(QString text, const QRegularExpression &re, QStringList &extracted,
                      const QString &tagPrefix)
{
    QString result;
    int lastIndex = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        result.append(text.sliced(lastIndex, match.capturedStart() - lastIndex));
        result.append(QStringLiteral("MATHPH%1X%2X").arg(tagPrefix).arg(extracted.size()));
        extracted.append(match.captured(0));
        lastIndex = match.capturedEnd();
    }
    result.append(text.sliced(lastIndex));
    return result;
}

QString renderMarkdownWithMath(const QString &rawMarkdown)
{
    QString text = rawMarkdown;

    // Mask fenced blocks and inline code so "$" inside code is untouched.
    // Unclosed fences are left alone (preview-only edge case).
    static const QRegularExpression codeFence(
        QStringLiteral(R"((?:^|\n)[ \t]*(?:```|~~~)[^\n]*\n[\s\S]*?\n[ \t]*(?:```|~~~))"));
    static const QRegularExpression inlineCode(QStringLiteral(R"(`+[^`\n]+`+)"));

    QStringList codeBlocks;
    QStringList inlineCodes;
    text = extractRanges(text, codeFence, codeBlocks, QStringLiteral("FENCE"));
    text = extractRanges(text, inlineCode, inlineCodes, QStringLiteral("CODE"));

    // Display math $$...$$ first (may span lines), then inline $...$ per line.
    static const QRegularExpression displayMath(QStringLiteral(R"(\${2}([\s\S]+?)\${2})"));
    static const QRegularExpression inlineMath(QStringLiteral(R"(\$([^\$\n]+?)\$)"));

    QStringList displayFormulas;
    QStringList inlineFormulas;

    QString textAfterDisplay;
    int lastIdx = 0;
    auto itDisp = displayMath.globalMatch(text);
    while (itDisp.hasNext()) {
        auto m = itDisp.next();
        textAfterDisplay.append(text.sliced(lastIdx, m.capturedStart() - lastIdx));
        textAfterDisplay.append(QStringLiteral("\n\nMATHPHDISPX%1X\n\n").arg(displayFormulas.size()));
        displayFormulas.append(formatLatexFormula(m.captured(1), true));
        lastIdx = m.capturedEnd();
    }
    textAfterDisplay.append(text.sliced(lastIdx));
    text = textAfterDisplay;

    QString textAfterInline;
    lastIdx = 0;
    auto itInline = inlineMath.globalMatch(text);
    while (itInline.hasNext()) {
        auto m = itInline.next();
        textAfterInline.append(text.sliced(lastIdx, m.capturedStart() - lastIdx));
        if (isLikelyMath(m.captured(1))) {
            textAfterInline.append(QStringLiteral("MATHPHINLINEX%1X").arg(inlineFormulas.size()));
            inlineFormulas.append(formatLatexFormula(m.captured(1), false));
        } else {
            textAfterInline.append(m.captured(0)); // keep raw currency text
        }
        lastIdx = m.capturedEnd();
    }
    textAfterInline.append(text.sliced(lastIdx));
    text = textAfterInline;

    // Restore code before the CommonMark conversion.
    for (int i = 0; i < inlineCodes.size(); ++i)
        text.replace(QStringLiteral("MATHPHCODEX%1X").arg(i), inlineCodes.at(i));
    for (int i = 0; i < codeBlocks.size(); ++i)
        text.replace(QStringLiteral("MATHPHFENCEX%1X").arg(i), codeBlocks.at(i));

    QTextDocument temp;
    temp.setMarkdown(text);
    QString html = temp.toHtml();

    // Display placeholders arrive wrapped as "<p>TAG</p>"; replace the whole
    // paragraph to avoid nesting <p> inside <p>.
    for (int i = 0; i < displayFormulas.size(); ++i) {
        const QString tag = QStringLiteral("MATHPHDISPX%1X").arg(i);
        QRegularExpression para(QStringLiteral("<p[^>]*>\\s*%1\\s*</p>").arg(tag));
        if (html.contains(para))
            html.replace(para, displayFormulas.at(i));
        else
            html.replace(tag, displayFormulas.at(i));
    }
    for (int i = 0; i < inlineFormulas.size(); ++i)
        html.replace(QStringLiteral("MATHPHINLINEX%1X").arg(i), inlineFormulas.at(i));

    return html;
}

} // namespace

MarkdownPreview::MarkdownPreview(QWidget *parent)
    : QTextBrowser(parent)
{
    // Never hand raw document links straight to the OS: only http/https
    // links and mail addresses may leave the app.
    setOpenExternalLinks(false);
    // Internal navigation is handled manually (see openAllowedLink): the
    // default would blank the preview when a target fails to load.
    setOpenLinks(false);
    connect(this, &QTextBrowser::anchorClicked,
            this, &MarkdownPreview::openAllowedLink);
    setFrameShape(QTextBrowser::NoFrame);

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    connect(&m_debounce, &QTimer::timeout, this, &MarkdownPreview::renderNow);

    connect(verticalScrollBar(), &QAbstractSlider::valueChanged, this, [this](int value) {
        if (m_inRender)
            return; // scrollbar resets during setHtml(); must not clobber m_lastRatio
        const int max = verticalScrollBar()->maximum();
        const double ratio = max > 0 ? double(value) / double(max) : 0.0;
        m_lastRatio = ratio;
        emit scrollRatioChanged(ratio);
    });
}

void MarkdownPreview::setSourceDocument(QTextDocument *document)
{
    m_sourceDocument = document;
}

// Throttled re-render: at most one render per interval, anchored to the first
// request in a burst, so preview latency stays bounded without extending the
// window on rapid typing.
void MarkdownPreview::scheduleRender()
{
    if (m_debounce.isActive())
        return; // already pending; keep the original deadline
    m_debounce.start();
}

void MarkdownPreview::setDocumentDirectory(const QString &dir)
{
    m_documentDir = dir;
}

void MarkdownPreview::renderNow()
{
    m_debounce.stop(); // bypass the throttle entirely
    if (m_sourceDocument)
        m_pendingMarkdown = m_sourceDocument->toPlainText(); // pull once, lazily
    // setHtml() resets the scrollbar (firing valueChanged); capture the ratio
    // first and ignore scrollbar signals until the restore is done, or every
    // re-render while scrolled would jump the preview back to the top.
    const double ratio = m_lastRatio;
    m_inRender = true;
    // Math-aware render: LaTeX is converted to Unicode/HTML before Markdown
    // parsing so '$' pairs and '_' subscripts never reach the emphasis rules.
    document()->setHtml(renderMarkdownWithMath(m_pendingMarkdown));
    m_inRender = false;
    m_lastRatio = ratio;
    emit documentRendered();
    // setHtml() resets scrolling; restore the previous position silently.
    setScrollRatio(ratio);
}

void MarkdownPreview::setScrollRatio(double ratio)
{
    // No signal blocking here: the scrollbar's valueChanged is what moves
    // the viewport, so blocking it would freeze the visible content while
    // the scrollbar itself travels. Echo loops are cut with the owner's
    // reentry guard instead (see MainWindow::onEditorScrolled).
    verticalScrollBar()->setValue(qRound(ratio * double(verticalScrollBar()->maximum())));
}

void MarkdownPreview::resetScroll()
{
    m_lastRatio = 0.0;
    setScrollRatio(0.0);
}

QVariant MarkdownPreview::loadResource(int type, const QUrl &name)
{
    if (type == QTextDocument::ImageResource && name.isRelative() && !m_documentDir.isEmpty()) {
        const QString path = m_documentDir + QLatin1Char('/')
                             + name.toString(QUrl::FullyDecoded);
        const QImage image(path);
        if (!image.isNull()) {
            document()->addResource(QTextDocument::ImageResource, name, image);
            return image;
        }
    }
    return QTextBrowser::loadResource(type, name);
}

void MarkdownPreview::openAllowedLink(const QUrl &url)
{
    // Same-document anchors are scrolled to manually (internal navigation
    // is disabled; see the constructor).
    if (url.hasFragment() && url.scheme().isEmpty() && url.path().isEmpty()
        && url.host().isEmpty() && url.query().isEmpty()) {
        scrollToAnchor(url.fragment());
        return;
    }
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https")
        || scheme == QLatin1String("mailto")) {
        QDesktopServices::openUrl(url);
    }
    // Anything else (file://, custom schemes, ...) is silently ignored.
}
