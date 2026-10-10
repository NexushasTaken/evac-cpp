#include <gtkmm.h>

#include "ui/app_context.h"
#include "ui/main_window.h"

int main(int argc, char* argv[])
{
    AppContext ctx(Paths::executable_dir());

    Glib::RefPtr<Gtk::Application> app = Gtk::Application::create("org.catapult.launcher");
    return app->make_window_and_run<MainWindow>(argc, argv, std::ref(ctx));
}
