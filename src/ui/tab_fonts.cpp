#include "ui/tab_fonts.h"

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
#include <filesystem>

#include "core/fs.h"
#include "ui/dialogs.h"
#include "ui/font_preview.h"
#include "ui/qt_helpers.h"

// Same order as FontManager::font_fields().
static const char* const FIELD_CAPTIONS[] = {"Game UI", "Menus", "Map", "Overmap"};
static const char* const FIELD_TOOLTIPS[] = {
    "The classic game interface: the sidebar, messages and most screens.",
    "The newer menus (drawn with ImGui). They use the game UI font size.",
    "The game world, when not using tiles.",
    "The world map view, a.k.a. the overmap."};
// The font lists with their own sizes in options.json; the menus use the UI's.
static const char* const SIZE_FIELDS[] = {"typeface", "map_typeface", "overmap_typeface"};

static QLabel* make_section_heading(const QString& text)
{
    QLabel* label = make_heading(text);
    label->setContentsMargins(0, 6, 0, 0);
    return label;
}

static QFrame* make_separator()
{
    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    return separator;
}

static QSpinBox* make_spin(int value, int min, int max, const QString& tooltip)
{
    QSpinBox* spin = new QSpinBox();
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setToolTip(tooltip);
    return spin;
}

TabFonts::TabFonts(AppContext& ctx, QWidget& parent) : ctx_(ctx), parent_(parent), updating_(false)
{
    build_ui();
}

void TabFonts::build_ui()
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);

    hint_label_ = make_label("");
    hint_label_->setWordWrap(true);
    hint_label_->setAlignment(Qt::AlignLeft | Qt::AlignTop);  // Not in the middle of the empty tab.
    outer->addWidget(hint_label_);

    // The tab is taller than the window can be.
    scroller_ = new QScrollArea();
    scroller_->setWidgetResizable(true);
    scroller_->setFrameShape(QFrame::NoFrame);
    scroller_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget* page = new QWidget();
    QVBoxLayout* content = new QVBoxLayout(page);
    content->setContentsMargins(0, 0, 12, 0);  // Room for the scrollbar.
    content->setSpacing(8);
    scroller_->setWidget(page);
    outer->addWidget(scroller_, 1);

    // Available fonts.
    content->addWidget(make_section_heading("Available fonts"));
    fonts_list_ = new QListWidget();
    fonts_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    fonts_list_->setMinimumHeight(150);
    content->addWidget(fonts_list_);

    add_file_button_ = new QPushButton("Add Font File...");
    add_file_button_->setToolTip("Add a .ttf, .otf or .ttc font from your computer.\n"
                                 "It is copied to the game's user font folder.");
    content->addWidget(add_file_button_, 0, Qt::AlignLeft);

    description_label_ = make_label("");
    description_label_->setWordWrap(true);
    content->addWidget(description_label_);
    preview_ = new FontPreview();
    content->addWidget(preview_);

    set_ui_button_ = new QPushButton("Set for Game UI");
    set_ui_button_->setToolTip(FIELD_TOOLTIPS[0]);
    set_menus_button_ = new QPushButton("Set for Menus");
    set_menus_button_->setToolTip(FIELD_TOOLTIPS[1]);
    set_map_button_ = new QPushButton("Set for Map");
    set_map_button_->setToolTip(FIELD_TOOLTIPS[2]);
    set_overmap_button_ = new QPushButton("Set for Overmap");
    set_overmap_button_->setToolTip(FIELD_TOOLTIPS[3]);
    set_all_button_ = new QPushButton("Set for All");
    set_all_button_->setToolTip("Use the selected font for everything.");
    QHBoxLayout* set_row = new QHBoxLayout();
    set_row->setSpacing(8);
    set_row->addWidget(set_ui_button_, 1);
    set_row->addWidget(set_menus_button_, 1);
    set_row->addWidget(set_map_button_, 1);
    set_row->addWidget(set_overmap_button_, 1);
    set_row->addWidget(set_all_button_, 1);
    content->addLayout(set_row);

    // The font lists of fonts.json.
    content->addWidget(make_separator());
    content->addWidget(make_section_heading("Font lists"));
    QLabel* lists_hint = make_label("The game uses the first font of a list. Characters it lacks come from the next ones, "
                                    "and Unifont is always the last resort.");
    lists_hint->setWordWrap(true);
    content->addWidget(lists_hint);

    field_dropdown_ = new QComboBox();
    for (int i = 0; i < 4; ++i)
        field_dropdown_->addItem(FIELD_CAPTIONS[i]);
    content->addWidget(field_dropdown_, 0, Qt::AlignLeft);
    missing_field_label_ = make_label("This version of the game has no font list for this.");
    content->addWidget(missing_field_label_);

    entries_list_ = new QListWidget();
    entries_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    entries_list_->setMinimumHeight(120);
    content->addWidget(entries_list_);

    move_up_button_ = new QPushButton("Move Up");
    move_down_button_ = new QPushButton("Move Down");
    remove_button_ = new QPushButton("Remove...");
    remove_button_->setToolTip("Take the selected font out of this list. The font file stays.");
    reset_button_ = new QPushButton("Reset All Fonts...");
    reset_button_->setToolTip("Set all font lists back to the game's defaults.");
    QHBoxLayout* entry_row = new QHBoxLayout();
    entry_row->setSpacing(8);
    entry_row->addWidget(move_up_button_);
    entry_row->addWidget(move_down_button_);
    entry_row->addWidget(remove_button_);
    entry_row->addStretch(1);
    entry_row->addWidget(reset_button_);
    content->addLayout(entry_row);

    // Sizes and other options.json settings.
    content->addWidget(make_separator());
    content->addWidget(make_section_heading("Sizes and rendering"));
    build_sizes(*content);
    content->addStretch(1);

    connect(fonts_list_, &QListWidget::itemSelectionChanged, this, &TabFonts::on_font_selected);
    connect(add_file_button_, &QPushButton::clicked, this, &TabFonts::on_add_font_file);
    const std::vector<std::string>& fields = FontManager::font_fields();
    connect(set_ui_button_, &QPushButton::clicked, this, [this, fields]() { on_set_font({fields[0]}); });
    connect(set_menus_button_, &QPushButton::clicked, this, [this, fields]() { on_set_font({fields[1]}); });
    connect(set_map_button_, &QPushButton::clicked, this, [this, fields]() { on_set_font({fields[2]}); });
    connect(set_overmap_button_, &QPushButton::clicked, this, [this, fields]() { on_set_font({fields[3]}); });
    connect(set_all_button_, &QPushButton::clicked, this, [this, fields]() { on_set_font(fields); });
    connect(reset_button_, &QPushButton::clicked, this, &TabFonts::on_reset);
    connect(field_dropdown_, &QComboBox::currentIndexChanged, this, [this]() {
        if (!updating_)
            show_entries();
    });
    connect(entries_list_, &QListWidget::itemSelectionChanged, this, &TabFonts::update_entry_buttons);
    connect(move_up_button_, &QPushButton::clicked, this, [this]() { on_move_entry(-1); });
    connect(move_down_button_, &QPushButton::clicked, this, [this]() { on_move_entry(1); });
    connect(remove_button_, &QPushButton::clicked, this, &TabFonts::on_remove_entry);
}

void TabFonts::build_sizes(QVBoxLayout& box)
{
    QGridLayout* grid = new QGridLayout();
    grid->setVerticalSpacing(6);
    grid->setHorizontalSpacing(12);
    grid->addWidget(make_label("Size"), 0, 1);
    grid->addWidget(make_label("Width (px)"), 0, 2);
    grid->addWidget(make_label("Height (px)"), 0, 3);

    const char* const captions[] = {"Game UI and menus:", "Map:", "Overmap:"};
    const char* const tooltips[] = {"Font size for the game UI and the menus.", "Font size for the game world (when not using tiles).",
                                    "Font size for the world map view, a.k.a. the overmap."};
    for (int i = 0; i < 3; ++i)
    {
        // The ranges the game accepts.
        size_spins_[i] = make_spin(16, 8, 100, tooltips[i]);
        width_spins_[i] = make_spin(8, 6, 100, "Width of a character cell.");
        height_spins_[i] = make_spin(16, 8, 100, "Height of a character cell.");
        grid->addWidget(make_label(captions[i]), i + 1, 0);
        grid->addWidget(size_spins_[i], i + 1, 1);
        grid->addWidget(width_spins_[i], i + 1, 2);
        grid->addWidget(height_spins_[i], i + 1, 3);
        connect(size_spins_[i], &QSpinBox::valueChanged, this, &TabFonts::on_size_changed);
    }
    grid->setColumnStretch(4, 1);
    box.addLayout(grid);

    manual_check_ = new QCheckBox("Set width and height by hand");
    manual_check_->setToolTip("Otherwise they follow the size and the proportions of the font set first.\n"
                              "The launcher only knows the proportions of its own fonts.");
    connect(manual_check_, &QCheckBox::toggled, this, &TabFonts::on_manual_toggled);
    box.addWidget(manual_check_);

    blending_check_ = new QCheckBox("Font blending");
    blending_check_->setToolTip("Smoother font rendering.");
    box.addWidget(blending_check_);

    ascii_check_ = new QCheckBox("Draw lines instead of using line characters (SDL ASCII lines)");
    ascii_check_->setToolTip("For box borders. Use this when your font lacks the line drawing characters.");
    box.addWidget(ascii_check_);

    save_button_ = new QPushButton("Save");
    save_button_->setToolTip("Save these settings to options.json.\nThey are also saved when setting a font.");
    connect(save_button_, &QPushButton::clicked, this, &TabFonts::on_save);
    box.addWidget(save_button_, 0, Qt::AlignLeft);
}

void TabFonts::show_unavailable(const QString& message)
{
    hint_label_->setText(message);
    hint_label_->setVisible(true);
    scroller_->setVisible(false);
}

void TabFonts::refresh()
{
    std::string game = ctx_.settings.get_string("game");
    Paths::InstallsSummary installs = ctx_.paths.installs_summary();
    if (installs.find(game) == installs.end())
    {
        show_unavailable("Install the game first (on the Game tab).");
        return;
    }
    if (!ctx_.fonts.font_config_file_exists())
    {
        ctx_.status.post("Can't manage fonts at this time: font config file does not exist. "
                         "Make sure you've started the game at least once to create it.",
                         MSG_WARN);
        show_unavailable("Can't manage fonts at this time: the font config file does not exist.\n"
                         "Start the game at least once to create it.");
        return;
    }
    if (!ctx_.fonts.options_file_exists())
    {
        ctx_.status.post("Can't manage fonts at this time: game options file does not exist. "
                         "Make sure you've started the game at least once to create it.",
                         MSG_WARN);
        show_unavailable("Can't manage fonts at this time: the game options file does not exist.\n"
                         "Start the game at least once to create it.");
        return;
    }
    if (!ctx_.fonts.load_available_fonts() || !ctx_.fonts.load_font_config() || !ctx_.fonts.load_game_options())
    {
        show_unavailable("Could not read the font settings, see the log below.");
        return;
    }

    hint_label_->setVisible(false);
    scroller_->setVisible(true);

    fill_fonts_list(-1);
    show_entries();

    updating_ = true;
    // Width and height are by hand if they are not what the launcher would pick.
    bool manual = false;
    for (int i = 0; i < 3; ++i)
    {
        FontDims dims = ctx_.fonts.game_dims(SIZE_FIELDS[i]);
        FontDims picked = ctx_.fonts.auto_dims(SIZE_FIELDS[i], dims.size);
        manual = manual || dims.width != picked.width || dims.height != picked.height;
        size_spins_[i]->setValue(dims.size);
        width_spins_[i]->setValue(dims.width);
        height_spins_[i]->setValue(dims.height);
    }
    manual_check_->setChecked(manual);
    std::string blending = ctx_.fonts.get_game_option("FONT_BLENDING");
    blending_check_->setChecked(blending == "true" || blending == "True");
    bool has_ascii = ctx_.fonts.has_game_option("USE_DRAW_ASCII_LINES_ROUTINE");
    ascii_check_->setVisible(has_ascii);
    if (has_ascii)
        ascii_check_->setChecked(ctx_.fonts.get_game_option("USE_DRAW_ASCII_LINES_ROUTINE") == "true");
    updating_ = false;
    on_manual_toggled();
}

void TabFonts::fill_fonts_list(int select)
{
    updating_ = true;
    fonts_list_->clear();
    const std::vector<FontInfo>& fonts = ctx_.fonts.available_fonts();
    for (size_t i = 0; i < fonts.size(); ++i)
    {
        QListWidgetItem* item =
            new QListWidgetItem(qstr(fonts[i].bundled ? fonts[i].name : fonts[i].name + "  (added from a file)"), fonts_list_);
        item->setToolTip(qstr(fonts[i].file));
    }
    if (select >= 0)
        fonts_list_->setCurrentRow(select);
    updating_ = false;
    on_font_selected();
}

int TabFonts::selected_font_index()
{
    int row = selected_row(fonts_list_);
    if (row < 0 || (size_t)row >= ctx_.fonts.available_fonts().size())
        return -1;
    return row;
}

void TabFonts::update_preview()
{
    int index = selected_font_index();
    if (index < 0)
        preview_->show_font(std::filesystem::path(), 0);
    else
        preview_->show_font(ctx_.fonts.font_file_path(ctx_.fonts.available_fonts()[index]), size_spins_[0]->value());
}

void TabFonts::on_font_selected()
{
    if (updating_)
        return;
    int index = selected_font_index();
    bool have = (index >= 0);
    const std::vector<std::string>& fields = FontManager::font_fields();
    set_ui_button_->setEnabled(have && ctx_.fonts.has_font_field(fields[0]));
    set_menus_button_->setEnabled(have && ctx_.fonts.has_font_field(fields[1]));
    set_map_button_->setEnabled(have && ctx_.fonts.has_font_field(fields[2]));
    set_overmap_button_->setEnabled(have && ctx_.fonts.has_font_field(fields[3]));
    set_all_button_->setEnabled(have);
    description_label_->setText(have ? qstr(ctx_.fonts.available_fonts()[index].description) : QString());
    update_preview();
}

void TabFonts::on_add_font_file()
{
    QString file = QFileDialog::getOpenFileName(&parent_, "Choose a font file", QString(),
                                                "Fonts (.ttf, .otf, .ttc) (*.ttf *.otf *.ttc *.TTF *.OTF *.TTC)");
    if (!file.isEmpty())
        add_font_file(str(file));
}

void TabFonts::add_font_file(const std::string& path)
{
    std::function<void()> add = [this, path]() {
        int index = ctx_.fonts.add_font_file(to_path(path));
        if (index >= 0)
            fill_fonts_list(index);
    };
    if (!ctx_.fonts.add_would_replace(to_path(path)))
    {
        add();
        return;
    }
    std::string file = to_utf8(to_path(path).filename());
    dialogs::confirm(parent_, qstr("Replace " + file + "?"),
                     qstr("The user font folder already has a font file called " + file + ". It will be replaced."),
                     "Replace", add);
}

void TabFonts::on_set_font(const std::vector<std::string>& fields)
{
    int index = selected_font_index();
    if (index < 0)
        return;
    const std::string& name = ctx_.fonts.available_fonts()[index].name;

    std::vector<std::string> present;
    const std::vector<std::string>& all = FontManager::font_fields();
    for (size_t i = 0; i < fields.size(); ++i)
    {
        if (!ctx_.fonts.has_font_field(fields[i]))
            continue;
        present.push_back(fields[i]);
        size_t caption = std::find(all.begin(), all.end(), fields[i]) - all.begin();
        ctx_.status.post("Setting " + name + " as the " + FIELD_CAPTIONS[caption] + " font.");
    }

    if (ctx_.fonts.set_font((size_t)index, present))
        on_save();  // The width and height may depend on the font.
    show_entries();
}

void TabFonts::on_reset()
{
    dialogs::confirm(parent_, "Reset all fonts?",
                     "All font lists in fonts.json, with their hinting and antialiasing, are set back to the "
                     "game's defaults. The sizes are kept.",
                     "Reset", [this]() {
                         if (ctx_.fonts.reset_font())
                             on_save();
                         show_entries();
                     });
}

std::string TabFonts::current_field()
{
    int selected = field_dropdown_->currentIndex();
    const std::vector<std::string>& fields = FontManager::font_fields();
    return (selected >= 0 && (size_t)selected < fields.size()) ? fields[selected] : fields[0];
}

// Each row is a small widget of its own: the name, the hinting and the antialiasing.
void TabFonts::show_entries()
{
    updating_ = true;
    entries_list_->clear();

    std::string field = current_field();
    bool present = ctx_.fonts.has_font_field(field);
    missing_field_label_->setVisible(!present);
    entries_ = ctx_.fonts.font_entries(field);

    const std::vector<std::string>& modes = FontManager::hinting_modes();
    for (size_t i = 0; i < entries_.size(); ++i)
    {
        const FontEntry& entry = entries_[i];
        QWidget* row = new QWidget();
        QHBoxLayout* layout = new QHBoxLayout(row);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(12);

        std::string name = to_utf8(to_path(entry.path).stem());
        QLabel* label = make_label(qstr(i == 0 ? name : name + "  (fallback)"));
        label->setToolTip(qstr(entry.path));
        layout->addWidget(label, 1);

        layout->addWidget(make_label("Hinting:"));
        QComboBox* hinting = new QComboBox();
        for (size_t m = 0; m < modes.size(); ++m)
            hinting->addItem(qstr(modes[m]));
        std::vector<std::string>::const_iterator mode = std::find(modes.begin(), modes.end(), entry.hinting);
        if (mode != modes.end())
        {
            hinting->setCurrentIndex((int)(mode - modes.begin()));
            hinting->setToolTip("How glyphs are fitted to the pixel grid. Bitmap is sharp and pixelated.");
        }
        else
        {
            hinting->setCurrentIndex(-1);
            hinting->setToolTip(entry.hinting.empty() ? QString("Not set in fonts.json: the game decides.")
                                                      : qstr("\"" + entry.hinting + "\" is not a mode this game knows."));
        }
        layout->addWidget(hinting);

        QCheckBox* antialiasing = new QCheckBox("Antialiasing");
        antialiasing->setChecked(entry.antialiasing);
        layout->addWidget(antialiasing);

        connect(hinting, &QComboBox::currentIndexChanged, this,
                [this, i, hinting, antialiasing]() { on_entry_style_changed(i, hinting, antialiasing); });
        connect(antialiasing, &QCheckBox::toggled, this,
                [this, i, hinting, antialiasing]() { on_entry_style_changed(i, hinting, antialiasing); });

        QListWidgetItem* item = new QListWidgetItem(entries_list_);
        item->setSizeHint(row->sizeHint());
        entries_list_->setItemWidget(item, row);
    }
    updating_ = false;
    update_entry_buttons();
}

int TabFonts::selected_entry()
{
    int row = selected_row(entries_list_);
    if (row < 0 || (size_t)row >= entries_.size())
        return -1;
    return row;
}

void TabFonts::update_entry_buttons()
{
    int index = selected_entry();
    move_up_button_->setEnabled(index > 0);
    move_down_button_->setEnabled(index >= 0 && (size_t)index + 1 < entries_.size());
    remove_button_->setEnabled(index >= 0);
}

void TabFonts::on_entry_style_changed(size_t index, QComboBox* hinting, QCheckBox* antialiasing)
{
    if (updating_ || index >= entries_.size())
        return;
    const std::vector<std::string>& modes = FontManager::hinting_modes();
    int mode = hinting->currentIndex();
    bool known = (mode >= 0 && (size_t)mode < modes.size());
    // An entry without hinting gets the game's default with its first setting.
    std::string hinting_mode = known ? modes[mode] : std::string("Default");

    if (ctx_.fonts.set_entry_style(current_field(), index, entries_[index].path, hinting_mode, antialiasing->isChecked()))
    {
        entries_[index].hinting = hinting_mode;
        entries_[index].antialiasing = antialiasing->isChecked();
        if (!known)
        {
            updating_ = true;
            hinting->setCurrentIndex(0);
            updating_ = false;
        }
        return;
    }
    // Show what the file has now. Not from inside the widget's own signal:
    // show_entries deletes the widget.
    QTimer::singleShot(0, this, [this]() { show_entries(); });
}

void TabFonts::on_move_entry(int offset)
{
    int index = selected_entry();
    if (index < 0)
        return;
    bool moved = ctx_.fonts.move_entry(current_field(), (size_t)index, entries_[index].path, offset);
    show_entries();
    if (moved)
    {
        entries_list_->setCurrentRow(index + offset);
        on_save();  // The width and height may depend on the first font.
    }
}

void TabFonts::on_remove_entry()
{
    int index = selected_entry();
    if (index < 0)
        return;
    std::string field = current_field();
    std::string path = entries_[index].path;
    std::string name = to_utf8(to_path(path).stem());
    dialogs::confirm(parent_, qstr("Remove " + name + " from the list?"),
                     "The font is taken out of this font list in fonts.json, with its hinting and antialiasing. "
                     "The font file itself stays.",
                     "Remove", [this, field, index, path]() {
                         if (ctx_.fonts.remove_entry(field, (size_t)index, path) && index == 0)
                             on_save();  // The width and height may depend on the first font.
                         show_entries();
                     });
}

void TabFonts::on_size_changed()
{
    if (updating_)
        return;
    if (!manual_check_->isChecked())
    {
        for (int i = 0; i < 3; ++i)
        {
            FontDims dims = ctx_.fonts.auto_dims(SIZE_FIELDS[i], size_spins_[i]->value());
            width_spins_[i]->setValue(dims.width);
            height_spins_[i]->setValue(dims.height);
        }
    }
    update_preview();
}

void TabFonts::on_manual_toggled()
{
    if (updating_)
        return;
    bool manual = manual_check_->isChecked();
    for (int i = 0; i < 3; ++i)
    {
        width_spins_[i]->setEnabled(manual);
        height_spins_[i]->setEnabled(manual);
    }
    on_size_changed();
}

void TabFonts::on_save()
{
    // The font set first may have changed, and with it the proportions.
    on_size_changed();

    FontOptions options;
    FontDims* dims[] = {&options.ui, &options.map, &options.overmap};
    for (int i = 0; i < 3; ++i)
    {
        dims[i]->size = size_spins_[i]->value();
        dims[i]->width = width_spins_[i]->value();
        dims[i]->height = height_spins_[i]->value();
    }
    options.blending = blending_check_->isChecked();
    options.ascii_lines = ascii_check_->isChecked();
    ctx_.fonts.save_font_options(options);
}
