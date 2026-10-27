#include <gtkmm.h>

#include "ui/app_context.h"
#include "ui/main_window.h"

int main(int argc, char* argv[])
{
    AppContext ctx(default_locations());

    Glib::RefPtr<Gtk::Application> app = Gtk::Application::create("io.github.NexushasTaken.Evac");
    return app->make_window_and_run<MainWindow>(argc, argv, std::ref(ctx));
}
