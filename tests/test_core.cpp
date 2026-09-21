#include <QtTest>

#include "markdownhighlighter.h"
#include "theme.h"

#include <QTextBlock>
#include <QTextDocument>

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void headingLevelParsesLevels();
    void headingLevelRejectsNonHeadings();
    void headingLevelStripsClosingHashes();
    void openFenceDetection();
    void fencedCodeBlockStates();
    void themeColorsPresent();
    void palettesDifferByMode();
};

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
    QChar marker;
    QVERIFY(MarkdownHighlighter::isOpenFence(QStringLiteral("```"), &marker));
    QCOMPARE(marker, QChar::fromLatin1('`'));
    QVERIFY(MarkdownHighlighter::isOpenFence(QStringLiteral("  ~~~~ code"), &marker));
    QCOMPARE(marker, QChar::fromLatin1('~'));
    QVERIFY(!MarkdownHighlighter::isOpenFence(QStringLiteral("``")));
    QVERIFY(!MarkdownHighlighter::isOpenFence(QStringLiteral("text ``` inline")));
}

void TestCore::fencedCodeBlockStates()
{
    QTextDocument doc;
    MarkdownHighlighter highlighter(&doc);
    highlighter.setColors(theme::syntaxColors(theme::Mode::Light));

    doc.setPlainText(QStringLiteral("```cpp\nint x;\n```\nafter"));
    highlighter.rehighlight(); // make block states deterministic for the test

    const QVector<QTextBlock> blocks = [] (QTextDocument *d) {
        QVector<QTextBlock> out;
        for (QTextBlock b = d->firstBlock(); b.isValid(); b = b.next())
            out.append(b);
        return out;
    }(&doc);

    QCOMPARE(blocks.size(), 4);
    QCOMPARE(blocks.at(0).userState(), int(MarkdownHighlighter::StateFencedCode));
    QCOMPARE(blocks.at(1).userState(), int(MarkdownHighlighter::StateFencedCode));
    QCOMPARE(blocks.at(2).userState(), int(MarkdownHighlighter::StateNone));
    QCOMPARE(blocks.at(3).userState(), int(MarkdownHighlighter::StateNone));
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
