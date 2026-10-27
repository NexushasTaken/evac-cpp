#include <QApplication>

#include "ui/app_context.h"
#include "ui/main_window.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Evac");
    QGuiApplication::setDesktopFileName("io.github.NexushasTaken.Evac");

    AppContext ctx(default_locations());
    MainWindow window(ctx);
    window.show();
    return app.exec();
}
