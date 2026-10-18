#ifndef CATAPULT_TAB_SOUNDPACKS_H
#define CATAPULT_TAB_SOUNDPACKS_H

#include <gtkmm.h>

#include <functional>
#include <string>
#include <vector>

#include "core/soundpack_manager.h"
#include "ui/app_context.h"
#include "ui/task_runner.h"

// The Soundpacks tab: installed packs on the left, downloadable ones on the right.
class TabSoundpacks : public Gtk::Box
{
public:
    TabSoundpacks(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent);

    // Reloads the lists from disk. Call when the tab is shown.
    void refresh();

private:
    void build_ui();
    void show_unavailable(const Glib::ustring& message);
    void refresh_installed();
    void update_buttons();
    void run_task(const std::function<void()>& work);

    int selected_installed();
    int selected_available();

    void on_show_stock_toggled();
    void on_installed_selected();
    void on_available_selected();
    void on_toggle_clicked();
    void on_delete_clicked();
    void on_install_clicked();
    void on_install_from_file_clicked();
    void install_selected(const std::string& from_file);

    AppContext& ctx_;
    TaskRunner& runner_;
    Gtk::Window& parent_;
    bool updating_;
    std::vector<InstalledSoundpack> installed_;

    Gtk::Label hint_label_;
    Gtk::Box content_;
    Gtk::ListBox installed_list_;
    Gtk::ListBox available_list_;
    Gtk::CheckButton show_stock_check_;
    Gtk::Button toggle_button_;
    Gtk::Button delete_button_;
    Gtk::Button install_button_;
    Gtk::Button install_from_file_button_;
};

#endif
