// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAction>
#include <QImage>
#include <QMainWindow>
#include <QTimer>

#include "theme.h"

class FindReplaceBar;
class SpellChecker;
class QTextDocument;
class MarkdownEditor;
class MarkdownHighlighter;
class MarkdownPreview;
class OutlinePanel;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QDockWidget;
class QLabel;
class QMenu;
class QSplitter;
class QToolBar;

// Main window: menus, toolbar, status bar, file management (new/open/save/
// recents/drag-and-drop), HTML+PDF export, autosave recovery, theme and font
// persistence, and wiring between editor, highlighter, preview, outline and
// find/replace components.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Opens the given document; used for command-line file arguments.
    void openPathFromCommandLine(const QString &path);

    // Counts occurrences of text in the document without touching any editor
    // caret. Exposed statically for unit testing.
    static int countMatchesInDocument(QTextDocument *document, const QString &text,
                                      bool matchCase);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void exportHtml();
    void exportPdf();
    void printFile();

    void onTextChanged();
    void onCursorPositionChanged();
    void onEditorScrolled(int value);
    void onPreviewScrolled(double ratio);
    void onImagePasted(const QImage &image);

    void findNext(const QString &text, bool matchCase, bool backward);
    void countMatches(const QString &text, bool matchCase);
    void replaceCurrent(const QString &findText, const QString &replaceText, bool matchCase);
    void replaceAll(const QString &findText, const QString &replaceText, bool matchCase);
    void onFindTriggered();
    void onReplaceTriggered();

    void setThemeMode(int mode);
    void onLargerFont();
    void onSmallerFont();
    void onChooseFont();

    void rebuildOutline();
    void onOutlineActivated(int cursorPosition);

private:
    void createWidgets();
    void createActions();
    void createMenus();
    void createToolbars();
    void createStatusBar();
    void connectSignals();
    void refreshToolbarIcons();

    bool loadFile(const QString &path);
    bool saveToFile(const QString &path);
    void setCurrentFile(const QString &path);
    bool maybeSave();

    void readSettings();
    void writeSettings();

    void updateWindowTitle();
    void updateCounts();
    void addRecentFile(const QString &path);
    void updateRecentMenu();

    QString recoveryFilePath() const;
    void attemptRecoveryLoad();
    void writeRecoveryFile();
    void clearRecoveryFile();

    MarkdownEditor *m_editor = nullptr;
    MarkdownPreview *m_preview = nullptr;
    OutlinePanel *m_outline = nullptr;
    FindReplaceBar *m_findBar = nullptr;
    MarkdownHighlighter *m_highlighter = nullptr;
    SpellChecker *m_spellChecker = nullptr;

    QSplitter *m_splitter = nullptr;
    QDockWidget *m_outlineDock = nullptr;
    QLabel *m_statsLabel = nullptr;
    QMenu *m_recentMenu = nullptr;

    QTimer m_countTimer;
    QTimer m_outlineTimer;
    QTimer m_autosaveTimer;

    QString m_currentFile;
    theme::Mode m_themeMode = theme::Mode::Auto;
    QString m_fontFamily;
    int m_fontSize = 11;

    // Actions
    QAction *m_actionNew = nullptr;
    QAction *m_actionOpen = nullptr;
    QAction *m_actionSave = nullptr;
    QAction *m_actionSaveAs = nullptr;
    QAction *m_actionExportHtml = nullptr;
    QAction *m_actionExportPdf = nullptr;
    QAction *m_actionPrint = nullptr;
    QAction *m_actionQuit = nullptr;
    QAction *m_actionUndo = nullptr;
    QAction *m_actionRedo = nullptr;
    QAction *m_actionFind = nullptr;
    QAction *m_actionReplace = nullptr;
    QAction *m_actionThemeAuto = nullptr;
    QAction *m_actionThemeLight = nullptr;
    QAction *m_actionThemeDark = nullptr;
    QAction *m_actionOutline = nullptr;
    QAction *m_actionLargerFont = nullptr;
    QAction *m_actionSmallerFont = nullptr;
    QAction *m_actionChooseFont = nullptr;
    QAction *m_actionSpellCheck = nullptr;

    // Format actions (shared by Format menu and Format toolbar).
    QAction *m_actionBold = nullptr;
    QAction *m_actionItalic = nullptr;
    QAction *m_actionStrike = nullptr;
    QAction *m_actionH1 = nullptr;
    QAction *m_actionH2 = nullptr;
    QAction *m_actionH3 = nullptr;
    QAction *m_actionNormal = nullptr;
    QAction *m_actionLink = nullptr;
    QAction *m_actionInlineCode = nullptr;
    QAction *m_actionCodeBlock = nullptr;
    QAction *m_actionBlockquote = nullptr;
    QAction *m_actionBulletList = nullptr;
    QAction *m_actionNumberedList = nullptr;

    QToolBar *m_toolBarFile = nullptr;
    QToolBar *m_toolBarFormat = nullptr;
};
