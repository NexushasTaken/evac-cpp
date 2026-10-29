#ifndef EVAC_TAB_FONTS_H
#define EVAC_TAB_FONTS_H

#include <QWidget>
#include <string>
#include <vector>

#include "ui/app_context.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QVBoxLayout;
class FontPreview;

// The Fonts tab: pick a bundled font or one from a file for each font list of
// the game, edit those lists (order, hinting, antialiasing), and set the font
// sizes. This only edits two small files, so it all happens on the main thread.
class TabFonts : public QWidget {
 public:
  TabFonts(AppContext& ctx, QWidget& parent);

  // Reloads everything from the game's config files. Call when the tab is
  // shown.
  void refresh();

 private:
  void build_ui();
  void build_sizes(QVBoxLayout& box);
  void show_unavailable(const QString& message);
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
  void on_entry_style_changed(size_t index, QComboBox* hinting,
                              QCheckBox* antialiasing);
  void on_move_entry(int offset);
  void on_remove_entry();
  void on_size_changed();
  void on_manual_toggled();
  void on_save();

  AppContext& ctx_;
  QWidget& parent_;
  bool updating_;
  std::vector<FontEntry> entries_;  // As shown in entries_list_.

  QLabel* hint_label_;
  QScrollArea* scroller_;

  QListWidget* fonts_list_;
  QPushButton* add_file_button_;
  QLabel* description_label_;
  FontPreview* preview_;
  QPushButton* set_ui_button_;
  QPushButton* set_menus_button_;
  QPushButton* set_map_button_;
  QPushButton* set_overmap_button_;
  QPushButton* set_all_button_;
  QPushButton* reset_button_;

  QComboBox* field_dropdown_;
  QLabel* missing_field_label_;
  QListWidget* entries_list_;
  QPushButton* move_up_button_;
  QPushButton* move_down_button_;
  QPushButton* remove_button_;

  // UI (also the menus), map, overmap.
  QSpinBox* size_spins_[3];
  QSpinBox* width_spins_[3];
  QSpinBox* height_spins_[3];
  QCheckBox* manual_check_;
  QCheckBox* blending_check_;
  QCheckBox* ascii_check_;
  QPushButton* save_button_;
};

#endif
