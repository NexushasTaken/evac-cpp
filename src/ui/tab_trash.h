#ifndef EVAC_TAB_TRASH_H
#define EVAC_TAB_TRASH_H

#include <gtkmm.h>

#include <functional>
#include <string>
#include <vector>

#include "core/trash.h"
#include "ui/app_context.h"
#include "ui/task_runner.h"

// The Trash tab: what was deleted, to restore it or delete it for good.
class TabTrash : public Gtk::Box
{
public:
    TabTrash(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent);

    // Reloads the list. Call when the tab is shown.
    void refresh();

    // Asks to delete the oldest items until the trash is under its size limit
    // (what Free Up Space does). Says so if it is not over the limit.
    void ask_free_up();

private:
    void build_ui();
    void update_summary();
    void update_buttons();
    void run_task(const std::function<void()>& work);
    int selected_item();

    void on_selection_changed();
    void on_restore_clicked();
    void on_delete_clicked();
    void on_empty_clicked();
    void on_free_up_clicked();
    void on_open_folder_clicked();

    AppContext& ctx_;
    TaskRunner& runner_;
    Gtk::Window& parent_;
    bool updating_;
    std::vector<TrashItem> items_;

    Gtk::Label summary_label_;
    Gtk::Box content_;
    Gtk::ListBox list_;
    Gtk::Label info_label_;
    Gtk::Button restore_button_;
    Gtk::Button delete_button_;
    Gtk::Button empty_button_;
    Gtk::Button free_up_button_;
    Gtk::Button open_folder_button_;
};

#endif
