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

    // Call when settings that the tab depends on may have changed (on the Settings tab).
    void apply_settings();

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
    void on_install_new_clicked();
    void on_update_clicked();
    void on_play_clicked();
    void on_resume_clicked();
    void on_make_active_clicked();
    void on_delete_clicked();
    void on_installs_row_selected(Gtk::ListBoxRow* row);
    void on_installs_row_activated(Gtk::ListBoxRow* row);

    void install_selected(bool update_active);
    std::string removal_detail(const std::string& install_name, bool update);
    void start_game(const std::string& world);
    void open_folder(const std::string& dir);

    std::string release_key();
    std::string selected_install_name();

    AppContext& ctx_;
    TaskRunner& runner_;
    Gtk::Window& parent_;

    bool updating_;  // True while we change widgets ourselves, so handlers do nothing.
    int had_install_;  // -1 before the first refresh, then 0 or 1. Used to open/close the install section once.
    std::vector<Release> releases_;
    Paths::InstallsSummary installs_;
    std::vector<std::string> install_names_;  // Same order as the rows in installs_list_.

    Gtk::Box content_;
    Gtk::DropDown game_dropdown_;
    Gtk::Label description_;

    // Play section.
    Gtk::Label active_title_;
    Gtk::Button play_button_;
    Gtk::Button resume_button_;
    Gtk::MenuButton folder_menu_button_;
    Gtk::Box play_box_;

    // Installed versions.
    Gtk::Box installs_box_;
    Gtk::ListBox installs_list_;
    Gtk::Button make_active_button_;
    Gtk::Button delete_button_;

    // Install / update section.
    Gtk::Expander install_expander_;
    Gtk::CheckButton stable_button_;
    Gtk::CheckButton experimental_button_;
    Gtk::Button refresh_button_;
    Gtk::DropDown builds_dropdown_;
    Gtk::Button install_new_button_;
    Gtk::Button update_button_;
};

#endif
