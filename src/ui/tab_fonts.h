#ifndef CATAPULT_TAB_FONTS_H
#define CATAPULT_TAB_FONTS_H

#include <gtkmm.h>

#include <vector>

#include "ui/app_context.h"

// The Fonts tab: pick one of the bundled fonts for the game UI, the map and
// the overmap, and set the font sizes. This only edits two small files, so
// it all happens on the main thread.
class TabFonts : public Gtk::Box
{
public:
    explicit TabFonts(AppContext& ctx);

    // Reloads everything from the game's config files. Call when the tab is shown.
    void refresh();

private:
    void build_ui();
    void show_unavailable(const Glib::ustring& message);
    void show_current_config();
    int selected_font_index();

    void on_font_selected(Gtk::ListBoxRow* row);
    void on_set_font(bool ui, bool map, bool overmap);
    void on_reset();
    void on_save();

    AppContext& ctx_;
    bool updating_;

    Gtk::Label hint_label_;
    Gtk::Box content_;
    Gtk::ListBox fonts_list_;
    Gtk::Label description_label_;
    Gtk::Button set_ui_button_;
    Gtk::Button set_map_button_;
    Gtk::Button set_overmap_button_;
    Gtk::Button set_all_button_;
    Gtk::Button reset_button_;
    Gtk::Label current_config_label_;
    Gtk::SpinButton size_ui_;
    Gtk::SpinButton size_map_;
    Gtk::SpinButton size_overmap_;
    Gtk::CheckButton blending_check_;
    Gtk::Button save_button_;
};

#endif
