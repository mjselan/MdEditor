// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "markdowneditor.h"
#include "markdownhighlighter.h"
#include "appicons.h"
#include "findreplacebar.h"
#include "mainwindow.h"
#include "markdownpreview.h"
#include "spellchecker.h"
#include "theme.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextDocument>
#include <QUrl>

namespace {

QVector<QTextBlock> allBlocks(QTextDocument *doc)
{
    QVector<QTextBlock> out;
    for (QTextBlock b = doc->firstBlock(); b.isValid(); b = b.next())
        out.append(b);
    return out;
}

// Exposes protected insertion hooks for behavior tests.
class TestableEditor : public MarkdownEditor
{
public:
    using MarkdownEditor::MarkdownEditor;
    bool testCanInsert(const QMimeData *mime) const { return canInsertFromMimeData(mime); }
    void testInsert(const QMimeData *mime) { insertFromMimeData(mime); }
};

} // namespace

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void headingLevelParsesLevels();
    void headingLevelRejectsNonHeadings();
    void headingLevelStripsClosingHashes();
    void openFenceDetection();
    void fencedCodeBlockStates();
    void fenceDelimitersMustMatch(); // ``` is not closed by ~~~
    void fenceLengthMustMatchOrExceed();
    void forEachSelectedLineKeepsEmptyTrailingBlock();
    void bulletToggleOnSelectionWithEmptyBlock();
    void forEachSelectedLineExactOutputs();
    void forEachSelectedLineSelectionEndsBeforeLineEnd();
    void forEachSelectedLineWholeDocumentMultiLine();
    void indentSelectionExactOutput();
    void numberedListContinuationIncrements();
    void boldToggleUnwrapsSelection();
    void fencedCodeSingleLineUnwrapKeepsNeighbor();
    void headingsSkipFencedCode();
    void wordCountCases();
    void codeSegmentsExcludeLinkTargets();
    void searchChangedIsDebounced();
    void escapeHidesBarAndSignals();
    void themeLightIndependentOfAppPalette();
    void headingLevelKeepsTrailingHash();
    void enterOnEmptyItemExitsList();
    void enterAtLineStartSplitsPlainly();
    void tabTwiceKeepsIndentingSelection();
    void italicToggleOnBoldAddsMarkers();
    void fencedToggleUnwrapsEnclosing();
    void dropUrlsAreRejectedByEditor();
    void imagePastePrefersText();
    void replaceAllClosesEditBlock();
    void spellToggleRehighlights();
    void lineEndingRoundTrip();
    void recoverySkipsCleanDocument();
    void loadRecoveryMarksModified();
    void previewScrollFollowsRatio();
    void previewLinkClickKeepsDocument();
    void findBarHiddenAtStartup();
    void previewRendersDocumentSource();
    void previewThrottleRendersAtMostOncePerInterval();
    void matchCountingDoesNotNeedCaret();
    void spellCheckFlagsMisspellings();
    void spellCheckSkipsCodeSegments();
    void toolbarIconsRenderOnBothBackgrounds();
    void applicationIconRendersAllSizes();
    void themeColorsPresent();
    void palettesDifferByMode();

private:
    static void selectRange(MarkdownEditor &editor, int from, int to);
    static int spellUnderlineCount(const QTextBlock &block);
};

int TestCore::spellUnderlineCount(const QTextBlock &block)
{
    int count = 0;
    for (const auto &range : block.layout()->formats()) {
        if (range.format.underlineStyle() == QTextCharFormat::SpellCheckUnderline)
            ++count;
    }
    return count;
}

void TestCore::headingLevelParsesLevels()
{
    QString title;
    QCOMPARE(MarkdownHighlighter::headingLevel(QStringLiteral("# Title"), &title), 1);
    QCOMPARE(title, QStringLiteral("Title"));
    QCOMPARE(MarkdownHighlighter::headingLevel(QStringLiteral("###### Deep")), 6);
    QCOMPARE(MarkdownHighlighter::headingLevel(QStringLiteral("   ## Indented")), 2);
}

void TestCore::headingLevelRejectsNonHeadings()
{
    QCOMPARE(MarkdownHighlighter::headingLevel(QStringLiteral("plain text")), 0);
    QCOMPARE(MarkdownHighlighter::headingLevel(QStringLiteral("#NoSpace")), 0);
    QCOMPARE(MarkdownHighlighter::headingLevel(QStringLiteral("####### seven")), 0);
    QCOMPARE(MarkdownHighlighter::headingLevel(QString()), 0);
}

void TestCore::headingLevelStripsClosingHashes()
{
    QString title;
    QCOMPARE(MarkdownHighlighter::headingLevel(QStringLiteral("## Closed ##"), &title), 2);
    QCOMPARE(title, QStringLiteral("Closed"));
}

void TestCore::openFenceDetection()
{
    MarkdownHighlighter::FenceInfo info;

    QVERIFY(MarkdownHighlighter::isOpenFence(QStringLiteral("```"), &info));
    QCOMPARE(info.marker, QChar::fromLatin1('`'));
    QCOMPARE(info.length, 3);

    QVERIFY(MarkdownHighlighter::isOpenFence(QStringLiteral("  ~~~~ code"), &info));
    QCOMPARE(info.marker, QChar::fromLatin1('~'));
    QCOMPARE(info.length, 4);

    QVERIFY(!MarkdownHighlighter::isOpenFence(QStringLiteral("``")));
    QVERIFY(!MarkdownHighlighter::isOpenFence(QStringLiteral("text ``` inline")));
}

void TestCore::fencedCodeBlockStates()
{
    QTextDocument doc;
    MarkdownHighlighter highlighter(&doc);
    highlighter.setColors(theme::syntaxColors(theme::Mode::Light));

    doc.setPlainText(QStringLiteral("```cpp\nint x;\n```\nafter"));
    highlighter.rehighlight(); // deterministic block states for the test

    const QVector<QTextBlock> blocks = allBlocks(&doc);
    QCOMPARE(blocks.size(), 4);

    const auto state = [](const QTextBlock &b) {
        return MarkdownHighlighter::decodeFenceState(b.userState());
    };
    // Opening fence and content carry the opening fence parameters.
    QCOMPARE(state(blocks.at(0)).length, 3);
    QCOMPARE(state(blocks.at(1)).length, 3);
    QCOMPARE(state(blocks.at(1)).marker, QChar::fromLatin1('`'));
    // Closing fence and following text are outside the fence.
    QCOMPARE(state(blocks.at(2)).length, 0);
    QCOMPARE(state(blocks.at(3)).length, 0);
}

void TestCore::fenceDelimitersMustMatch()
{
    QTextDocument doc;
    MarkdownHighlighter highlighter(&doc);
    highlighter.setColors(theme::syntaxColors(theme::Mode::Light));

    // A ``` fence is NOT closed by a ~~~ line (CommonMark 4.5).
    doc.setPlainText(QStringLiteral("```cpp\nx = ~~~;\n~~~\nstill code\n```\nafter"));
    highlighter.rehighlight();

    const QVector<QTextBlock> blocks = allBlocks(&doc);
    QCOMPARE(blocks.size(), 6);
    const auto fenced = [](const QTextBlock &b) {
        return MarkdownHighlighter::decodeFenceState(b.userState()).length > 0;
    };
    QVERIFY(fenced(blocks.at(0)));
    QVERIFY(fenced(blocks.at(1)));
    QVERIFY(fenced(blocks.at(2))); // ~~~ line stays inside the ``` fence
    QVERIFY(fenced(blocks.at(3)));
    QVERIFY(!fenced(blocks.at(4))); // matching ``` closes
    QVERIFY(!fenced(blocks.at(5)));
}

void TestCore::fenceLengthMustMatchOrExceed()
{
    QTextDocument doc;
    MarkdownHighlighter highlighter(&doc);
    highlighter.setColors(theme::syntaxColors(theme::Mode::Light));

    // A 5-backtick fence is not closed by a 3-backtick line.
    doc.setPlainText(QStringLiteral("`````md\n```\nstill code\n`````\nafter"));
    highlighter.rehighlight();

    const QVector<QTextBlock> blocks = allBlocks(&doc);
    QCOMPARE(blocks.size(), 5);
    QCOMPARE(MarkdownHighlighter::decodeFenceState(blocks.at(0).userState()).length, 5);
    QCOMPARE(MarkdownHighlighter::decodeFenceState(blocks.at(1).userState()).length, 5);
    QCOMPARE(MarkdownHighlighter::decodeFenceState(blocks.at(2).userState()).length, 5);
    QCOMPARE(MarkdownHighlighter::decodeFenceState(blocks.at(3).userState()).length, 0);
    QCOMPARE(MarkdownHighlighter::decodeFenceState(blocks.at(4).userState()).length, 0);
}

void TestCore::forEachSelectedLineKeepsEmptyTrailingBlock()
{
    // Regression: a selection ending on an empty block used to bleed one
    // character past the block separator, corrupting the document.
    MarkdownEditor editor;
    editor.setPlainText(QStringLiteral("one\n\ntwo"));
    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(0);
    cursor.setPosition(5, QTextCursor::KeepAnchor); // ends inside the empty block
    editor.setTextCursor(cursor);

    editor.toggleBlockquote();
    // Empty lines are quoted like any other line (consistent with list
    // toggles), and the text after the selection end is left untouched.
    QCOMPARE(editor.toPlainText(), QStringLiteral("> one\n> \ntwo"));
    QCOMPARE(editor.document()->blockCount(), 3);
}

void TestCore::bulletToggleOnSelectionWithEmptyBlock()
{
    MarkdownEditor editor;
    editor.setPlainText(QStringLiteral("a\n\nb"));
    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(0);
    cursor.setPosition(4, QTextCursor::KeepAnchor); // full document
    editor.setTextCursor(cursor);

    editor.toggleBulletList();

    QCOMPARE(editor.toPlainText(), QStringLiteral("- a\n- \n- b"));
    QCOMPARE(editor.document()->blockCount(), 3);

    // Toggle off restores the original text.
    cursor = editor.textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
    editor.toggleBulletList();
    QCOMPARE(editor.toPlainText(), QStringLiteral("a\n\nb"));
}

void TestCore::selectRange(MarkdownEditor &editor, int from, int to)
{
    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(from);
    cursor.setPosition(to, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
}

void TestCore::forEachSelectedLineExactOutputs()
{
    // Regression: a selection ending mid-line used to duplicate the unselected
    // tail of the final line, because the transformed text was written into a
    // range capped at the selection end. Every transform must replace through
    // the end of the final affected line and leave "gamma" untouched.
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("alpha\nbeta tail\ngamma"));
        selectRange(editor, 0, 10); // ends inside "beta"
        editor.setHeadingLevel(2);
        QCOMPARE(editor.toPlainText(), QStringLiteral("## alpha\n## beta tail\ngamma"));
    }
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("alpha\nbeta tail\ngamma"));
        selectRange(editor, 0, 10);
        editor.toggleBlockquote();
        QCOMPARE(editor.toPlainText(), QStringLiteral("> alpha\n> beta tail\ngamma"));
    }
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("alpha\nbeta tail\ngamma"));
        selectRange(editor, 0, 10);
        editor.toggleBulletList();
        QCOMPARE(editor.toPlainText(), QStringLiteral("- alpha\n- beta tail\ngamma"));
    }
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("alpha\nbeta tail\ngamma"));
        selectRange(editor, 0, 10);
        editor.toggleNumberedList();
        QCOMPARE(editor.toPlainText(), QStringLiteral("1. alpha\n2. beta tail\ngamma"));
    }
}

void TestCore::forEachSelectedLineSelectionEndsBeforeLineEnd()
{
    // A selection ending on a separator (before the last line's content) must
    // transform only the fully selected line.
    MarkdownEditor editor;
    editor.setPlainText(QStringLiteral("alpha\nbeta tail\ngamma"));
    selectRange(editor, 0, 6); // 'a'..'a' + newline (position 6 ends before "beta")
    editor.toggleBlockquote();
    QCOMPARE(editor.toPlainText(), QStringLiteral("> alpha\nbeta tail\ngamma"));
}

void TestCore::forEachSelectedLineWholeDocumentMultiLine()
{
    MarkdownEditor editor;
    editor.setPlainText(QStringLiteral("one\ntwo\nthree"));
    selectRange(editor, 0, 13); // whole document
    editor.toggleBulletList();
    QCOMPARE(editor.toPlainText(), QStringLiteral("- one\n- two\n- three"));

    // Toggling off with a fresh full-document selection restores the original.
    QTextCursor cursor = editor.textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
    editor.toggleBulletList();
    QCOMPARE(editor.toPlainText(), QStringLiteral("one\ntwo\nthree"));
}

void TestCore::indentSelectionExactOutput()
{
    MarkdownEditor editor;
    editor.setPlainText(QStringLiteral("alpha\nbeta tail\ngamma"));
    selectRange(editor, 0, 10); // ends inside "beta"

    QTest::keyClick(&editor, Qt::Key_Tab);
    QCOMPARE(editor.toPlainText(), QStringLiteral("    alpha\n    beta tail\ngamma"));

    // Full-document selection (characterCount() includes the final separator,
    // so the maximum valid cursor position is one past it).
    QTextCursor cursor = editor.textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
    QTest::keyClick(&editor, Qt::Key_Backtab, Qt::ShiftModifier);
    QCOMPARE(editor.toPlainText(), QStringLiteral("alpha\nbeta tail\ngamma"));
}

void TestCore::numberedListContinuationIncrements()
{
    // Regression: marker.toInt() on "1." always failed, so Enter repeated
    // the same number instead of incrementing it.
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("1. item"));
        QTextCursor cursor = editor.textCursor();
        cursor.movePosition(QTextCursor::End);
        editor.setTextCursor(cursor);
        QTest::keyClick(&editor, Qt::Key_Return);
        QCOMPARE(editor.toPlainText(), QStringLiteral("1. item\n2. "));
    }
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("1) item"));
        QTextCursor cursor = editor.textCursor();
        cursor.movePosition(QTextCursor::End);
        editor.setTextCursor(cursor);
        QTest::keyClick(&editor, Qt::Key_Return);
        QCOMPARE(editor.toPlainText(), QStringLiteral("1) item\n2) "));
    }
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("- item"));
        QTextCursor cursor = editor.textCursor();
        cursor.movePosition(QTextCursor::End);
        editor.setTextCursor(cursor);
        QTest::keyClick(&editor, Qt::Key_Return);
        QCOMPARE(editor.toPlainText(), QStringLiteral("- item\n- "));
    }
    {
        // Numpad Enter behaves like Return, and one undo reverts the
        // whole continuation (single edit block).
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("1. item"));
        QTextCursor cursor = editor.textCursor();
        cursor.movePosition(QTextCursor::End);
        editor.setTextCursor(cursor);
        QTest::keyClick(&editor, Qt::Key_Enter);
        QCOMPARE(editor.toPlainText(), QStringLiteral("1. item\n2. "));
        editor.undo();
        QCOMPARE(editor.toPlainText(), QStringLiteral("1. item"));
    }
}

void TestCore::boldToggleUnwrapsSelection()
{
    // Regression: unwrapping selected "**word**" replaced the inner text
    // with itself and left the markers in place.
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("**word**"));
        selectRange(editor, 0, 8);
        editor.toggleBold();
        QCOMPARE(editor.toPlainText(), QStringLiteral("word"));
    }
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("word"));
        selectRange(editor, 0, 4);
        editor.toggleBold();
        QCOMPARE(editor.toPlainText(), QStringLiteral("**word**"));
    }
}

void TestCore::fencedCodeSingleLineUnwrapKeepsNeighbor()
{
    // Regression: unwrapping when first == last deleted the fence line and
    // then the line that slid into its position.
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("```\ncode"));
        QTextCursor cursor = editor.textCursor();
        cursor.movePosition(QTextCursor::Start);
        editor.setTextCursor(cursor);
        editor.toggleFencedCodeBlock();
        QCOMPARE(editor.toPlainText(), QStringLiteral("```\ncode"));
    }
    // Multi-line unwrap still removes both fences.
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("```\ncode\n```"));
        QTextCursor cursor = editor.textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        editor.setTextCursor(cursor);
        editor.toggleFencedCodeBlock();
        // Both fence lines are gone; the surviving trailing empty block
        // contributes the final newline.
        QCOMPARE(editor.toPlainText(), QStringLiteral("code\n"));
    }
}

void TestCore::headingsSkipFencedCode()
{
    // Regression: "# comment" lines inside fenced code blocks polluted the
    // outline.
    QTextDocument doc;
    MarkdownHighlighter highlighter(&doc);
    highlighter.setColors(theme::syntaxColors(theme::Mode::Light));
    doc.setPlainText(QStringLiteral("# real\n```python\n# not heading\n```\n## also real"));
    highlighter.rehighlight();

    const auto headings = highlighter.headings();
    QCOMPARE(headings.size(), 2);
    QCOMPARE(headings.at(0).level, 1);
    QCOMPARE(headings.at(0).title, QStringLiteral("real"));
    QVERIFY(headings.at(0).position >= 0);
    QCOMPARE(headings.at(1).level, 2);
    QCOMPARE(headings.at(1).title, QStringLiteral("also real"));
    QVERIFY(headings.at(1).position > headings.at(0).position);
}

void TestCore::wordCountCases()
{
    QCOMPARE(MainWindow::wordCount(QString()), 0);
    QCOMPARE(MainWindow::wordCount(QStringLiteral("   ")), 0);
    QCOMPARE(MainWindow::wordCount(QStringLiteral("hello")), 1);
    QCOMPARE(MainWindow::wordCount(QStringLiteral("  a  b\tc\nd ")), 4);
    QCOMPARE(MainWindow::wordCount(QStringLiteral("one\n\ntwo")), 2);
}

void TestCore::codeSegmentsExcludeLinkTargets()
{
    const QString text =
        QStringLiteral("[text](https://example.com/x) and <https://a.b/c>");
    const auto spans = MarkdownHighlighter::codeSegments(text);

    const int targetStart = text.indexOf(QStringLiteral("https://example"));
    QVERIFY(targetStart >= 0);
    QVERIFY(spans.contains(QPair<int, int>(targetStart, 21)));

    const int autoStart = text.indexOf(QStringLiteral("https://a.b"));
    QVERIFY(autoStart >= 0);
    QVERIFY(spans.contains(QPair<int, int>(autoStart, 13)));

    // The visible link text itself stays spell checked.
    const int linkTextPos = 1; // the "text" in "[text](...)"
    for (const auto &span : spans)
        QVERIFY(!(span.first <= linkTextPos && linkTextPos < span.first + span.second));
}

void TestCore::searchChangedIsDebounced()
{
    FindReplaceBar bar;
    QSignalSpy spy(&bar, &FindReplaceBar::searchTextChanged);

    bar.showForFind();
    QTest::qWait(300); // debounce interval is 150 ms
    QCOMPARE(spy.count(), 1);

    // A burst of keystrokes collapses into a single emission.
    bar.setFindText(QStringLiteral("a"));
    bar.setFindText(QStringLiteral("ab"));
    bar.setFindText(QStringLiteral("abc"));
    QTest::qWait(300);
    QCOMPARE(spy.count(), 2);

    // Clearing the box reports immediately instead of after the delay.
    QWidget *field = bar.focusWidget();
    QVERIFY(field != nullptr);
    QTest::keyClicks(field, QStringLiteral("xyz"));
    QTest::qWait(300);
    const int afterTyping = spy.count();
    QVERIFY(afterTyping > 2);
    QTest::keyClick(field, Qt::Key_A, Qt::ControlModifier); // select all
    QTest::keyClick(field, Qt::Key_Backspace); // delete -> textChanged("")
    QCOMPARE(spy.count(), afterTyping + 1);
}

void TestCore::escapeHidesBarAndSignals()
{
    FindReplaceBar bar;
    QSignalSpy spy(&bar, &FindReplaceBar::escapePressed);
    bar.showForFind();
    QVERIFY(bar.isVisible());

    QTest::keyClick(&bar, Qt::Key_Escape);
    QVERIFY(bar.isHidden());
    QCOMPARE(spy.count(), 1);
}

void TestCore::themeLightIndependentOfAppPalette()
{
    // Regression: QPalette() copies the application palette, so Light mode
    // returned dark colors on a dark OS (and dark mode inherited unlisted
    // light OS roles). The scheme must be deterministic either way.
    const QPalette saved = qApp->palette();
    qApp->setPalette(theme::paletteFor(theme::Mode::Dark));
    const QPalette light = theme::paletteFor(theme::Mode::Light);
    const QPalette dark = theme::paletteFor(theme::Mode::Dark);
    qApp->setPalette(saved);

    QVERIFY(light.color(QPalette::Window).lightness() > 128);
    QVERIFY(light.color(QPalette::Base).lightness() > 128);
    QVERIFY(light.color(QPalette::Text).lightness() < 128);
    QVERIFY(dark.color(QPalette::Window).lightness() < 128);
    QVERIFY(dark.color(QPalette::Text).lightness() > 128);
}

void TestCore::headingLevelKeepsTrailingHash()
{
    // A closing hash run needs preceding whitespace: "## Learning C#"
    // keeps its "#", while "## Closed ##" still strips the closer.
    QString title;
    QCOMPARE(MarkdownHighlighter::headingLevel(QStringLiteral("## Learning C#"), &title), 2);
    QCOMPARE(title, QStringLiteral("Learning C#"));
}

void TestCore::enterOnEmptyItemExitsList()
{
    // Enter on an empty item drops its marker (exiting the list) instead
    // of nesting forever, in a single undo step.
    MarkdownEditor editor;
    editor.setPlainText(QStringLiteral("1. item\n2. "));
    QTextCursor cursor = editor.textCursor();
    cursor.movePosition(QTextCursor::End);
    editor.setTextCursor(cursor);
    QTest::keyClick(&editor, Qt::Key_Return);
    QCOMPARE(editor.toPlainText(), QStringLiteral("1. item\n\n"));
    editor.undo();
    QCOMPARE(editor.toPlainText(), QStringLiteral("1. item\n2. "));
}

void TestCore::enterAtLineStartSplitsPlainly()
{
    // No marker continuation when there is nothing to continue.
    MarkdownEditor editor;
    editor.setPlainText(QStringLiteral("- foo"));
    QTextCursor cursor = editor.textCursor();
    cursor.movePosition(QTextCursor::Start);
    editor.setTextCursor(cursor);
    QTest::keyClick(&editor, Qt::Key_Return);
    QCOMPARE(editor.toPlainText(), QStringLiteral("\n- foo"));
}

void TestCore::tabTwiceKeepsIndentingSelection()
{
    // The transformed range stays selected so repeated Tab keeps working.
    MarkdownEditor editor;
    editor.setPlainText(QStringLiteral("alpha\nbeta tail\ngamma"));
    selectRange(editor, 0, 10);
    QTest::keyClick(&editor, Qt::Key_Tab);
    QCOMPARE(editor.toPlainText(), QStringLiteral("    alpha\n    beta tail\ngamma"));
    QTest::keyClick(&editor, Qt::Key_Tab);
    QCOMPARE(editor.toPlainText(), QStringLiteral("        alpha\n        beta tail\ngamma"));
}

void TestCore::italicToggleOnBoldAddsMarkers()
{
    // Ctrl+I on "**bold**" must not strip it down to "*bold*": runs are
    // counted, and only exact runs unwrap.
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("**bold**"));
        selectRange(editor, 2, 6); // inner "bold"
        editor.toggleItalic();
        QCOMPARE(editor.toPlainText(), QStringLiteral("***bold***"));
    }
    // Exact runs still toggle off.
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("**bold**"));
        selectRange(editor, 0, 8);
        editor.toggleBold();
        QCOMPARE(editor.toPlainText(), QStringLiteral("bold"));
    }
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("*italic*"));
        selectRange(editor, 0, 8);
        editor.toggleItalic();
        QCOMPARE(editor.toPlainText(), QStringLiteral("italic"));
    }
}

void TestCore::fencedToggleUnwrapsEnclosing()
{
    // Bare cursor inside the block unwraps the whole pair.
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("```\ncode\n```"));
        QTextCursor cursor = editor.textCursor();
        cursor.setPosition(5); // inside "code"
        editor.setTextCursor(cursor);
        editor.toggleFencedCodeBlock();
        QCOMPARE(editor.toPlainText(), QStringLiteral("code\n"));
    }
    // Bare cursor on either fence unwraps too.
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("```\ncode\n```"));
        QTextCursor cursor = editor.textCursor();
        cursor.setPosition(0);
        editor.setTextCursor(cursor);
        editor.toggleFencedCodeBlock();
        QCOMPARE(editor.toPlainText(), QStringLiteral("code\n"));
    }
    // Bare cursor outside any fence wraps the line (unchanged behavior).
    {
        MarkdownEditor editor;
        editor.setPlainText(QStringLiteral("plain"));
        QTextCursor cursor = editor.textCursor();
        cursor.movePosition(QTextCursor::End);
        editor.setTextCursor(cursor);
        editor.toggleFencedCodeBlock();
        QCOMPARE(editor.toPlainText(), QStringLiteral("```\nplain\n```"));
    }
}

void TestCore::dropUrlsAreRejectedByEditor()
{
    TestableEditor editor;
    QMimeData urls;
    urls.setUrls({ QUrl::fromLocalFile(QStringLiteral("/tmp/test.md")) });
    QVERIFY(!editor.testCanInsert(&urls));
    QMimeData text;
    text.setText(QStringLiteral("hello"));
    QVERIFY(editor.testCanInsert(&text));
}

void TestCore::imagePastePrefersText()
{
    // Clipboard with both text and image (office/browsers) pastes text.
    TestableEditor editor;
    QSignalSpy spy(&editor, &MarkdownEditor::imagePasted);
    QImage image(10, 10, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QMimeData both;
    both.setImageData(image);
    both.setText(QStringLiteral("pasted table"));
    editor.testInsert(&both);
    QCOMPARE(editor.toPlainText(), QStringLiteral("pasted table"));
    QCOMPARE(spy.count(), 0);

    // Image-only clipboard still emits for embedding.
    TestableEditor imageOnly;
    QSignalSpy spy2(&imageOnly, &MarkdownEditor::imagePasted);
    QMimeData mime;
    mime.setImageData(image);
    imageOnly.testInsert(&mime);
    QCOMPARE(spy2.count(), 1);
}

void TestCore::replaceAllClosesEditBlock()
{
    // Clean recovery dir: a stale snapshot would prompt modally in the
    // MainWindow constructor and block headless runs.
    QStandardPaths::setTestModeEnabled(true);
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .removeRecursively();
    MainWindow window;
    auto *editor = window.findChild<MarkdownEditor *>();
    QVERIFY(editor != nullptr);
    editor->setPlainText(QStringLiteral("a a a"));
    QMetaObject::invokeMethod(&window, "replaceAll", Q_ARG(QString, QStringLiteral("a")),
                              Q_ARG(QString, QStringLiteral("b")), Q_ARG(bool, false));
    QCOMPARE(editor->toPlainText(), QStringLiteral("b b b"));
    // A later edit must be its own undo step: the replace block was closed.
    editor->insertPlainText(QStringLiteral("!"));
    editor->document()->undo();
    QCOMPARE(editor->toPlainText(), QStringLiteral("b b b"));
}

void TestCore::spellToggleRehighlights()
{
    SpellChecker checker;
    if (!checker.available()) {
        QSKIP("Sonnet not available in this build");
    }
    checker.setEnabled(true);

    MarkdownHighlighter highlighter(nullptr);
    highlighter.setSpellChecker(&checker);
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("helllo world"));
    highlighter.setDocument(&doc);
    QTest::qWait(50);
    QCOMPARE(spellUnderlineCount(doc.firstBlock()), 1);

    // Disabling rehighlights through the stateChanged connection: no
    // manual rehighlight() call here.
    checker.setEnabled(false);
    QTest::qWait(50);
    QCOMPARE(spellUnderlineCount(doc.firstBlock()), 0);
}

void TestCore::lineEndingRoundTrip()
{
    QCOMPARE(MainWindow::detectLineEnding(QByteArray("a\r\nb")), QStringLiteral("\r\n"));
    QCOMPARE(MainWindow::detectLineEnding(QByteArray("a\nb")), QStringLiteral("\n"));
    QCOMPARE(MainWindow::detectLineEnding(QByteArray()), QStringLiteral("\n"));
    QCOMPARE(MainWindow::encodeWithLineEnding(QStringLiteral("a\nb"), QStringLiteral("\r\n")),
             QByteArray("a\r\nb"));
    QCOMPARE(MainWindow::encodeWithLineEnding(QStringLiteral("a\nb"), QStringLiteral("\n")),
             QByteArray("a\nb"));
}

void TestCore::recoverySkipsCleanDocument()
{
    QStandardPaths::setTestModeEnabled(true);
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir(dir).removeRecursively();
    MainWindow window;
    // Fresh window: nothing modified, so no snapshot is written.
    QMetaObject::invokeMethod(&window, "writeRecoveryFile");
    const auto snapshots = [&] {
        return QDir(dir).entryList({ QStringLiteral("recovery-*.json") }, QDir::Files);
    };
    QCOMPARE(snapshots().size(), 0);
    auto *editor = window.findChild<MarkdownEditor *>();
    QVERIFY(editor != nullptr);
    // A real user edit (setPlainText does not touch the modified flag).
    editor->insertPlainText(QStringLiteral("unsaved work"));
    QMetaObject::invokeMethod(&window, "writeRecoveryFile");
    QCOMPARE(snapshots().size(), 1);
    QDir(dir).removeRecursively();
}

void TestCore::loadRecoveryMarksModified()
{
    QStandardPaths::setTestModeEnabled(true);
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir(dir).removeRecursively(); // no stale prompt that would block headless
    QDir().mkpath(dir);
    MainWindow window; // constructed clean: no prompt below can block
    const QString path = dir + QStringLiteral("/recovery-99999.json");
    QJsonObject root;
    root.insert(QStringLiteral("content"), QStringLiteral("restored"));
    root.insert(QStringLiteral("path"), QStringLiteral(""));
    QFile fixture(path);
    QVERIFY(fixture.open(QIODevice::WriteOnly));
    fixture.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    fixture.close();

    bool ok = false;
    QMetaObject::invokeMethod(&window, "loadRecoveryFile", Q_RETURN_ARG(bool, ok),
                              Q_ARG(QString, path));
    QVERIFY(ok);
    auto *editor = window.findChild<MarkdownEditor *>();
    QVERIFY(editor != nullptr);
    QCOMPARE(editor->toPlainText(), QStringLiteral("restored"));
    // Restored work counts as unsaved: closing must prompt, not discard.
    QVERIFY(editor->document()->isModified());
    QFile::remove(path);
}

void TestCore::previewScrollFollowsRatio()
{
    // Programmatic setScrollRatio must move the viewport, not just the
    // scrollbar: valueChanged is what drives QAbstractScrollArea.
    MarkdownPreview preview;
    preview.show();
    preview.resize(400, 300);
    QTextDocument source;
    QStringList lines;
    for (int i = 0; i < 200; ++i)
        lines << QStringLiteral("line %1").arg(i);
    source.setPlainText(lines.join(QStringLiteral("\n\n"))); // hard breaks: 200 blocks
    preview.setSourceDocument(&source);
    preview.renderNow();
    QVERIFY(preview.verticalScrollBar()->maximum() > 0);

    const int topBlock = preview.cursorForPosition(QPoint(5, 5)).blockNumber();
    QVERIFY(topBlock < 5);
    preview.setScrollRatio(1.0);
    QTest::qWait(50);
    const int bottomBlock = preview.cursorForPosition(QPoint(5, 5)).blockNumber();
    QVERIFY(bottomBlock > 150);
}

void TestCore::previewLinkClickKeepsDocument()
{
    // anchorClicked fires regardless of openLinks (Qt docs); with internal
    // navigation off, a failed target can no longer blank the preview.
    // file:// keeps the test side-effect free (no browser launch).
    MarkdownPreview preview;
    preview.show();
    preview.resize(400, 300);
    QTextDocument source;
    source.setPlainText(QStringLiteral("[click me](file:///tmp/nonexistent.md)"));
    preview.setSourceDocument(&source);
    preview.renderNow();
    const QString before = preview.document()->toPlainText();

    QMetaObject::invokeMethod(&preview, "openAllowedLink",
                              Q_ARG(QUrl, QUrl(QStringLiteral("file:///tmp/nonexistent.md"))));
    QTest::qWait(50);
    QCOMPARE(preview.document()->toPlainText(), before);

    // Same-document anchors scroll instead of navigating.
    QString html = QStringLiteral("<p>%1</p><p><a name=\"sec\">target</a></p>")
                       .arg(QStringLiteral("<br/>").repeated(200));
    preview.document()->setHtml(html);
    QVERIFY(preview.verticalScrollBar()->maximum() > 0);
    QMetaObject::invokeMethod(&preview, "openAllowedLink",
                              Q_ARG(QUrl, QUrl(QStringLiteral("#sec"))));
    QVERIFY(preview.verticalScrollBar()->value() > 0);
}

void TestCore::findBarHiddenAtStartup()
{
    QWidget parent;
    FindReplaceBar bar(&parent);
    parent.show();
    QVERIFY(bar.isHidden());
}

void TestCore::previewRendersDocumentSource()
{
    MarkdownPreview preview;
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("# Title\n\nBody text"));
    preview.setSourceDocument(&doc);
    preview.renderNow(); // bypass the throttle for determinism

    const QString text = preview.document()->toPlainText();
    QVERIFY(text.contains(QStringLiteral("Title")));
    QVERIFY(text.contains(QStringLiteral("Body text")));
}

void TestCore::previewThrottleRendersAtMostOncePerInterval()
{
    // scheduleRender() is a throttle, not a debounce: a burst of requests
    // yields exactly one render at the first deadline.
    MarkdownPreview preview;
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("hello"));
    preview.setSourceDocument(&doc);

    QSignalSpy rendered(&preview, &MarkdownPreview::documentRendered);
    preview.scheduleRender();
    preview.scheduleRender();
    preview.scheduleRender();
    QTest::qWait(400); // interval is 250 ms
    QCOMPARE(rendered.count(), 1);
}

void TestCore::matchCountingDoesNotNeedCaret()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("Ip sum ipsum\nip\n"));

    // Case-insensitive: Ip, ipsum, ip -> 3
    QCOMPARE(MainWindow::countMatchesInDocument(&doc, QStringLiteral("ip"), false), 3);
    // Case-sensitive: ipsum, ip -> 2 ("Ip" excluded)
    QCOMPARE(MainWindow::countMatchesInDocument(&doc, QStringLiteral("ip"), true), 2);
    // Empty / no-match
    QCOMPARE(MainWindow::countMatchesInDocument(&doc, QString(), false), 0);
    QCOMPARE(MainWindow::countMatchesInDocument(&doc, QStringLiteral("zzz"), false), 0);
    // Null document is safe
    QCOMPARE(MainWindow::countMatchesInDocument(nullptr, QStringLiteral("ip"), false), 0);
}

void TestCore::spellCheckFlagsMisspellings()
{
    SpellChecker checker;
    if (!checker.available()) {
        QSKIP("Sonnet not available in this build");
    }
    checker.setEnabled(true);
    QVERIFY(checker.isWordCorrect(QStringLiteral("hello")));
    QVERIFY(!checker.isWordCorrect(QStringLiteral("helllo")));

    MarkdownHighlighter highlighter(nullptr);
    highlighter.setSpellChecker(&checker);
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("hello helllo world"));
    highlighter.setDocument(&doc);
    QVERIFY(doc.firstBlock().isValid());
    QTest::qWait(50);
    QCOMPARE(spellUnderlineCount(doc.firstBlock()), 1);
}

void TestCore::spellCheckSkipsCodeSegments()
{
    SpellChecker checker;
    if (!checker.available()) {
        QSKIP("Sonnet not available in this build");
    }
    checker.setEnabled(true);

    MarkdownHighlighter highlighter(nullptr);
    highlighter.setSpellChecker(&checker);
    QTextDocument doc;
    // `helllo` sits in inline code; `zzqqx` is plain prose.
    doc.setPlainText(QStringLiteral("use `helllo` code zzqqx here"));
    highlighter.setDocument(&doc);
    QTest::qWait(50);
    QCOMPARE(spellUnderlineCount(doc.firstBlock()), 1);
}

void TestCore::toolbarIconsRenderOnBothBackgrounds()
{
    // Every icon renders non-empty for both theme backgrounds.
    for (int i = 0; i <= int(appicons::Icon::NumberedList); ++i) {
        for (const QColor bg : { QColor(0xff, 0xff, 0xff), QColor(0x1e, 0x1e, 0x1e) }) {
            const QIcon icon = appicons::makeIcon(static_cast<appicons::Icon>(i), bg);
            QVERIFY(!icon.isNull());
            QVERIFY2(!icon.availableSizes().isEmpty(),
                     qPrintable(QStringLiteral("icon %1 has no sizes").arg(i)));
            const QSize expected(48, 48); // largest requested bucket
            QVERIFY(icon.availableSizes().contains(expected));
            QPixmap pm = icon.pixmap(expected);
            // The platform DPR (e.g. 125% Windows scaling) scales the request;
            // validate the logical size via the pixmap's own DPR.
            QCOMPARE(int(pm.width() / pm.devicePixelRatio()), expected.width());
            QVERIFY2(!pm.toImage().allGray(),
                     qPrintable(QStringLiteral("icon %1 rendered empty").arg(i)));
        }
    }
}

void TestCore::applicationIconRendersAllSizes()
{
    // Every standard size paints something with the brand gradient present.
    for (int size : { 16, 24, 32, 48, 64, 128, 256 }) {
        const QImage img = appicons::applicationIconPixmap(size).toImage();
        QCOMPARE(img.width(), size);
        QCOMPARE(img.height(), size);
        QVERIFY2(!img.allGray(), qPrintable(QStringLiteral("app icon %1 empty").arg(size)));

        // Corner pixel is transparent (rounded badge); center-ish pixel is
        // inside the indigo/purple badge.
        QCOMPARE(img.pixelColor(0, 0).alpha(), 0);
        // (size/8, size/8) is inside the badge interior for every size (the
        // badge spans 0.75..23.25 on the 24-unit grid; fixed pixel offsets
        // from the edges can land on the anti-aliased boundary).
        const QColor mid = img.pixelColor(size / 8, size / 8);
        QVERIFY2(mid.alpha() > 200,
                 qPrintable(QStringLiteral("app icon %1 badge not opaque").arg(size)));
        QVERIFY2(mid.blue() > mid.red(), // indigo/purple family: blue-dominant
                 qPrintable(QStringLiteral("app icon %1 wrong badge color").arg(size)));
    }

    const QIcon icon = appicons::applicationIcon();
    QVERIFY(!icon.isNull());
    QVERIFY(icon.availableSizes().size() >= 5);
}

void TestCore::themeColorsPresent()
{
    const auto light = theme::syntaxColors(theme::Mode::Light);
    const auto dark = theme::syntaxColors(theme::Mode::Dark);
    QCOMPARE(light.size(), 12);
    QCOMPARE(dark.size(), 12);
    QVERIFY(light.value(QStringLiteral("heading")) != dark.value(QStringLiteral("heading")));
}

void TestCore::palettesDifferByMode()
{
    const QPalette light = theme::paletteFor(theme::Mode::Light);
    const QPalette dark = theme::paletteFor(theme::Mode::Dark);
    QVERIFY(light.color(QPalette::Window) != dark.color(QPalette::Window));
}

QTEST_MAIN(TestCore)
#include "test_core.moc"
