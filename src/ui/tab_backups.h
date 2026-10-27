#ifndef EVAC_TAB_BACKUPS_H
#define EVAC_TAB_BACKUPS_H

#include <gtkmm.h>

#include <functional>
#include <string>
#include <vector>

#include "core/backup_manager.h"
#include "ui/app_context.h"
#include "ui/task_runner.h"

// The Backups tab: back up the saved games, and restore or delete backups.
class TabBackups : public Gtk::Box
{
public:
    TabBackups(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent);

    // Reloads the list of backups. Call when the tab is shown.
    void refresh();

private:
    void build_ui();
    void show_unavailable(const Glib::ustring& message);
    void reload_list();
    void update_buttons();
    void run_task(const std::function<void()>& work);
    int selected_backup();

    void on_selection_changed();
    void on_name_changed();
    void on_create_clicked();
    void on_restore_clicked();
    void confirm_restore(const std::string& name, const std::vector<std::string>& changed);
    void on_delete_clicked();

    AppContext& ctx_;
    TaskRunner& runner_;
    Gtk::Window& parent_;
    bool updating_;
    std::vector<BackupSummary> backups_;

    Gtk::Label hint_label_;
    Gtk::Box content_;
    Gtk::ListBox list_;
    Gtk::Button restore_button_;
    Gtk::Button delete_button_;
    Gtk::Button refresh_button_;
    Gtk::Label info_label_;
    Gtk::Entry name_entry_;
    Gtk::Button create_button_;
};

#endif
