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
#include "ui/tab_snapshots.h"
#include "ui/tab_soundpacks.h"
#include "ui/tab_trash.h"
#include "ui/task_runner.h"

class MainWindow : public Gtk::ApplicationWindow
{
public:
    explicit MainWindow(AppContext& ctx);

private:
    bool on_close();
    void save_window_state();
    void on_switch_page(Gtk::Widget* page, guint page_number);
    void on_task_finished();
    void show_page(Gtk::Widget* page);
    void check_trash_limit();

    AppContext& ctx_;
    bool page_refresh_pending_;  // A tab was opened while a job was running.
    std::uint64_t trash_size_seen_;  // To notice when the trash grows.
    TaskRunner runner_;
    Gtk::Box layout_;
    Gtk::Notebook tabs_;
    TabGame tab_game_;
    TabMods tab_mods_;
    TabSoundpacks tab_soundpacks_;
    TabFonts tab_fonts_;
    TabSnapshots tab_snapshots_;
    TabBackups tab_backups_;
    TabTrash tab_trash_;
    TabSettings tab_settings_;
    LogView log_view_;
};

#endif
