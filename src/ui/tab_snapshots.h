#ifndef EVAC_TAB_SNAPSHOTS_H
#define EVAC_TAB_SNAPSHOTS_H

#include <gtkmm.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "core/snapshot_manager.h"
#include "ui/app_context.h"
#include "ui/task_runner.h"

// The Snapshots tab: git snapshots of each world, to take, restore or discard.
// See docs/git-backups.md.
class TabSnapshots : public Gtk::Box
{
public:
    TabSnapshots(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent);

    // Reloads the worlds and their status. Call when the tab is shown.
    void refresh();

private:
    void build_ui();
    void show_unavailable(const Glib::ustring& message);
    void reload();
    void fill_worlds(const std::vector<WorldInfo>& worlds);
    void fill_snapshots();
    void update_buttons();
    void update_details();
    void ask_about_deleted_worlds();
    void run_task(const std::function<void()>& work);
    bool check_writable();
    const WorldInfo* selected_world();
    const Snapshot* selected_snapshot();

    void on_world_selected();
    void on_snapshot_selected();
    void on_take_clicked();
    void on_take_all_clicked();
    void on_restore_clicked();
    void on_discard_clicked();
    void on_copy_clicked();
    void on_export_clicked();
    void on_import_clicked();
    void on_start_fresh_clicked();
    void on_delete_clicked();
    void import_bundle(const std::string& path, const std::string& world);

    AppContext& ctx_;
    TaskRunner& runner_;
    Gtk::Window& parent_;
    bool updating_;
    std::vector<WorldInfo> worlds_;
    std::vector<Snapshot> snapshots_;
    std::string selected_name_;  // Kept across reloads.
    std::set<std::string> asked_;  // Deleted worlds already asked about.

    Gtk::Label hint_label_;
    Gtk::Box content_;
    Gtk::ListBox worlds_list_;
    Gtk::Label snapshots_title_;
    Gtk::MenuButton menu_button_;
    Gtk::Button export_button_;
    Gtk::Button import_button_;
    Gtk::Button start_fresh_button_;
    Gtk::Button delete_button_;
    Gtk::ListBox snapshots_list_;
    Gtk::Button restore_button_;
    Gtk::Button discard_button_;
    Gtk::Label details_label_;
    Gtk::Entry name_entry_;
    Gtk::Entry description_entry_;
    Gtk::Button take_button_;
    Gtk::Button take_all_button_;
    Gtk::Label footer_label_;
    Gtk::Label command_label_;
    Gtk::Button copy_button_;
};

#endif
