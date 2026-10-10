#ifndef CATAPULT_TAB_GAME_H
#define CATAPULT_TAB_GAME_H

#include <gtkmm.h>

#include <functional>
#include <string>
#include <vector>

#include "core/paths.h"
#include "core/release_data.h"
#include "ui/app_context.h"
#include "ui/task_runner.h"

// The Game tab: choose a game, install or update a release, launch it.
class TabGame : public Gtk::Box
{
public:
    TabGame(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent);

    // Call once after the window exists; loads the release list.
    void start();

private:
    void build_ui();

    // Settings -> widgets.
    void apply_game_choice();
    void reload_builds_list();
    void refresh_installed();

    // Slow jobs, run on the worker thread.
    void fetch_releases();
    void run_task(const std::function<void()>& work, const std::function<void()>& after);

    // Widget handlers.
    void on_game_selected();
    void on_channel_toggled();
    void on_build_selected();
    void on_update_current_toggled();
    void on_install_clicked();
    void on_play_clicked();
    void on_resume_clicked();
    void on_make_active_clicked();
    void on_delete_clicked();
    void on_installs_row_selected(Gtk::ListBoxRow* row);
    void on_installs_row_activated(Gtk::ListBoxRow* row);

    void confirm_game_safe_to_delete(const std::string& install_name, const std::function<void(bool)>& answer);
    void start_game(const std::string& world);
    void open_folder(const std::string& dir);

    std::string release_key();
    std::string selected_install_name();

    AppContext& ctx_;
    TaskRunner& runner_;
    Gtk::Window& parent_;

    bool updating_;  // True while we change widgets ourselves, so handlers do nothing.
    std::vector<Release> releases_;
    Paths::InstallsSummary installs_;
    std::vector<std::string> install_names_;  // Same order as the rows in installs_list_.

    Gtk::Box content_;
    Gtk::DropDown game_dropdown_;
    Gtk::Label description_;
    Gtk::CheckButton stable_button_;
    Gtk::CheckButton experimental_button_;
    Gtk::Button refresh_button_;
    Gtk::DropDown builds_dropdown_;
    Gtk::CheckButton update_current_check_;
    Gtk::Button install_button_;
    Gtk::Label active_install_label_;
    Gtk::Button game_dir_button_;
    Gtk::Button user_dir_button_;
    Gtk::Button play_button_;
    Gtk::Button resume_button_;
    Gtk::Box installs_box_;
    Gtk::ListBox installs_list_;
    Gtk::Button make_active_button_;
    Gtk::Button delete_button_;
};

#endif
