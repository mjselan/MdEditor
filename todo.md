Suggested TODO list (paste into your coding agent)
 P0: Fix MarkdownEditor::forEachSelectedLine — remove the qMin(to, lastContentEnd) cap; always replace through lastContentEnd. Add regression tests: mid-line-ending multi-line selection for heading, blockquote, bullet list, numbered list, and Tab/Shift-Tab indent, asserting exact output text (not just "no crash").
 P1: Fix MainWindow::exportPdf() to render from m_preview (call m_preview->renderNow() then print m_preview->document()), matching the exportHtml() fix.
 P2: Remove or repurpose the redundant replaceCurrent → m_countTimer.start() connection in connectSignals().
 P2: Remove the redundant setMatchCount(1) call in findNext() (immediately overwritten by countMatches()).
 P2: Simplify replaceAll's count > 0 ? count : 0 to count.
 P2: Remove unused MarkdownPreview::setSourceMarkdown(), or document when it should be used instead of setSourceDocument().
 P2: Mirror FREEBUFF_HAVE_SONNET / KF6::Sonnet* onto the test_core target (conditionally, when KF6Sonnet_FOUND) so the Sonnet-enabled code path is actually covered by CI.
 P2: Confirm the debounce→throttle change in MarkdownPreview::scheduleRender() is intentional; update the README's "updated on a 250 ms debounce" wording if so.
 Optional/nice-to-have (not a bug): add a real "Print…" action using QPrintDialog alongside PDF export, now that PrintSupport is already linked.