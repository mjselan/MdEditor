Bugs confirmed by reading the code

1. Replace All leaves an edit block open. In MainWindow::replaceAll, you call cursor.beginEditBlock(), then reassign cursor = document->find(...) in the loop. find() ends by returning a null cursor, so cursor.endEditBlock() is a no-op and the document's edit block is never closed. That can stop textChanged (and so the preview updating) and merge later edits into one undo step. Use a separate cursor for the edit block:

cpp
QTextDocument *doc = m_editor->document();
QTextCursor edit(doc);
edit.beginEditBlock();
QTextCursor hit(doc);
while (!(hit = doc->find(findText, hit, flags)).isNull()) {
    hit.insertText(replaceText);
    ++count;
}
edit.endEditBlock();

2. Session recovery can lose data.

attemptRecoveryLoad() calls setCurrentFile(), which sets setModified(false), then only sets the window-title star with setWindowModified(true). maybeSave() checks document()->isModified(), so closing (or opening another file) discards the restored text without asking. closeEvent then deletes the recovery file. Add m_editor->document()->setModified(true) after setCurrentFile.
writeRecoveryFile() runs every 30 seconds even when nothing is unsaved. After a crash, users get a "restore?" prompt for a document that is identical to the saved one, and large documents are rewritten to disk constantly. Add if (!m_editor->document()->isModified()) return;.
The README says the snapshot is written "on exit", but closeEvent deletes it.
There is one global recovery.json. A second app instance will prompt to restore the first instance's autosave, and answering "No" deletes it. Use a per-process file or a QLockFile.

3. Ctrl++ and Ctrl+- font changes are never saved. onLargerFont and onSmallerFont change the editor font but not m_fontFamily or m_fontSize, which are what writeSettings() saves. Only the font dialog persists. Read m_editor->font() in writeSettings() instead. pointSize() also returns -1 for pixel-sized fonts, so +1 gives 0.

4. The Spell Check toggle does nothing visible. SpellChecker::stateChanged is only connected to the menu action, and nothing calls rehighlight(). Underlines don't appear or disappear until each block is edited. Add this in setSpellChecker():

cpp
connect(checker, &SpellChecker::stateChanged, this, &QSyntaxHighlighter::rehighlight);

suggestionsFor, ignoreWord, addToPersonal, setDictionary and dictionaries are never called. Underlines exist, but there's no right-click menu with suggestions or "Add to dictionary".

5. The editor keeps stale colors after a theme change. MarkdownEditor computes the current-line and gutter colors once in its constructor, and its comment claims theme switches "flow through". There's no changeEvent handler. Since MainWindow builds the editor before readSettings() applies the saved theme, a saved Light theme on a dark OS gives wrong colors. Override changeEvent for QEvent::PaletteChange and ApplicationPaletteChange and recompute. The lineNumber and lineNumberActive entries in theme.cpp are unused.

6. Ctrl+I on bold text turns it italic. wrapSelection("*") sees **bold** as "markers already adjacent" and removes one * from each side, leaving *bold*. It should add italic (***bold***). Count the run of * around the selection instead of matching one character.

7. Headings ending in # lose the character. ^\s{0,3}(#{1,6})(\s+)(.+?)\s*#*\s*$ makes ## Learning C# show as "Learning C" in the outline. CommonMark requires whitespace before a closing sequence. Use (.+?)(?:\s+#+)?\s*$.

8. Undo and Redo are always enabled. Nothing connects undoAvailable or redoAvailable.

9. MarkdownHighlighter::headings() is unused. MainWindow::rebuildOutline() duplicates its loop. Pick one, and pass headings as a struct Heading { int level; QString title; int pos; } instead of two parallel vectors.

Likely bugs, worth a quick test
Theme can't go from dark back to light. paletteFor(Light) returns a default-constructed QPalette, which copies the current application palette, so after a dark palette is applied it returns the same dark palette. Since you test on Qt 6.11, the cleanest fix on Qt 6.8+ is QGuiApplication::styleHints()->setColorScheme(...), with your custom palette only as an older-Qt fallback. Test on a dark OS: View → Theme → Light.
The find bar may be visible at startup. The header says "Hidden bar", but nothing in the constructor or createWidgets() calls hide(). Only Esc does.
QSignalBlocker on the scrollbars. QAbstractScrollArea connects the scrollbar's valueChanged to its own internal scroll slot. Blocking signals can leave the viewport not repainting until something else triggers it. Use a bool m_syncing guard instead. Test by scrolling the preview with the wheel and watching whether the editor moves immediately.
Clicking a link may blank the preview. setOpenExternalLinks(false) with the default openLinks == true lets QTextBrowser also try to navigate to the URL. Call setOpenLinks(false) and handle #anchor links yourself.
Dropping a file onto the editor may insert its path. QMimeData::hasText() is true for URL lists, so the QPlainTextEdit probably accepts the drop as text before MainWindow::dropEvent sees it. Override canInsertFromMimeData and insertFromMimeData to forward URL lists.
Dark-mode printing. setDefaultStyleSheet only affects HTML parsed afterwards, not a document filled by setMarkdown, so the "black text when printing" workaround probably has no effect. Print a clone() with a black foreground merged over the whole document. Export a dark-mode PDF to check.
Image paste is too greedy. insertFromMimeData prefers the image whenever one exists. Excel, Word and some browsers put both text and an image on the clipboard, so pasting a table pastes a screenshot. Paste text if any is present, and use the image only when there is no text.
Enter on an empty list item. I read this as leaving the empty - line behind and adding a new blank line below it. Enter at the very start of a list line would produce - - foo. The Enter handling is also not in one edit block, so undo takes two steps.
Editing behavior
forEachSelectedLine collapses the selection afterward, so pressing Tab twice indents only the first line. Restore the selection over the replaced range.
Bullet and number toggles turn blank lines between paragraphs into empty items (- or 1. ). Skip blank lines for lists.
The fenced-block toggle only unwraps when the selection starts on the opening fence and ends on the closing one. Detect an enclosing fence from the cursor.
The list-continuation handler only checks Key_Return, not Key_Enter.
Ctrl+Shift+U is the Unicode-input shortcut under IBus on Linux.
File I/O and platform
Line endings are changed silently. Saving with QIODevice::Text turns LF files into CRLF on Windows and CRLF files into LF elsewhere. For a Markdown editor used on git repos, that means whole-file diffs. Detect the line ending on load and write it back unchanged. Detect invalid UTF-8 too, since readAll() silently substitutes U+FFFD and saving then corrupts the file.
Command-line paths on Windows. QString::fromLocal8Bit(argv[1]) mangles non-ASCII paths. Use app.arguments(), and consider QCommandLineParser so --version doesn't get opened as a file.
HTML export isn't atomic or checked. It uses plain QFile with no error check, so use QSaveFile. Exported relative image paths break once the file is moved, and toHtml() emits Qt-specific -qt- CSS.
Windows-specific icon font. appicons.cpp hardcodes "Segoe UI", so the B, I and H1 glyphs render differently elsewhere. Use QGuiApplication::font().
Static icon cache. The QHash<int, QIcon> is a function-local static, so it is destroyed after QGuiApplication. Clear it on aboutToQuit.
Icon palette mismatch. refresh() uses the window palette while iconFor() uses the application palette.
Font fallback. Fall back to QFontDatabase::systemFont(FixedFont) instead of the string "monospace".
Performance
onCursorPositionChanged starts the count timer, so every arrow-key press copies the whole document via toPlainText() and rescans it. Word counts don't depend on the cursor. Drop it, or use it only for a selection count.
The outline panel is rebuilt every 250 ms while typing, even when its dock is hidden. Rebuild only when it is visible.
MarkdownPreview::loadResource may re-decode images on every re-render. Cache by path and modification time, and scale to the viewport width. Pasted screenshots are full-size PNGs and overflow the pane.
Add a length cap (say 5–10k characters) for inline highlighting. The lazy (.+?) regexes with lookbehinds can go quadratic on one huge line, such as minified content.
Spell check calls format(i) and setFormat(i, 1, ...) per character. Group by runs.
[\w]+ splits "don't" into don and t and flags snake_case. Use \w+(?:['’]\w+)*.
Cache isWordCorrect results, since rehighlighting calls it for every word.
Structure
MainWindow is about 1,100 lines. It handles file I/O, recovery, settings, export, find/replace and wiring. Extract RecoveryManager, a typed AppSettings (which would also validate the theme value you currently static_cast from an int), and a file-loading class that owns encoding and line endings. Then most tests don't need a widget.
Put pure text logic in a small mdtext module. wordCount, countMatchesInDocument, headingLevel and the fence helpers currently live on MainWindow or MarkdownHighlighter only so tests can reach them.
Replace the QHash<QString, QColor> roles with a struct SyntaxColors. A typo such as color("hedaing") currently fails silently at runtime. The toJson and applyJson stubs in theme.cpp are dead code, so remove them or implement them.
Use Theme::Mode instead of setThemeMode(int).
Use the same QSettings everywhere. main.cpp already sets the org and app name, so QSettings s; is enough and settingsOrg() can go.
Replace the per-method #ifdef MARKDOWNEDITOR_HAVE_SONNET blocks in spellchecker.cpp. The return; before Q_UNUSED is unreachable code. Compile spellchecker_sonnet.cpp or spellchecker_stub.cpp from CMake. Also construct Sonnet::Speller lazily, since it loads dictionaries.
connect your undo/redo actions to document()->modificationChanged for the title star instead of updating it in onTextChanged. That removes a whole class of bug like #2.
QKeySequence::ZoomIn and ZoomOut handle Ctrl+= and the keyboard-layout differences that "Ctrl++" doesn't.
Tests and CI

Bugs #1, #2, #6 and #7 are all easy to cover with QTest once the text logic is separated. For example, feed wrapSelection **bold** with Ctrl+I, or call replaceAll and then check document()->isUndoRedoEnabled() and that textChanged still fires. Set QT_QPA_PLATFORM=offscreen for headless runs, and add a GitHub Actions matrix (Ubuntu, Windows, macOS) with install-qt-action.