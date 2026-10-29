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

  AppContext& m_ctx;
  QWidget& m_parent;
  bool m_updating;
  std::vector<FontEntry> m_entries;  // As shown in m_entries_list.

  QLabel* m_hint_label;
  QScrollArea* m_scroller;

  QListWidget* m_fonts_list;
  QPushButton* m_add_file_button;
  QLabel* m_description_label;
  FontPreview* m_preview;
  QPushButton* m_set_ui_button;
  QPushButton* m_set_menus_button;
  QPushButton* m_set_map_button;
  QPushButton* m_set_overmap_button;
  QPushButton* m_set_all_button;
  QPushButton* m_reset_button;

  QComboBox* m_field_dropdown;
  QLabel* m_missing_field_label;
  QListWidget* m_entries_list;
  QPushButton* m_move_up_button;
  QPushButton* m_move_down_button;
  QPushButton* m_remove_button;

  // UI (also the menus), map, overmap.
  QSpinBox* m_size_spins[3];
  QSpinBox* m_width_spins[3];
  QSpinBox* m_height_spins[3];
  QCheckBox* m_manual_check;
  QCheckBox* m_blending_check;
  QCheckBox* m_ascii_check;
  QPushButton* m_save_button;
};

#endif
