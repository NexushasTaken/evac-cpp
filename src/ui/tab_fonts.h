#ifndef EVAC_TAB_FONTS_H
#define EVAC_TAB_FONTS_H

#include <gtkmm.h>

#include <string>
#include <vector>

#include "ui/app_context.h"
#include "ui/font_preview.h"

// The Fonts tab: pick a bundled font or one from a file for each font list of
// the game, edit those lists (order, hinting, antialiasing), and set the font
// sizes. This only edits two small files, so it all happens on the main thread.
class TabFonts : public Gtk::Box
{
public:
    TabFonts(AppContext& ctx, Gtk::Window& parent);

    // Reloads everything from the game's config files. Call when the tab is shown.
    void refresh();

private:
    void build_ui();
    void build_sizes(Gtk::Box& box);
    void show_unavailable(const Glib::ustring& message);
    void fill_fonts_list(int select);
    int selected_font_index();
    void update_preview();

    // The font list picked in the dropdown ("typeface", ...).
    std::string current_field();
    void show_entries();
    int selected_entry();
    void update_entry_buttons();

    void on_font_selected();
    void on_add_font_file();
    void add_font_file(const std::string& path);
    void on_set_font(const std::vector<std::string>& fields);
    void on_reset();
    void on_entry_style_changed(size_t index, Gtk::DropDown* hinting, Gtk::CheckButton* antialiasing);
    void on_move_entry(int offset);
    void on_remove_entry();
    void on_size_changed();
    void on_manual_toggled();
    void on_save();

    AppContext& ctx_;
    Gtk::Window& parent_;
    bool updating_;
    std::vector<FontEntry> entries_;  // As shown in entries_list_.

    Gtk::Label hint_label_;
    Gtk::ScrolledWindow scroller_;
    Gtk::Box content_;

    Gtk::ListBox fonts_list_;
    Gtk::Button add_file_button_;
    Gtk::Label description_label_;
    FontPreview preview_;
    Gtk::Button set_ui_button_;
    Gtk::Button set_menus_button_;
    Gtk::Button set_map_button_;
    Gtk::Button set_overmap_button_;
    Gtk::Button set_all_button_;
    Gtk::Button reset_button_;

    Gtk::DropDown field_dropdown_;
    Gtk::Label missing_field_label_;
    Gtk::ListBox entries_list_;
    Gtk::Button move_up_button_;
    Gtk::Button move_down_button_;
    Gtk::Button remove_button_;

    // UI (also the menus), map, overmap.
    Gtk::SpinButton size_spins_[3];
    Gtk::SpinButton width_spins_[3];
    Gtk::SpinButton height_spins_[3];
    Gtk::CheckButton manual_check_;
    Gtk::CheckButton blending_check_;
    Gtk::CheckButton ascii_check_;
    Gtk::Button save_button_;
};

#endif
