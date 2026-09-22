#include <QtTest>

#include "markdowneditor.h"
#include "markdownhighlighter.h"
#include "mainwindow.h"
#include "markdownpreview.h"
#include "spellchecker.h"
#include "theme.h"

#include <QSignalSpy>
#include <QTextBlock>
#include <QTextDocument>

namespace {

QVector<QTextBlock> allBlocks(QTextDocument *doc)
{
    QVector<QTextBlock> out;
    for (QTextBlock b = doc->firstBlock(); b.isValid(); b = b.next())
        out.append(b);
    return out;
}

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
    void previewRendersDocumentSource();
    void previewThrottleRendersAtMostOncePerInterval();
    void matchCountingDoesNotNeedCaret();
    void spellCheckFlagsMisspellings();
    void spellCheckSkipsCodeSegments();
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
