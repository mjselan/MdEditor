Implementation Task List for AI Agent
Each task specifies target files, reproduction conditions, and precise technical solutions.
Task 1: Fix Sonnet enabled() method call compile break
Priority: High (Breaks builds with KF6Sonnet installed)
Target File: src/spellchecker.cpp
Issue: In SpellChecker::setEnabled(bool enabled), parameter enabled shadows the member function SpellChecker::enabled(). Calling enabled() evaluates as calling a bool variable as a functor.
Action:
Change line 113 from d->decorator->setActive(enabled()); to d->decorator->setActive(enabled); (or d->decorator->setActive(d->enabled);).
Task 2: Decouple Find match counting from cursor movement
Priority: High (Functional bug & UX glitch)
Target Files: src/findreplacebar.h, src/findreplacebar.cpp, src/mainwindow.h, src/mainwindow.cpp
Issue: FindReplaceBar::updateMatchCount() triggers on every QLineEdit::textChanged and emits findNext(..., false). In MainWindow::findNext, this runs m_editor->setTextCursor(found), jumping the caret away while the user is actively typing in the find box. Furthermore, it only reports 0 or 1 match.
Action:
In FindReplaceBar:
Add signal void searchTextChanged(const QString &text, bool matchCase);.
In constructor, connect m_find->textChanged and m_matchCase->toggled to emit searchTextChanged(...) instead of calling updateMatchCount().
Remove the findNext emission inside updateMatchCount().
In MainWindow:
Connect m_findBar->searchTextChanged to a new private slot: void countMatches(const QString &text, bool matchCase).
In countMatches:
If text.isEmpty(), call m_findBar->setMatchCount(0) and return.
Count matches across m_editor->document() using a temporary local QTextCursor (starting at block 0 / start of document) in a loop with doc->find(text, tempCursor, flags) without touching m_editor->setTextCursor().
Pass the actual count integer to m_findBar->setMatchCount(count).
In findNext(const QString &text, bool matchCase, bool backward):
Maintain active navigation jump when explicitly requested (Enter, Next, Prev buttons).
Keep updating or refreshing the total match count.
Task 3: Fix forEachSelectedLine block-boundary bleeding on empty blocks
Priority: High (Data corruption / formatting glitch)
Target File: src/markdowneditor.cpp
Issue:
code
C++
cursor.setPosition(last.position() + qMax(1, last.length() - 1), QTextCursor::KeepAnchor);
QTextBlock::length() includes the block separator (usually 1 char for \u2029). If last is an empty line, last.length() == 1. Subtracting 1 gives 0, but qMax(1, 0) forces the offset to 1, causing the anchor to extend into the next block and mangling surrounding text.
Action:
Change line 282 from:
code
C++
cursor.setPosition(last.position() + qMax(1, last.length() - 1), QTextCursor::KeepAnchor);
to:
code
C++
const int lastBlockContentLen = qMax(0, last.length() - 1);
cursor.setPosition(last.position() + lastBlockContentLen, QTextCursor::KeepAnchor);
Apply the exact same fix to MarkdownEditor::toggleFencedCodeBlock() around line 316.
Task 4: CommonMark-compliant fenced code block matching
Priority: Medium (Highlighter inaccuracy)
Target Files: src/markdownhighlighter.h, src/markdownhighlighter.cpp
Issue: isOpenFence() accepts either 3+ backticks or tildes. Currently, a ~~~ closing marker closes a ``` block and vice versa, and delimiter length constraints are not respected.
Action:
Update MarkdownHighlighter::BlockState or encode state as a bitfield / combined integer:
Marker character (' vs ~).
Minimum length of opening fence (e.g. 3, 4, 5+).
Update isOpenFence:
Have it return fence details (e.g., delimiter character char and length int).
In highlightBlock(const QString &text):
If currently inside a fence, inspect whether text contains a closing fence whose marker character matches the opening delimiter and whose length is greater than or equal to the opening fence length.
If matched, reset the block state to StateNone. Otherwise, retain the state and fence parameters for subsequent blocks.
Task 5: Set document directory on autosave recovery load
Priority: Medium (Preview visual regression)
Target File: src/mainwindow.cpp
Issue: attemptRecoveryLoad() restores the editor text and current file path, but does not call m_preview->setDocumentDirectory(...). If the recovered document used relative image paths, they fail to load until the file is saved again.
Action:
In MainWindow::attemptRecoveryLoad(), after setCurrentFile(path):
code
C++
if (!path.isEmpty()) {
    m_preview->setDocumentDirectory(QFileInfo(path).absolutePath());
}
Task 6: Export rendered Markdown to HTML instead of raw source
Priority: Medium (Functional discrepancy)
Target File: src/mainwindow.cpp
Issue: MainWindow::exportHtml() exports m_editor->document()->toHtml(). Because m_editor holds raw markdown text, Qt wraps raw markdown in <p> tags instead of exporting formatted HTML.
Action:
In MainWindow::exportHtml(), change the write call from:
code
C++
file.write(m_editor->document()->toHtml().toUtf8());
to:
code
C++
file.write(m_preview->document()->toHtml().toUtf8());
Task 7: Defer toPlainText() Markdown conversion to the preview debounce timer
Priority: Medium (Keystroke latency on large documents)
Target Files: src/markdownpreview.h, src/markdownpreview.cpp, src/mainwindow.cpp
Issue: MainWindow::onTextChanged() calls m_preview->setSourceMarkdown(m_editor->toPlainText()) synchronously on every character insertion/deletion. For documents with 10k+ lines, copying the full string on every keypress causes input stutter.
Action:
In MarkdownPreview:
Allow passing a QPointer<QTextDocument> or pointer to m_editor->document() via setSourceDocument(QTextDocument *doc).
Or update setSourceMarkdownPending(std::function<QString()> supplier) so supplier() is only evaluated when m_debounce times out in renderNow().
In MainWindow::onTextChanged():
Replace the eager m_editor->toPlainText() call with a trigger method: m_preview->scheduleRender();.
When m_preview renders upon timer expiration, pull toPlainText() once.
Task 8: CMake: Make Qt6::Test conditional on BUILD_TESTING
Priority: Low / Build hygiene
Target File: CMakeLists.txt
Issue: find_package(Qt6 6.5 REQUIRED COMPONENTS ... Test) forces Qt6::Test to be installed even when building release packages where tests are disabled.
Action:
In the root find_package, change to:
code
Cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)
Inside if(BUILD_TESTING):
code
Cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
Task 9: Expand unit tests in tests/test_core.cpp
Priority: Low / QA
Target Files: CMakeLists.txt, tests/test_core.cpp
Action:
Include markdowneditor.cpp (or split the selection manipulation routines into a testable helper class) into test_core.
Add test cases covering:
forEachSelectedLine with selections ending on an empty block (confirm trailing blocks are not deleted or corrupted).
Document find match counting logic (confirming correct match count without mutating caret position).
Fenced code block delimiters matching (confirming ``` is not terminated by ~~~).