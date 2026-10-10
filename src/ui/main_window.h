#ifndef CATAPULT_MAIN_WINDOW_H
#define CATAPULT_MAIN_WINDOW_H

#include <gtkmm.h>

#include "ui/app_context.h"
#include "ui/log_view.h"
#include "ui/tab_backups.h"
#include "ui/tab_fonts.h"
#include "ui/tab_mods.h"
#include "ui/tab_game.h"
#include "ui/tab_settings.h"
#include "ui/tab_soundpacks.h"
#include "ui/task_runner.h"

class MainWindow : public Gtk::ApplicationWindow
{
public:
    explicit MainWindow(AppContext& ctx);

private:
    bool on_close();
    void on_switch_page(Gtk::Widget* page, guint page_number);
    void on_task_finished();
    void show_page(Gtk::Widget* page);

    AppContext& ctx_;
    bool page_refresh_pending_;  // A tab was opened while a job was running.
    TaskRunner runner_;
    Gtk::Box layout_;
    Gtk::Notebook tabs_;
    TabGame tab_game_;
    TabMods tab_mods_;
    TabSoundpacks tab_soundpacks_;
    TabFonts tab_fonts_;
    TabBackups tab_backups_;
    TabSettings tab_settings_;
    LogView log_view_;
};

#endif
