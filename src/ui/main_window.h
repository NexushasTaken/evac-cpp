#ifndef CATAPULT_MAIN_WINDOW_H
#define CATAPULT_MAIN_WINDOW_H

#include <gtkmm.h>

#include "ui/app_context.h"
#include "ui/log_view.h"
#include "ui/tab_game.h"
#include "ui/task_runner.h"

class MainWindow : public Gtk::ApplicationWindow
{
public:
    explicit MainWindow(AppContext& ctx);

private:
    bool on_close();

    AppContext& ctx_;
    TaskRunner runner_;
    Gtk::Box layout_;
    Gtk::Notebook tabs_;
    TabGame tab_game_;
    LogView log_view_;
};

#endif
