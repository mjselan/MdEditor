// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCommandLineParser>

#include "appicons.h"
#include "mainwindow.h"
#include "theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
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

    const QStringList files = parser.positionalArguments();
    if (!files.isEmpty())
        window.openPathFromCommandLine(files.first());

    return app.exec();
}
