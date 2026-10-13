// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCommandLineParser>
#include <QFileOpenEvent>
#include <QFileInfo>

#include "appicons.h"
#include "mainwindow.h"
#include "theme.h"

// Subclass QApplication to intercept native macOS file opening (Finder / "Open With")
class Application : public QApplication
{
public:
    using QApplication::QApplication;

    void setMainWindow(MainWindow *window)
    {
        m_mainWindow = window;
        if (!m_pendingFile.isEmpty() && m_mainWindow) {
            m_mainWindow->openPathFromCommandLine(m_pendingFile);
            m_pendingFile.clear();
        }
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::FileOpen) {
            auto *openEvent = static_cast<QFileOpenEvent *>(event);
            const QString path = QFileInfo(openEvent->file()).absoluteFilePath();
            if (m_mainWindow) {
                m_mainWindow->openPathFromCommandLine(path);
            } else {
                m_pendingFile = path;
            }
            return true;
        }
        return QApplication::event(event);
    }

private:
    MainWindow *m_mainWindow = nullptr;
    QString m_pendingFile;
};

int main(int argc, char *argv[])
{
    Application app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("MdEditor"));
    QApplication::setApplicationName(QStringLiteral("MarkdownEditor"));
    QApplication::setApplicationVersion(QStringLiteral(MARKDOWNEDITOR_VERSION));
    QApplication::setWindowIcon(appicons::applicationIcon());

    // Default to the OS color scheme until the user overrides it in settings.
    app.setPalette(theme::paletteFor(theme::Mode::Auto));

    // arguments() decodes argv as Unicode on Windows (fromLocal8Bit would
    // mangle non-ASCII paths); --help/--version come for free.
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Lightweight offline Markdown editor."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"),
                                 QStringLiteral("Markdown file to open."),
                                 QStringLiteral("[file]"));
    parser.process(app);

    MainWindow window;
    window.show();
    app.setMainWindow(&window);

    const QStringList files = parser.positionalArguments();
    if (!files.isEmpty()) {
        const QString absolute = QFileInfo(files.first()).absoluteFilePath();
        window.openPathFromCommandLine(absolute);
    }

    return app.exec();
}