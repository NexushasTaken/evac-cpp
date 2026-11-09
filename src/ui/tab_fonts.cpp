#include "ui/tab_fonts.h"

#include <qnamespace.h>
#include <qobject.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "core/font_manager.h"
#include "core/fs.h"
#include "core/paths.h"
#include "core/status.h"
#include "ui/app_context.h"
#include "ui/dialogs.h"
#include "ui/font_preview.h"
#include "ui/qt_helpers.h"

// Same order as FontManager::font_fields().
static const char* const FIELD_CAPTIONS[] = {
    "Game UI",
    "Menus",
    "Map",
    "Overmap",
};
static const char* const FIELD_TOOLTIPS[] = {
    "The classic game interface: the sidebar, messages and most screens.",
    "The newer menus (drawn with ImGui). They use the game UI font size.",
    "The game world, when not using tiles.",
    "The world map view, a.k.a. the overmap.",
};
// The font lists with their own sizes in options.json; the menus use the UI's.
static const char* const SIZE_FIELDS[] = {
    "typeface",
    "map_typeface",
    "overmap_typeface",
};

static QLabel* make_section_heading(const QString& text) {
  QLabel* label = make_heading(text);
  label->setContentsMargins(0, 6, 0, 0);
  return label;
}

static QFrame* make_separator() {
  auto* separator = new QFrame();
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  return separator;
}

static QSpinBox* make_spin(int value, int min, int max,
                           const QString& tooltip) {
  auto* spin = new QSpinBox();
  spin->setRange(min, max);
  spin->setValue(value);
  spin->setToolTip(tooltip);
  return spin;
}

TabFonts::TabFonts(AppContext& ctx, QWidget& parent)
    : m_ctx(ctx), m_parent(parent) {
  build_ui();
}

void TabFonts::build_ui() {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(12, 12, 12, 12);

  m_hint_label = make_label("");
  m_hint_label->setWordWrap(true);
  m_hint_label->setAlignment(
      Qt::AlignLeft | Qt::AlignTop);  // Not in the middle of the empty tab.
  outer->addWidget(m_hint_label);

  // The tab is taller than the window can be.
  m_scroller = new QScrollArea();
  m_scroller->setWidgetResizable(true);
  m_scroller->setFrameShape(QFrame::NoFrame);
  m_scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* page = new QWidget();
  auto* content = new QVBoxLayout(page);
  content->setContentsMargins(0, 0, 12, 0);  // Room for the scrollbar.
  content->setSpacing(8);
  m_scroller->setWidget(page);
  outer->addWidget(m_scroller, 1);

  // Available fonts.
  content->addWidget(make_section_heading("Available fonts"));
  m_fonts_list = new QListWidget();
  m_fonts_list->setSelectionMode(QAbstractItemView::SingleSelection);
  m_fonts_list->setMinimumHeight(150);
  content->addWidget(m_fonts_list);

  m_add_file_button = new QPushButton("Add Font File...");
  m_add_file_button->setToolTip(
      "Add a .ttf, .otf or .ttc font from your computer.\n"
      "It is copied to the game's user font folder.");
  content->addWidget(m_add_file_button, 0, Qt::AlignLeft);

  m_description_label = make_label("");
  m_description_label->setWordWrap(true);
  content->addWidget(m_description_label);
  m_preview = new FontPreview();
  content->addWidget(m_preview);

  m_set_ui_button = new QPushButton("Set for Game UI");
  m_set_ui_button->setToolTip(FIELD_TOOLTIPS[0]);
  m_set_menus_button = new QPushButton("Set for Menus");
  m_set_menus_button->setToolTip(FIELD_TOOLTIPS[1]);
  m_set_map_button = new QPushButton("Set for Map");
  m_set_map_button->setToolTip(FIELD_TOOLTIPS[2]);
  m_set_overmap_button = new QPushButton("Set for Overmap");
  m_set_overmap_button->setToolTip(FIELD_TOOLTIPS[3]);
  m_set_all_button = new QPushButton("Set for All");
  m_set_all_button->setToolTip("Use the selected font for everything.");
  auto* set_row = new QHBoxLayout();
  set_row->setSpacing(8);
  set_row->addWidget(m_set_ui_button, 1);
  set_row->addWidget(m_set_menus_button, 1);
  set_row->addWidget(m_set_map_button, 1);
  set_row->addWidget(m_set_overmap_button, 1);
  set_row->addWidget(m_set_all_button, 1);
  content->addLayout(set_row);

  // The font lists of fonts.json.
  content->addWidget(make_separator());
  content->addWidget(make_section_heading("Font lists"));
  QLabel* lists_hint = make_label(
      "The game uses the first font of a list. Characters it lacks come from "
      "the next ones, "
      "and Unifont is always the last resort.");
  lists_hint->setWordWrap(true);
  content->addWidget(lists_hint);

  m_field_dropdown = new QComboBox();
  for (const auto* i : FIELD_CAPTIONS) {
    m_field_dropdown->addItem(i);
  }
  content->addWidget(m_field_dropdown, 0, Qt::AlignLeft);
  m_missing_field_label =
      make_label("This version of the game has no font list for this.");
  content->addWidget(m_missing_field_label);

  m_entries_list = new QListWidget();
  m_entries_list->setSelectionMode(QAbstractItemView::SingleSelection);
  m_entries_list->setMinimumHeight(120);
  content->addWidget(m_entries_list);

  m_move_up_button = new QPushButton("Move Up");
  m_move_down_button = new QPushButton("Move Down");
  m_remove_button = new QPushButton("Remove...");
  m_remove_button->setToolTip(
      "Take the selected font out of this list. The font file stays.");
  m_reset_button = new QPushButton("Reset All Fonts...");
  m_reset_button->setToolTip("Set all font lists back to the game's defaults.");
  auto* entry_row = new QHBoxLayout();
  entry_row->setSpacing(8);
  entry_row->addWidget(m_move_up_button);
  entry_row->addWidget(m_move_down_button);
  entry_row->addWidget(m_remove_button);
  entry_row->addStretch(1);
  entry_row->addWidget(m_reset_button);
  content->addLayout(entry_row);

  // Sizes and other options.json settings.
  content->addWidget(make_separator());
  content->addWidget(make_section_heading("Sizes and rendering"));
  build_sizes(*content);
  content->addStretch(1);

  connect(m_fonts_list, &QListWidget::itemSelectionChanged, this,
          &TabFonts::on_font_selected);
  connect(m_add_file_button, &QPushButton::clicked, this,
          &TabFonts::on_add_font_file);
  const std::vector<std::string>& fields = FontManager::font_fields();
  connect(m_set_ui_button, &QPushButton::clicked, this,
          [this, fields] { on_set_font({fields[0]}); });
  connect(m_set_menus_button, &QPushButton::clicked, this,
          [this, fields] { on_set_font({fields[1]}); });
  connect(m_set_map_button, &QPushButton::clicked, this,
          [this, fields] { on_set_font({fields[2]}); });
  connect(m_set_overmap_button, &QPushButton::clicked, this,
          [this, fields] { on_set_font({fields[3]}); });
  connect(m_set_all_button, &QPushButton::clicked, this,
          [this, fields] { on_set_font(fields); });
  connect(m_reset_button, &QPushButton::clicked, this, &TabFonts::on_reset);
  connect(m_field_dropdown, &QComboBox::currentIndexChanged, this, [this] {
    if (!m_updating) {
      show_entries();
    }
  });
  connect(m_entries_list, &QListWidget::itemSelectionChanged, this,
          &TabFonts::update_entry_buttons);
  connect(m_move_up_button, &QPushButton::clicked, this,
          [this] { on_move_entry(-1); });
  connect(m_move_down_button, &QPushButton::clicked, this,
          [this] { on_move_entry(1); });
  connect(m_remove_button, &QPushButton::clicked, this,
          &TabFonts::on_remove_entry);
}

void TabFonts::build_sizes(QVBoxLayout& box) {
  auto* grid = new QGridLayout();
  grid->setVerticalSpacing(6);
  grid->setHorizontalSpacing(12);
  grid->addWidget(make_label("Size"), 0, 1);
  grid->addWidget(make_label("Width (px)"), 0, 2);
  grid->addWidget(make_label("Height (px)"), 0, 3);

  const char* const captions[] = {"Game UI and menus:", "Map:", "Overmap:"};
  const char* const tooltips[] = {
      "Font size for the game UI and the menus.",
      "Font size for the game world (when not using tiles).",
      "Font size for the world map view, a.k.a. the overmap."};
  for (int i = 0; i < 3; ++i) {
    // The ranges the game accepts.
    m_size_spins[i] = make_spin(16, 8, 100, tooltips[i]);
    m_width_spins[i] = make_spin(8, 6, 100, "Width of a character cell.");
    m_height_spins[i] = make_spin(16, 8, 100, "Height of a character cell.");
    grid->addWidget(make_label(captions[i]), i + 1, 0);
    grid->addWidget(m_size_spins[i], i + 1, 1);
    grid->addWidget(m_width_spins[i], i + 1, 2);
    grid->addWidget(m_height_spins[i], i + 1, 3);
    connect(m_size_spins[i], &QSpinBox::valueChanged, this,
            &TabFonts::on_size_changed);
  }
  grid->setColumnStretch(4, 1);
  box.addLayout(grid);

  m_manual_check = new QCheckBox("Set width and height by hand");
  m_manual_check->setToolTip(
      "Otherwise they follow the size and the proportions of the font set "
      "first.\n"
      "The launcher only knows the proportions of its own fonts.");
  connect(m_manual_check, &QCheckBox::toggled, this,
          &TabFonts::on_manual_toggled);
  box.addWidget(m_manual_check);

  m_blending_check = new QCheckBox("Font blending");
  m_blending_check->setToolTip("Smoother font rendering.");
  box.addWidget(m_blending_check);

  m_ascii_check = new QCheckBox(
      "Draw lines instead of using line characters (SDL ASCII lines)");
  m_ascii_check->setToolTip(
      "For box borders. Use this when your font lacks the line drawing "
      "characters.");
  box.addWidget(m_ascii_check);

  m_save_button = new QPushButton("Save");
  m_save_button->setToolTip(
      "Save these settings to options.json.\nThey are also saved when setting "
      "a font.");
  connect(m_save_button, &QPushButton::clicked, this, &TabFonts::on_save);
  box.addWidget(m_save_button, 0, Qt::AlignLeft);
}

void TabFonts::show_unavailable(const QString& message) {
  m_hint_label->setText(message);
  m_hint_label->setVisible(true);
  m_scroller->setVisible(false);
}

void TabFonts::refresh() {
  const std::string game = m_ctx.settings.get_string("game");
  const Paths::InstallsSummary installs = m_ctx.paths.installs_summary();
  if (!installs.contains(game)) {
    show_unavailable("Install the game first (on the Game tab).");
    return;
  }
  if (!m_ctx.fonts.font_config_file_exists()) {
    m_ctx.status.post(
        "Can't manage fonts at this time: font config file does not exist. "
        "Make sure you've started the game at least once to create it.",
        MSG_WARN);
    show_unavailable(
        "Can't manage fonts at this time: the font config file does not "
        "exist.\n"
        "Start the game at least once to create it.");
    return;
  }
  if (!m_ctx.fonts.options_file_exists()) {
    m_ctx.status.post(
        "Can't manage fonts at this time: game options file does not exist. "
        "Make sure you've started the game at least once to create it.",
        MSG_WARN);
    show_unavailable(
        "Can't manage fonts at this time: the game options file does not "
        "exist.\n"
        "Start the game at least once to create it.");
    return;
  }
  if (!m_ctx.fonts.load_available_fonts() || !m_ctx.fonts.load_font_config() ||
      !m_ctx.fonts.load_game_options()) {
    show_unavailable("Could not read the font settings, see the log below.");
    return;
  }

  m_hint_label->setVisible(false);
  m_scroller->setVisible(true);

  fill_fonts_list(-1);
  show_entries();

  m_updating = true;
  // Width and height are by hand if they are not what the launcher would pick.
  bool manual = false;
  for (int i = 0; i < 3; ++i) {
    const FontDims dims = m_ctx.fonts.game_dims(SIZE_FIELDS[i]);
    const FontDims picked = m_ctx.fonts.auto_dims(SIZE_FIELDS[i], dims.size);
    manual =
        manual || dims.width != picked.width || dims.height != picked.height;
    m_size_spins[i]->setValue(dims.size);
    m_width_spins[i]->setValue(dims.width);
    m_height_spins[i]->setValue(dims.height);
  }
  m_manual_check->setChecked(manual);
  const std::string blending = m_ctx.fonts.get_game_option("FONT_BLENDING");
  m_blending_check->setChecked(blending == "true" || blending == "True");
  const bool has_ascii =
      m_ctx.fonts.has_game_option("USE_DRAW_ASCII_LINES_ROUTINE");
  m_ascii_check->setVisible(has_ascii);
  if (has_ascii) {
    m_ascii_check->setChecked(
        m_ctx.fonts.get_game_option("USE_DRAW_ASCII_LINES_ROUTINE") == "true");
  }
  m_updating = false;
  on_manual_toggled();
}

void TabFonts::fill_fonts_list(int select) {
  m_updating = true;
  m_fonts_list->clear();
  const std::vector<FontInfo>& fonts = m_ctx.fonts.available_fonts();
  for (const auto& font : fonts) {
    auto* item = new QListWidgetItem(
        qstr(font.bundled ? font.name : font.name + "  (added from a file)"),
        m_fonts_list);
    item->setToolTip(qstr(font.file));
  }
  if (select >= 0) {
    m_fonts_list->setCurrentRow(select);
  }
  m_updating = false;
  on_font_selected();
}

int TabFonts::selected_font_index() {
  const int row = selected_row(m_fonts_list);
  if (row < 0 ||
      static_cast<size_t>(row) >= m_ctx.fonts.available_fonts().size()) {
    return -1;
  }
  return row;
}

void TabFonts::update_preview() {
  const int index = selected_font_index();
  if (index < 0) {
    m_preview->show_font(std::filesystem::path(), 0);
  } else {
    m_preview->show_font(
        m_ctx.fonts.font_file_path(m_ctx.fonts.available_fonts()[index]),
        m_size_spins[0]->value());
  }
}

void TabFonts::on_font_selected() {
  if (m_updating) {
    return;
  }
  const int index = selected_font_index();
  const bool have = (index >= 0);
  const std::vector<std::string>& fields = FontManager::font_fields();
  m_set_ui_button->setEnabled(have && m_ctx.fonts.has_font_field(fields[0]));
  m_set_menus_button->setEnabled(have && m_ctx.fonts.has_font_field(fields[1]));
  m_set_map_button->setEnabled(have && m_ctx.fonts.has_font_field(fields[2]));
  m_set_overmap_button->setEnabled(have &&
                                   m_ctx.fonts.has_font_field(fields[3]));
  m_set_all_button->setEnabled(have);
  m_description_label->setText(
      have ? qstr(m_ctx.fonts.available_fonts()[index].description)
           : QString());
  update_preview();
}

void TabFonts::on_add_font_file() {
  const QString file = QFileDialog::getOpenFileName(
      &m_parent, "Choose a font file", QString(),
      "Fonts (.ttf, .otf, .ttc) (*.ttf *.otf *.ttc *.TTF *.OTF *.TTC)");
  if (!file.isEmpty()) {
    add_font_file(str(file));
  }
}

void TabFonts::add_font_file(const std::string& path) {
  const std::function<void()> add = [this, path] {
    const int index = m_ctx.fonts.add_font_file(to_path(path));
    if (index >= 0) {
      fill_fonts_list(index);
    }
  };
  if (!m_ctx.fonts.add_would_replace(to_path(path))) {
    add();
    return;
  }
  const std::string file = to_utf8(to_path(path).filename());
  dialogs::confirm(m_parent, qstr("Replace " + file + "?"),
                   qstr("The user font folder already has a font file called " +
                        file + ". It will be replaced."),
                   "Replace", add);
}

void TabFonts::on_set_font(const std::vector<std::string>& fields) {
  const int index = selected_font_index();
  if (index < 0) {
    return;
  }
  const std::string& name = m_ctx.fonts.available_fonts()[index].name;

  std::vector<std::string> present;
  const std::vector<std::string>& all = FontManager::font_fields();
  for (const auto& field : fields) {
    if (!m_ctx.fonts.has_font_field(field)) {
      continue;
    }
    present.push_back(field);
    const size_t caption = std::ranges::find(all, field) - all.begin();
    m_ctx.status.post("Setting " + name + " as the " + FIELD_CAPTIONS[caption] +
                      " font.");
  }

  if (m_ctx.fonts.set_font(static_cast<size_t>(index), present)) {
    on_save();  // The width and height may depend on the font.
  }
  show_entries();
}

void TabFonts::on_reset() {
  dialogs::confirm(m_parent, "Reset all fonts?",
                   "All font lists in fonts.json, with their hinting and "
                   "antialiasing, are set back to the "
                   "game's defaults. The sizes are kept.",
                   "Reset", [this] {
                     if (m_ctx.fonts.reset_font()) {
                       on_save();
                     }
                     show_entries();
                   });
}

std::string TabFonts::current_field() {
  const int selected = m_field_dropdown->currentIndex();
  const std::vector<std::string>& fields = FontManager::font_fields();
  return (selected >= 0 && static_cast<size_t>(selected) < fields.size())
             ? fields[selected]
             : fields[0];
}

// Each row is a small widget of its own: the name, the hinting and the
// antialiasing.
void TabFonts::show_entries() {
  m_updating = true;
  m_entries_list->clear();

  const std::string field = current_field();
  const bool present = m_ctx.fonts.has_font_field(field);
  m_missing_field_label->setVisible(!present);
  m_entries = m_ctx.fonts.font_entries(field);

  const std::vector<std::string>& modes = FontManager::hinting_modes();
  for (size_t i = 0; i < m_entries.size(); ++i) {
    const FontEntry& entry = m_entries[i];
    auto* row = new QWidget();
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(12);

    const std::string name = to_utf8(to_path(entry.path).stem());
    QLabel* label = make_label(qstr(i == 0 ? name : name + "  (fallback)"));
    label->setToolTip(qstr(entry.path));
    layout->addWidget(label, 1);

    layout->addWidget(make_label("Hinting:"));
    auto* hinting = new QComboBox();
    for (const auto& mode : modes) {
      hinting->addItem(qstr(mode));
    }
    const auto mode = std::ranges::find(modes, entry.hinting);
    if (mode != modes.end()) {
      hinting->setCurrentIndex(static_cast<int>(mode - modes.begin()));
      hinting->setToolTip(
          "How glyphs are fitted to the pixel grid. Bitmap is sharp and "
          "pixelated.");
    } else {
      hinting->setCurrentIndex(-1);
      hinting->setToolTip(
          entry.hinting.empty()
              ? QString("Not set in fonts.json: the game decides.")
              : qstr("\"" + entry.hinting +
                     "\" is not a mode this game knows."));
    }
    layout->addWidget(hinting);

    auto* antialiasing = new QCheckBox("Antialiasing");
    antialiasing->setChecked(entry.antialiasing);
    layout->addWidget(antialiasing);

    connect(hinting, &QComboBox::currentIndexChanged, this,
            [this, i, hinting, antialiasing] {
              on_entry_style_changed(i, hinting, antialiasing);
            });
    connect(antialiasing, &QCheckBox::toggled, this,
            [this, i, hinting, antialiasing] {
              on_entry_style_changed(i, hinting, antialiasing);
            });

    auto* item = new QListWidgetItem(m_entries_list);
    item->setSizeHint(row->sizeHint());
    m_entries_list->setItemWidget(item, row);
  }
  m_updating = false;
  update_entry_buttons();
}

int TabFonts::selected_entry() {
  const int row = selected_row(m_entries_list);
  if (row < 0 || static_cast<size_t>(row) >= m_entries.size()) {
    return -1;
  }
  return row;
}

void TabFonts::update_entry_buttons() {
  const int index = selected_entry();
  m_move_up_button->setEnabled(index > 0);
  m_move_down_button->setEnabled(index >= 0 && static_cast<size_t>(index) + 1 <
                                                   m_entries.size());
  m_remove_button->setEnabled(index >= 0);
}

void TabFonts::on_entry_style_changed(size_t index, QComboBox* hinting,
                                      QCheckBox* antialiasing) {
  if (m_updating || index >= m_entries.size()) {
    return;
  }
  const std::vector<std::string>& modes = FontManager::hinting_modes();
  const int mode = hinting->currentIndex();
  const bool known = (mode >= 0 && static_cast<size_t>(mode) < modes.size());
  // An entry without hinting gets the game's default with its first setting.
  const std::string hinting_mode = known ? modes[mode] : std::string("Default");

  if (m_ctx.fonts.set_entry_style(current_field(), index, m_entries[index].path,
                                  hinting_mode, antialiasing->isChecked())) {
    m_entries[index].hinting = hinting_mode;
    m_entries[index].antialiasing = antialiasing->isChecked();
    if (!known) {
      m_updating = true;
      hinting->setCurrentIndex(0);
      m_updating = false;
    }
    return;
  }
  // Show what the file has now. Not from inside the widget's own signal:
  // show_entries deletes the widget.
  QTimer::singleShot(0, this, [this] { show_entries(); });
}

void TabFonts::on_move_entry(int offset) {
  const int index = selected_entry();
  if (index < 0) {
    return;
  }
  const bool moved =
      m_ctx.fonts.move_entry(current_field(), static_cast<size_t>(index),
                             m_entries[index].path, offset);
  show_entries();
  if (moved) {
    m_entries_list->setCurrentRow(index + offset);
    on_save();  // The width and height may depend on the first font.
  }
}

void TabFonts::on_remove_entry() {
  const int index = selected_entry();
  if (index < 0) {
    return;
  }
  const std::string field = current_field();
  const std::string path = m_entries[index].path;
  const std::string name = to_utf8(to_path(path).stem());
  dialogs::confirm(
      m_parent, qstr("Remove " + name + " from the list?"),
      "The font is taken out of this font list in fonts.json, with its hinting "
      "and antialiasing. "
      "The font file itself stays.",
      "Remove", [this, field, index, path] {
        if (m_ctx.fonts.remove_entry(field, static_cast<size_t>(index), path) &&
            index == 0) {
          on_save();  // The width and height may depend on the first font.
        }
        show_entries();
      });
}

void TabFonts::on_size_changed() {
  if (m_updating) {
    return;
  }
  if (!m_manual_check->isChecked()) {
    for (int i = 0; i < 3; ++i) {
      const FontDims dims =
          m_ctx.fonts.auto_dims(SIZE_FIELDS[i], m_size_spins[i]->value());
      m_width_spins[i]->setValue(dims.width);
      m_height_spins[i]->setValue(dims.height);
    }
  }
  update_preview();
}

void TabFonts::on_manual_toggled() {
  if (m_updating) {
    return;
  }
  const bool manual = m_manual_check->isChecked();
  for (int i = 0; i < 3; ++i) {
    m_width_spins[i]->setEnabled(manual);
    m_height_spins[i]->setEnabled(manual);
  }
  on_size_changed();
}

void TabFonts::on_save() {
  // The font set first may have changed, and with it the proportions.
  on_size_changed();

  FontOptions options;
  static std::array<FontDims*, 3> dims = {&options.ui, &options.map,
                                          &options.overmap};
  for (int i = 0; i < dims.size(); ++i) {
    dims.at(i)->size = m_size_spins[i]->value();
    dims.at(i)->width = m_width_spins[i]->value();
    dims.at(i)->height = m_height_spins[i]->value();
  }
  options.blending = m_blending_check->isChecked();
  options.ascii_lines = m_ascii_check->isChecked();
  m_ctx.fonts.save_font_options(options);
}
