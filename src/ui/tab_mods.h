#ifndef CATAPULT_TAB_MODS_H
#define CATAPULT_TAB_MODS_H

#include <gtkmm.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "ui/app_context.h"
#include "ui/task_runner.h"

// The Mods tab: the installed mods on the left, the local repository (where
// the Kenan modpack is unpacked to) on the right, and details of the selected
// mod at the bottom.
class TabMods : public Gtk::Box
{
public:
    TabMods(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent);

    // Reads the mods from disk and updates both lists. Call when the tab is shown.
    void refresh();

private:
    // What a row of a list stands for.
    struct ModRow
    {
        std::string id;
        std::string name;
        std::string location;
        bool greyed;
    };

    void build_ui();
    void show_unavailable(const Glib::ustring& message);
    void reload_installed();
    void reload_available();
    void show_mod_info(const std::string& id, bool from_available);
    void run_task(const std::function<void()>& work);
    void ask(const Glib::ustring& title, const Glib::ustring& detail, const std::vector<Glib::ustring>& buttons,
             const std::function<void(int)>& answer);

    std::set<int> selected_rows(Gtk::ListBox& list);
    void fill_list(Gtk::ListBox& list, const std::vector<ModRow>& rows);

    void on_installed_selection_changed();
    void on_available_selection_changed();
    void on_show_stock_toggled();
    void on_show_installed_toggled();
    void on_delete_clicked();
    void on_add_selected_clicked();
    void on_add_all_clicked();
    void on_kenan_clicked();
    void install_mods(const std::vector<std::string>& new_ids, const std::vector<std::string>& reinstall_ids);

    AppContext& ctx_;
    TaskRunner& runner_;
    Gtk::Window& parent_;
    bool updating_;
    std::vector<ModRow> installed_rows_;
    std::vector<ModRow> available_rows_;
    std::set<int> previous_installed_selection_;
    std::set<int> previous_available_selection_;

    Gtk::Label hint_label_;
    Gtk::Box content_;
    Gtk::Label installed_caption_;
    Gtk::Label available_caption_;
    Gtk::ListBox installed_list_;
    Gtk::ListBox available_list_;
    Gtk::CheckButton show_stock_check_;
    Gtk::CheckButton show_installed_check_;
    Gtk::Button delete_button_;
    Gtk::Button add_selected_button_;
    Gtk::Button add_all_button_;
    Gtk::Button kenan_button_;
    Gtk::Label info_label_;
};

#endif
