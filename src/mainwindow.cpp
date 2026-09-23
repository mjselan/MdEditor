#include "mainwindow.h"

#include "appicons.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPrinter>
#include <QPrintDialog>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QToolBar>
#include <QStatusBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

#include "findreplacebar.h"
#include "markdowneditor.h"
#include "markdownhighlighter.h"
#include "markdownpreview.h"
#include "outlinepanel.h"
#include "spellchecker.h"

namespace {

constexpr int kDebounceMs = 250;
constexpr int kAutosaveIntervalMs = 30 * 1000;
constexpr int kMaxRecentFiles = 10;

QString settingsOrg() { return QStringLiteral("MdEditor"); }
QString settingsApp() { return QStringLiteral("MarkdownEditor"); }

QString fileFilter()
{
    return QStringLiteral("Markdown (*.md *.markdown *.mkd *.txt);;All files (*)");
}

bool isMarkdownFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("md") || suffix == QLatin1String("markdown")
           || suffix == QLatin1String("mkd") || suffix == QLatin1String("txt");
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    createWidgets();
    createActions();
    createToolbars(); // before createMenus: View menu adds toolbar toggles
    createMenus();
    createStatusBar();
    connectSignals();
    readSettings();
    attemptRecoveryLoad();

    updateWindowTitle();
    updateCounts();
    rebuildOutline();
    m_autosaveTimer.start(kAutosaveIntervalMs);
    setAcceptDrops(true);
}

void MainWindow::createWidgets()
{
    m_editor = new MarkdownEditor(this);
    m_preview = new MarkdownPreview(this);
    m_preview->setSourceDocument(m_editor->document()); // live lazy rendering
    m_outline = new OutlinePanel(this);
    m_findBar = new FindReplaceBar(this);
    m_highlighter = new MarkdownHighlighter(m_editor->document());
    m_spellChecker = new SpellChecker(this);

    m_highlighter->setColors(theme::syntaxColors(m_themeMode));

    m_highlighter->setSpellChecker(m_spellChecker);
    if (m_spellChecker->available())
        m_spellChecker->setEnabled(true);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_editor);
    m_splitter->addWidget(m_preview);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({ 600, 600 });

    auto *center = new QWidget(this);
    auto *centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);
    centerLayout->addWidget(m_findBar);
    centerLayout->addWidget(m_splitter, 1);
    setCentralWidget(center);

    m_outlineDock = new QDockWidget(tr("Outline"), this);
    m_outlineDock->setObjectName(QStringLiteral("outlineDock"));
    m_outlineDock->setWidget(m_outline);
    addDockWidget(Qt::LeftDockWidgetArea, m_outlineDock);
    m_outlineDock->hide();

    m_outline->setColors(palette().color(QPalette::Window),
                         palette().color(QPalette::Text));
}

void MainWindow::createActions()
{
    m_actionNew = new QAction(tr("&New"), this);
    m_actionNew->setShortcut(QKeySequence::New);
    connect(m_actionNew, &QAction::triggered, this, &MainWindow::newFile);

    m_actionOpen = new QAction(tr("&Open..."), this);
    m_actionOpen->setShortcut(QKeySequence::Open);
    connect(m_actionOpen, &QAction::triggered, this, &MainWindow::openFile);

    m_actionSave = new QAction(tr("&Save"), this);
    m_actionSave->setShortcut(QKeySequence::Save);
    connect(m_actionSave, &QAction::triggered, this, &MainWindow::saveFile);

    m_actionSaveAs = new QAction(tr("Save &As..."), this);
    m_actionSaveAs->setShortcut(QKeySequence::SaveAs);
    connect(m_actionSaveAs, &QAction::triggered, this, &MainWindow::saveFileAs);

    m_actionExportHtml = new QAction(tr("Export &HTML..."), this);
    connect(m_actionExportHtml, &QAction::triggered, this, &MainWindow::exportHtml);

    m_actionExportPdf = new QAction(tr("Export &PDF..."), this);
    connect(m_actionExportPdf, &QAction::triggered, this, &MainWindow::exportPdf);

    m_actionPrint = new QAction(tr("&Print..."), this);
    m_actionPrint->setShortcut(QKeySequence::Print);
    connect(m_actionPrint, &QAction::triggered, this, &MainWindow::printFile);

    m_actionQuit = new QAction(tr("E&xit"), this);
    m_actionQuit->setShortcut(QKeySequence::Quit);
    connect(m_actionQuit, &QAction::triggered, this, &MainWindow::close);

    m_actionFind = new QAction(tr("&Find..."), this);
    m_actionFind->setShortcut(QKeySequence::Find);
    connect(m_actionFind, &QAction::triggered, this, &MainWindow::onFindTriggered);

    m_actionReplace = new QAction(tr("&Replace..."), this);
    m_actionReplace->setShortcut(QKeySequence::Replace);
    connect(m_actionReplace, &QAction::triggered, this, &MainWindow::onReplaceTriggered);

    m_actionThemeAuto = new QAction(tr("&Auto (follow system)"), this);
    m_actionThemeLight = new QAction(tr("&Light"), this);
    m_actionThemeDark = new QAction(tr("D&ark"), this);
    auto *themeGroup = new QActionGroup(this);
    for (QAction *action : { m_actionThemeAuto, m_actionThemeLight, m_actionThemeDark }) {
        action->setCheckable(true);
        themeGroup->addAction(action);
    }
    connect(m_actionThemeAuto, &QAction::triggered, this, [this] { setThemeMode(0); });
    connect(m_actionThemeLight, &QAction::triggered, this, [this] { setThemeMode(1); });
    connect(m_actionThemeDark, &QAction::triggered, this, [this] { setThemeMode(2); });

    m_actionOutline = new QAction(tr("&Outline"), this);
    m_actionOutline->setCheckable(true);
    m_actionOutline->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    connect(m_actionOutline, &QAction::toggled, m_outlineDock, &QDockWidget::setVisible);
    connect(m_outlineDock, &QDockWidget::visibilityChanged, m_actionOutline,
            &QAction::setChecked);

    m_actionLargerFont = new QAction(tr("&Larger Font"), this);
    m_actionLargerFont->setShortcut(QKeySequence(QStringLiteral("Ctrl++")));
    connect(m_actionLargerFont, &QAction::triggered, this, &MainWindow::onLargerFont);

    m_actionSmallerFont = new QAction(tr("&Smaller Font"), this);
    m_actionSmallerFont->setShortcut(QKeySequence(QStringLiteral("Ctrl+-")));
    connect(m_actionSmallerFont, &QAction::triggered, this, &MainWindow::onSmallerFont);

    m_actionChooseFont = new QAction(tr("Choose &Font..."), this);
    connect(m_actionChooseFont, &QAction::triggered, this, &MainWindow::onChooseFont);

    m_actionSpellCheck = new QAction(tr("&Spell Check"), this);
    m_actionSpellCheck->setCheckable(true);
    if (m_spellChecker->available()) {
        connect(m_actionSpellCheck, &QAction::toggled, m_spellChecker,
                &SpellChecker::setEnabled);
        connect(m_spellChecker, &SpellChecker::stateChanged, this, [this] {
            m_actionSpellCheck->setChecked(m_spellChecker->enabled());
        });
        m_actionSpellCheck->setChecked(m_spellChecker->enabled());
    } else {
        m_actionSpellCheck->setEnabled(false);
        m_actionSpellCheck->setToolTip(tr("Sonnet library not found; spell check disabled"));
    }

    m_actionUndo = new QAction(tr("&Undo"), this);
    m_actionUndo->setShortcut(QKeySequence::Undo);
    connect(m_actionUndo, &QAction::triggered, m_editor, &QPlainTextEdit::undo);
    m_actionRedo = new QAction(tr("&Redo"), this);
    m_actionRedo->setShortcut(QKeySequence::Redo);
    connect(m_actionRedo, &QAction::triggered, m_editor, &QPlainTextEdit::redo);

    // Format actions are created here (not in createMenus) because both the
    // Format toolbar and the Format menu share them.
    const auto makeFormatAction = [this](const QString &text, const QKeySequence &shortcut,
                                          void (MarkdownEditor::*slot)()) {
        auto *action = new QAction(text, this);
        action->setShortcut(shortcut);
        connect(action, &QAction::triggered, m_editor, slot);
        return action;
    };

    m_actionBold = makeFormatAction(tr("&Bold"), QKeySequence::Bold,
                                    &MarkdownEditor::toggleBold);
    m_actionItalic = makeFormatAction(tr("&Italic"), QKeySequence::Italic,
                                      &MarkdownEditor::toggleItalic);
    m_actionStrike = makeFormatAction(tr("Stri&kethrough"),
                                      QKeySequence(QStringLiteral("Ctrl+Shift+K")),
                                      &MarkdownEditor::toggleStrikethrough);
    m_actionH1 = makeFormatAction(tr("Heading &1"), QKeySequence(QStringLiteral("Ctrl+1")),
                                  &MarkdownEditor::setHeadingLevel1);
    m_actionH2 = makeFormatAction(tr("Heading &2"), QKeySequence(QStringLiteral("Ctrl+2")),
                                  &MarkdownEditor::setHeadingLevel2);
    m_actionH3 = makeFormatAction(tr("Heading &3"), QKeySequence(QStringLiteral("Ctrl+3")),
                                  &MarkdownEditor::setHeadingLevel3);
    m_actionNormal = makeFormatAction(tr("&Normal text"), QKeySequence(QStringLiteral("Ctrl+0")),
                                      &MarkdownEditor::setHeadingLevel0);
    m_actionLink = makeFormatAction(tr("&Link"), QKeySequence(QStringLiteral("Ctrl+K")),
                                    &MarkdownEditor::insertLink);
    m_actionInlineCode = makeFormatAction(tr("Inline &Code"),
                                          QKeySequence(QStringLiteral("Ctrl+Shift+C")),
                                          &MarkdownEditor::toggleInlineCode);
    m_actionCodeBlock = makeFormatAction(tr("Code &Block"),
                                         QKeySequence(QStringLiteral("Ctrl+Shift+B")),
                                         &MarkdownEditor::toggleFencedCodeBlock);
    m_actionBlockquote = makeFormatAction(tr("Block&quote"),
                                          QKeySequence(QStringLiteral("Ctrl+Shift+Q")),
                                          &MarkdownEditor::toggleBlockquote);
    m_actionBulletList = makeFormatAction(tr("&Bulleted List"),
                                          QKeySequence(QStringLiteral("Ctrl+Shift+U")),
                                          &MarkdownEditor::toggleBulletList);
    m_actionNumberedList = makeFormatAction(tr("&Numbered List"),
                                            QKeySequence(QStringLiteral("Ctrl+Shift+N")),
                                            &MarkdownEditor::toggleNumberedList);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_actionNew);
    fileMenu->addAction(m_actionOpen);
    m_recentMenu = fileMenu->addMenu(tr("Open &Recent"));
    fileMenu->addSeparator();
    fileMenu->addAction(m_actionSave);
    fileMenu->addAction(m_actionSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actionExportHtml);
    fileMenu->addAction(m_actionExportPdf);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actionPrint);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actionQuit);
    updateRecentMenu();

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_actionUndo);
    editMenu->addAction(m_actionRedo);
    editMenu->addSeparator();
    editMenu->addAction(m_actionFind);
    editMenu->addAction(m_actionReplace);

    QMenu *formatMenu = menuBar()->addMenu(tr("For&mat"));
    formatMenu->addAction(m_actionBold);
    formatMenu->addAction(m_actionItalic);
    formatMenu->addAction(m_actionStrike);
    formatMenu->addSeparator();
    formatMenu->addAction(m_actionH1);
    formatMenu->addAction(m_actionH2);
    formatMenu->addAction(m_actionH3);
    formatMenu->addAction(m_actionNormal);
    formatMenu->addSeparator();
    formatMenu->addAction(m_actionLink);
    formatMenu->addAction(m_actionInlineCode);
    formatMenu->addAction(m_actionCodeBlock);
    formatMenu->addSeparator();
    formatMenu->addAction(m_actionBlockquote);
    formatMenu->addAction(m_actionBulletList);
    formatMenu->addAction(m_actionNumberedList);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    QMenu *themeMenu = viewMenu->addMenu(tr("&Theme"));
    themeMenu->addAction(m_actionThemeAuto);
    themeMenu->addAction(m_actionThemeLight);
    themeMenu->addAction(m_actionThemeDark);
    viewMenu->addAction(m_actionOutline);
    viewMenu->addSeparator();
    for (QToolBar *bar : findChildren<QToolBar *>())
        viewMenu->addAction(bar->toggleViewAction());
    viewMenu->addSeparator();
    viewMenu->addAction(m_actionLargerFont);
    viewMenu->addAction(m_actionSmallerFont);
    viewMenu->addAction(m_actionChooseFont);

    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(m_actionSpellCheck);
}

void MainWindow::createToolbars()
{
    const QSize iconSize(18, 18);

    m_toolBarFile = addToolBar(tr("File"));
    m_toolBarFile->setObjectName(QStringLiteral("toolbarFile"));
    m_toolBarFile->setIconSize(iconSize);
    m_toolBarFile->setMovable(false);
    m_toolBarFile->addAction(m_actionNew);
    m_toolBarFile->addAction(m_actionOpen);
    m_toolBarFile->addAction(m_actionSave);
    m_toolBarFile->addSeparator();
    m_toolBarFile->addAction(m_actionUndo);
    m_toolBarFile->addAction(m_actionRedo);
    m_toolBarFile->addSeparator();
    m_toolBarFile->addAction(m_actionExportHtml);
    m_toolBarFile->addAction(m_actionExportPdf);
    m_toolBarFile->addAction(m_actionPrint);

    m_toolBarFormat = addToolBar(tr("Format"));
    m_toolBarFormat->setObjectName(QStringLiteral("toolbarFormat"));
    m_toolBarFormat->setIconSize(iconSize);
    m_toolBarFormat->setMovable(false);
    m_toolBarFormat->addAction(m_actionBold);
    m_toolBarFormat->addAction(m_actionItalic);
    m_toolBarFormat->addAction(m_actionStrike);
    m_toolBarFormat->addSeparator();
    m_toolBarFormat->addAction(m_actionH1);
    m_toolBarFormat->addAction(m_actionH2);
    m_toolBarFormat->addAction(m_actionH3);
    m_toolBarFormat->addAction(m_actionNormal);
    m_toolBarFormat->addSeparator();
    m_toolBarFormat->addAction(m_actionLink);
    m_toolBarFormat->addAction(m_actionInlineCode);
    m_toolBarFormat->addAction(m_actionCodeBlock);
    m_toolBarFormat->addSeparator();
    m_toolBarFormat->addAction(m_actionBlockquote);
    m_toolBarFormat->addAction(m_actionBulletList);
    m_toolBarFormat->addAction(m_actionNumberedList);

    refreshToolbarIcons();
}

void MainWindow::refreshToolbarIcons()
{
    using appicons::Icon;
    const QColor window = palette().window().color();
    appicons::refresh(window);

    m_actionNew->setIcon(appicons::iconFor(Icon::NewFile));
    m_actionOpen->setIcon(appicons::iconFor(Icon::Open));
    m_actionSave->setIcon(appicons::iconFor(Icon::Save));
    m_actionUndo->setIcon(appicons::iconFor(Icon::Undo));
    m_actionRedo->setIcon(appicons::iconFor(Icon::Redo));
    m_actionExportHtml->setIcon(appicons::iconFor(Icon::ExportHtml));
    m_actionExportPdf->setIcon(appicons::iconFor(Icon::ExportPdf));
    m_actionPrint->setIcon(appicons::iconFor(Icon::Print));
    m_actionBold->setIcon(appicons::iconFor(Icon::Bold));
    m_actionItalic->setIcon(appicons::iconFor(Icon::Italic));
    m_actionStrike->setIcon(appicons::iconFor(Icon::Strikethrough));
    m_actionH1->setIcon(appicons::iconFor(Icon::H1));
    m_actionH2->setIcon(appicons::iconFor(Icon::H2));
    m_actionH3->setIcon(appicons::iconFor(Icon::H3));
    m_actionNormal->setIcon(appicons::iconFor(Icon::PlainParagraph));
    m_actionLink->setIcon(appicons::iconFor(Icon::Link));
    m_actionInlineCode->setIcon(appicons::iconFor(Icon::InlineCode));
    m_actionCodeBlock->setIcon(appicons::iconFor(Icon::CodeBlock));
    m_actionBlockquote->setIcon(appicons::iconFor(Icon::Blockquote));
    m_actionBulletList->setIcon(appicons::iconFor(Icon::BulletList));
    m_actionNumberedList->setIcon(appicons::iconFor(Icon::NumberedList));
}

void MainWindow::createStatusBar()
{
    m_statsLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statsLabel);
}

void MainWindow::connectSignals()
{
    connect(m_editor, &QPlainTextEdit::textChanged, this, &MainWindow::onTextChanged);
    connect(m_editor, &QPlainTextEdit::cursorPositionChanged, this,
            &MainWindow::onCursorPositionChanged);

    connect(m_editor->verticalScrollBar(), &QAbstractSlider::valueChanged, this,
            &MainWindow::onEditorScrolled);
    connect(m_preview, &MarkdownPreview::scrollRatioChanged, this,
            &MainWindow::onPreviewScrolled);

    m_countTimer.setSingleShot(true);
    m_countTimer.setInterval(150);
    connect(&m_countTimer, &QTimer::timeout, this, &MainWindow::updateCounts);

    m_outlineTimer.setSingleShot(true);
    m_outlineTimer.setInterval(kDebounceMs);
    connect(&m_outlineTimer, &QTimer::timeout, this, &MainWindow::rebuildOutline);

    m_autosaveTimer.setInterval(kAutosaveIntervalMs);
    connect(&m_autosaveTimer, &QTimer::timeout, this, &MainWindow::writeRecoveryFile);

    connect(m_findBar, &FindReplaceBar::findNext, this, &MainWindow::findNext);
    connect(m_findBar, &FindReplaceBar::searchTextChanged, this, &MainWindow::countMatches);
    connect(m_findBar, &FindReplaceBar::replaceCurrent, this, &MainWindow::replaceCurrent);
    connect(m_findBar, &FindReplaceBar::replaceAll, this, &MainWindow::replaceAll);

    connect(m_editor, &MarkdownEditor::imagePasted, this, &MainWindow::onImagePasted);
    connect(m_outline, &OutlinePanel::headingActivated, this,
            &MainWindow::onOutlineActivated);
}

// ---------------------------------------------------------------------------
// File management
// ---------------------------------------------------------------------------

void MainWindow::openPathFromCommandLine(const QString &path)
{
    if (maybeSave())
        loadFile(path);
}

void MainWindow::newFile()
{
    if (!maybeSave())
        return;
    m_editor->clear();
    setCurrentFile(QString());
    updateCounts();
    rebuildOutline();
}

void MainWindow::openFile()
{
    if (!maybeSave())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open File"), QString(),
                                                      fileFilter());
    if (!path.isEmpty())
        loadFile(path);
}

bool MainWindow::loadFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Open Error"),
                             tr("Cannot open %1:\n%2").arg(path, file.errorString()));
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString content = stream.readAll();
    file.close();

    m_editor->setPlainText(content);
    setCurrentFile(path);
    addRecentFile(path);
    m_preview->setDocumentDirectory(QFileInfo(path).absolutePath());
    updateCounts();
    rebuildOutline();
    return true;
}

bool MainWindow::saveFile()
{
    if (m_currentFile.isEmpty())
        return saveFileAs();
    return saveToFile(m_currentFile);
}

bool MainWindow::saveFileAs()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save File"), m_currentFile,
                                                fileFilter());
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += QLatin1String(".md");
    return saveToFile(path);
}

bool MainWindow::saveToFile(const QString &path)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save Error"),
                             tr("Cannot save %1:\n%2").arg(path, file.errorString()));
        return false;
    }
    const QByteArray utf8 = m_editor->toPlainText().toUtf8();
    if (file.write(utf8) != utf8.size() || !file.commit()) {
        QMessageBox::warning(this, tr("Save Error"),
                             tr("Cannot save %1:\n%2").arg(path, file.errorString()));
        return false;
    }

    setCurrentFile(path);
    addRecentFile(path);
    m_preview->setDocumentDirectory(QFileInfo(path).absolutePath());
    clearRecoveryFile();
    return true;
}

void MainWindow::setCurrentFile(const QString &path)
{
    m_currentFile = path;
    m_editor->document()->setModified(false);
    setWindowModified(false);
    updateWindowTitle();
}

bool MainWindow::maybeSave()
{
    if (!m_editor->document()->isModified())
        return true;
    const QMessageBox::StandardButton answer = QMessageBox::warning(
        this, tr("Unsaved Changes"),
        tr("The document has been modified.\nDo you want to save your changes?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save)
        return saveFile();
    return answer == QMessageBox::Discard;
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

void MainWindow::exportHtml()
{
    QString suggested = QStringLiteral("export.html");
    if (!m_currentFile.isEmpty())
        suggested = QFileInfo(m_currentFile).completeBaseName() + QStringLiteral(".html");
    const QString path = QFileDialog::getSaveFileName(this, tr("Export HTML"), suggested,
                                                      QStringLiteral("HTML (*.html *.htm)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Error"),
                             tr("Cannot write %1:\n%2").arg(path, file.errorString()));
        return;
    }
    // Export the rendered document (the preview holds the markdown-rendered
    // copy); the editor document contains raw markdown source.
    m_preview->renderNow(); // ensure the rendered copy is current
    file.write(m_preview->document()->toHtml().toUtf8());
    file.close();
    statusBar()->showMessage(tr("Exported to %1").arg(path), 4000);
}

void MainWindow::exportPdf()
{
    QString suggested = QStringLiteral("export.pdf");
    if (!m_currentFile.isEmpty())
        suggested = QFileInfo(m_currentFile).completeBaseName() + QStringLiteral(".pdf");
    const QString path = QFileDialog::getSaveFileName(this, tr("Export PDF"), suggested,
                                                      QStringLiteral("PDF (*.pdf)"));
    if (path.isEmpty())
        return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    m_preview->renderNow(); // ensure the rendered copy is current
    m_preview->document()->print(&printer);
    statusBar()->showMessage(tr("Exported to %1").arg(path), 4000);
}

void MainWindow::printFile()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print Document"));
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_preview->renderNow(); // ensure the rendered copy is current
    m_preview->document()->print(&printer);
}

// ---------------------------------------------------------------------------
// Text / cursor / scroll slots
// ---------------------------------------------------------------------------

void MainWindow::onTextChanged()
{
    setWindowModified(m_editor->document()->isModified());
    m_countTimer.start();
    m_outlineTimer.start();
    m_preview->scheduleRender(); // pulls toPlainText() lazily in renderNow()
}

void MainWindow::onCursorPositionChanged()
{
    m_countTimer.start();
    m_outline->setActiveHeading(m_editor->textCursor().position());
}

void MainWindow::onEditorScrolled(int value)
{
    const int max = m_editor->verticalScrollBar()->maximum();
    const double ratio = max > 0 ? double(value) / double(max) : 0.0;
    m_preview->setScrollRatio(ratio); // internally signal-blocked
}

void MainWindow::onPreviewScrolled(double ratio)
{
    QScrollBar *bar = m_editor->verticalScrollBar();
    const QSignalBlocker blocker(bar); // do not echo back into the preview
    bar->setValue(qRound(ratio * double(bar->maximum())));
}

// ---------------------------------------------------------------------------
// Find & replace
// ---------------------------------------------------------------------------

int MainWindow::countMatchesInDocument(QTextDocument *document, const QString &text,
                                       bool matchCase)
{
    if (!document || text.isEmpty())
        return 0;

    QTextDocument::FindFlags flags;
    if (matchCase)
        flags |= QTextDocument::FindCaseSensitively;

    QTextCursor probe(document);
    probe.movePosition(QTextCursor::Start);

    int count = 0;
    while (true) {
        probe = document->find(text, probe, flags);
        if (probe.isNull())
            break;
        ++count;
        if (count >= 10000) // safety bound for huge documents
            break;
    }
    return count;
}

void MainWindow::countMatches(const QString &text, bool matchCase)
{
    // The helper navigates a private cursor only, so the editor caret never
    // moves while the user types in the find box.
    m_findBar->setMatchCount(
        countMatchesInDocument(m_editor->document(), text, matchCase));
}

void MainWindow::findNext(const QString &text, bool matchCase, bool backward)
{
    if (text.isEmpty()) {
        m_findBar->setMatchCount(0);
        return;
    }
    QTextDocument::FindFlags flags;
    if (backward)
        flags |= QTextDocument::FindBackward;
    if (matchCase)
        flags |= QTextDocument::FindCaseSensitively;

    QTextCursor cursor = m_editor->textCursor();
    QTextCursor found = m_editor->document()->find(text, cursor, flags);
    if (found.isNull()) {
        // Wrap around the document edges.
        cursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        found = m_editor->document()->find(text, cursor, flags);
    }
    if (found.isNull()) {
        m_findBar->setMatchCount(0);
        return;
    }
    m_editor->setTextCursor(found);
    countMatches(text, matchCase); // refresh the true total
}

void MainWindow::replaceCurrent(const QString &findText, const QString &replaceText,
                                bool matchCase)
{
    QTextCursor cursor = m_editor->textCursor();
    const QString selected = cursor.selectedText();
    const bool matches = matchCase ? selected == findText
                                   : selected.compare(findText, Qt::CaseInsensitive) == 0;
    if (matches)
        cursor.insertText(replaceText);
    findNext(findText, matchCase, false);
}

void MainWindow::replaceAll(const QString &findText, const QString &replaceText, bool matchCase)
{
    if (findText.isEmpty())
        return;
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::Start);

    QTextDocument::FindFlags flags;
    if (matchCase)
        flags |= QTextDocument::FindCaseSensitively;

    cursor.beginEditBlock();
    int count = 0;
    while (true) {
        cursor = m_editor->document()->find(findText, cursor, flags);
        if (cursor.isNull())
            break;
        cursor.insertText(replaceText);
        ++count;
    }
    cursor.endEditBlock();

    m_findBar->setMatchCount(qMax(0, count));
    statusBar()->showMessage(tr("Replaced %1 occurrence(s)").arg(count), 4000);
}

void MainWindow::onFindTriggered()
{
    m_findBar->setFindText(m_editor->textCursor().selectedText());
    m_findBar->showForFind();
}

void MainWindow::onReplaceTriggered()
{
    m_findBar->setFindText(m_editor->textCursor().selectedText());
    m_findBar->showForReplace();
}

// ---------------------------------------------------------------------------
// Theme / font / spell actions
// ---------------------------------------------------------------------------

void MainWindow::setThemeMode(int mode)
{
    m_themeMode = static_cast<theme::Mode>(mode);
    qApp->setPalette(theme::paletteFor(m_themeMode));
    m_highlighter->setColors(theme::syntaxColors(m_themeMode));
    m_outline->setColors(palette().color(QPalette::Window), palette().color(QPalette::Text));
    refreshToolbarIcons();
}

void MainWindow::onLargerFont()
{
    QFont f = m_editor->font();
    f.setPointSize(f.pointSize() + 1);
    m_editor->setEditorFont(f);
}

void MainWindow::onSmallerFont()
{
    QFont f = m_editor->font();
    f.setPointSize(qMax(8, f.pointSize() - 1));
    m_editor->setEditorFont(f);
}

void MainWindow::onChooseFont()
{
    bool ok = false;
    const QFont chosen = QFontDialog::getFont(&ok, m_editor->font(), this,
                                              tr("Choose Editor Font"));
    if (!ok)
        return;
    m_editor->setEditorFont(chosen);
    m_fontFamily = chosen.family();
    m_fontSize = chosen.pointSize();
}

// ---------------------------------------------------------------------------
// Outline
// ---------------------------------------------------------------------------

void MainWindow::rebuildOutline()
{
    QVector<QPair<int, QString>> headings;
    QVector<int> positions;
    const QTextBlock block = m_editor->document()->firstBlock();
    for (QTextBlock b = block; b.isValid(); b = b.next()) {
        QString title;
        const int level = MarkdownHighlighter::headingLevel(b.text(), &title);
        if (level > 0) {
            headings.append({ level, title });
            positions.append(b.position());
        }
    }
    m_outline->setHeadings(headings, positions);
    m_outline->setActiveHeading(m_editor->textCursor().position());
}

void MainWindow::onOutlineActivated(int cursorPosition)
{
    QTextCursor cursor = m_editor->textCursor();
    cursor.setPosition(cursorPosition);
    m_editor->setTextCursor(cursor);
    m_editor->centerCursor();
    m_editor->setFocus();
}

// ---------------------------------------------------------------------------
// Status bar / window title / recents
// ---------------------------------------------------------------------------

void MainWindow::updateWindowTitle()
{
    QString name = tr("Untitled");
    if (!m_currentFile.isEmpty())
        name = QFileInfo(m_currentFile).fileName();
    setWindowTitle(QStringLiteral("%1[*] - %2").arg(name, tr("Markdown Editor")));
}

void MainWindow::updateCounts()
{
    const QString text = m_editor->toPlainText();
    const int words = int(text.split(QRegularExpression(QStringLiteral("\\s+")),
                                     Qt::SkipEmptyParts).size());
    const int lines = m_editor->document()->blockCount();
    m_statsLabel->setText(tr("Words: %1    Characters: %2    Lines: %3")
                              .arg(words)
                              .arg(text.size())
                              .arg(lines));
}

void MainWindow::addRecentFile(const QString &path)
{
    QSettings settings(settingsOrg(), settingsApp());
    QStringList files = settings.value(QStringLiteral("recentFiles")).toStringList();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > kMaxRecentFiles)
        files.removeLast();
    settings.setValue(QStringLiteral("recentFiles"), files);
    updateRecentMenu();
}

void MainWindow::updateRecentMenu()
{
    if (!m_recentMenu)
        return;
    m_recentMenu->clear();
    QSettings settings(settingsOrg(), settingsApp());
    const QStringList files = settings.value(QStringLiteral("recentFiles")).toStringList();
    if (files.isEmpty()) {
        QAction *empty = m_recentMenu->addAction(tr("(no recent files)"));
        empty->setEnabled(false);
        return;
    }
    for (const QString &file : files) {
        m_recentMenu->addAction(QDir::toNativeSeparators(file), this, [this, file] {
            if (maybeSave())
                loadFile(file);
        });
    }
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

void MainWindow::readSettings()
{
    QSettings settings(settingsOrg(), settingsApp());
    const QByteArray geometry = settings.value(QStringLiteral("windowGeometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    else
        resize(1200, 750);

    m_themeMode = static_cast<theme::Mode>(settings.value(QStringLiteral("themeMode"), 0).toInt());
    setThemeMode(int(m_themeMode));
    switch (m_themeMode) {
    case theme::Mode::Light: m_actionThemeLight->setChecked(true); break;
    case theme::Mode::Dark: m_actionThemeDark->setChecked(true); break;
    default: m_actionThemeAuto->setChecked(true); break;
    }

    m_fontFamily = settings.value(QStringLiteral("editorFontFamily")).toString();
    m_fontSize = settings.value(QStringLiteral("editorFontSize"), 11).toInt();
    if (!m_fontFamily.isEmpty() && m_fontSize > 0) {
        QFont font(m_fontFamily, m_fontSize);
        font.setFixedPitch(true);
        m_editor->setEditorFont(font);
    }

    m_actionOutline->setChecked(settings.value(QStringLiteral("outlineVisible"), false).toBool());
}

void MainWindow::writeSettings()
{
    QSettings settings(settingsOrg(), settingsApp());
    settings.setValue(QStringLiteral("windowGeometry"), saveGeometry());
    settings.setValue(QStringLiteral("themeMode"), int(m_themeMode));
    settings.setValue(QStringLiteral("editorFontFamily"), m_fontFamily);
    settings.setValue(QStringLiteral("editorFontSize"), m_fontSize);
    settings.setValue(QStringLiteral("outlineVisible"), m_actionOutline->isChecked());
}

// ---------------------------------------------------------------------------
// Autosave / recovery
// ---------------------------------------------------------------------------

QString MainWindow::recoveryFilePath() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(base);
    return base + QStringLiteral("/recovery.json");
}

void MainWindow::attemptRecoveryLoad()
{
    QFile file(recoveryFilePath());
    if (!file.exists())
        return;

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Recover Session"),
        tr("An autosave from a previous session was found.\nRestore it?"),
        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        file.remove();
        return;
    }

    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    const QJsonObject root = doc.object();
    m_editor->setPlainText(root.value(QStringLiteral("content")).toString());
    const QString path = root.value(QStringLiteral("path")).toString();
    setCurrentFile(path);
    if (!path.isEmpty())
        m_preview->setDocumentDirectory(QFileInfo(path).absolutePath());
    setWindowModified(true);
    updateCounts();
    rebuildOutline();
}

void MainWindow::writeRecoveryFile()
{
    if (m_currentFile.isEmpty() && m_editor->toPlainText().isEmpty())
        return; // nothing worth recovering

    QJsonObject root;
    root.insert(QStringLiteral("path"), m_currentFile);
    root.insert(QStringLiteral("content"), m_editor->toPlainText());
    root.insert(QStringLiteral("timestamp"),
                QDateTime::currentDateTime().toString(Qt::ISODate));

    QFile file(recoveryFilePath());
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void MainWindow::clearRecoveryFile()
{
    QFile::remove(recoveryFilePath());
}

// ---------------------------------------------------------------------------
// Drag and drop
// ---------------------------------------------------------------------------

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    if (isMarkdownFile(urls.first().toLocalFile()))
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    const QString path = urls.first().toLocalFile();
    if (isMarkdownFile(path) && maybeSave())
        loadFile(path);
    event->acceptProposedAction();
}

// ---------------------------------------------------------------------------
// Image paste
// ---------------------------------------------------------------------------

void MainWindow::onImagePasted(const QImage &image)
{
    if (image.isNull())
        return;

    if (m_currentFile.isEmpty()) {
        QMessageBox::information(
            this, tr("Save Document First"),
            tr("Save the document before pasting images so they can be stored next to it."));
        if (!saveFileAs() || m_currentFile.isEmpty())
            return;
    }

    const QDir docDir = QFileInfo(m_currentFile).dir();
    if (!docDir.exists(QStringLiteral("assets")) && !docDir.mkpath(QStringLiteral("assets"))) {
        QMessageBox::warning(this, tr("Image Paste Error"),
                             tr("Cannot create the assets directory next to %1.")
                                 .arg(m_currentFile));
        return;
    }

    const QString relative = QStringLiteral("assets/pasted-%1.png")
                                 .arg(QDateTime::currentMSecsSinceEpoch());
    const QString absolute = docDir.filePath(relative);
    if (!image.save(absolute, "PNG")) {
        QMessageBox::warning(this, tr("Image Paste Error"),
                             tr("Cannot write image to %1.").arg(absolute));
        return;
    }

    m_editor->insertMarkdownImage(QStringLiteral("![image](%1)").arg(relative));
}

// ---------------------------------------------------------------------------
// Close
// ---------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        writeSettings();
        clearRecoveryFile();
        event->accept();
    } else {
        event->ignore();
    }
}
