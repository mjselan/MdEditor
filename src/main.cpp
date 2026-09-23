#include <QApplication>

#include "appicons.h"
#include "mainwindow.h"
#include "theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("MdEditor"));
    QApplication::setApplicationName(QStringLiteral("MarkdownEditor"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setWindowIcon(appicons::applicationIcon());

    // Default to the OS color scheme until the user overrides it in settings.
    app.setPalette(theme::paletteFor(theme::Mode::Auto));

    MainWindow window;
    window.show();

    if (argc > 1)
        window.openPathFromCommandLine(QString::fromLocal8Bit(argv[1]));

    return app.exec();
}
