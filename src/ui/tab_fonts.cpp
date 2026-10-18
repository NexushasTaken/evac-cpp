#include "ui/tab_fonts.h"

#include <filesystem>

#include "ui/dialogs.h"

// Same order as FontManager::font_fields().
static const char* const FIELD_CAPTIONS[] = {"Game UI", "Menus", "Map", "Overmap"};
static const char* const FIELD_TOOLTIPS[] = {
    "The classic game interface: the sidebar, messages and most screens.",
    "The newer menus (drawn with ImGui). They use the game UI font size.",
    "The game world, when not using tiles.",
    "The world map view, a.k.a. the overmap."};
// The font lists with their own sizes in options.json; the menus use the UI's.
static const char* const SIZE_FIELDS[] = {"typeface", "map_typeface", "overmap_typeface"};

static Gtk::Label* make_label(const Glib::ustring& text)
{
    Gtk::Label* label = Gtk::make_managed<Gtk::Label>(text);
    label->set_xalign(0.0f);
    return label;
}

static Gtk::Label* make_heading(const Glib::ustring& text)
{
    Gtk::Label* label = make_label("<b>" + Glib::Markup::escape_text(text) + "</b>");
    label->set_use_markup(true);
    label->set_margin_top(6);
    return label;
}

TabFonts::TabFonts(AppContext& ctx, Gtk::Window& parent)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8), ctx_(ctx), parent_(parent), updating_(false),
      content_(Gtk::Orientation::VERTICAL, 8)
{
    build_ui();
}

void TabFonts::build_ui()
{
    set_margin(12);

    hint_label_.set_wrap(true);
    hint_label_.set_xalign(0.0f);
    append(hint_label_);

    // The tab is taller than the window can be.
    scroller_.set_child(content_);
    scroller_.set_vexpand(true);
    scroller_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    content_.set_margin_end(12);  // Room for the scrollbar.
    append(scroller_);

    // Available fonts.
    content_.append(*make_heading("Available fonts"));
    Gtk::ScrolledWindow* fonts_scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    fonts_scroller->set_min_content_height(150);
    fonts_scroller->set_has_frame(true);
    fonts_scroller->set_child(fonts_list_);
    fonts_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
    content_.append(*fonts_scroller);

    add_file_button_.set_label("Add Font File...");
    add_file_button_.set_tooltip_text("Add a .ttf, .otf or .ttc font from your computer.\n"
                                      "It is copied to the game's user font folder.");
    add_file_button_.set_halign(Gtk::Align::START);
    content_.append(add_file_button_);

    description_label_.set_xalign(0.0f);
    description_label_.set_wrap(true);
    content_.append(description_label_);
    content_.append(preview_);

    set_ui_button_.set_label("Set for Game UI");
    set_ui_button_.set_tooltip_text(FIELD_TOOLTIPS[0]);
    set_menus_button_.set_label("Set for Menus");
    set_menus_button_.set_tooltip_text(FIELD_TOOLTIPS[1]);
    set_map_button_.set_label("Set for Map");
    set_map_button_.set_tooltip_text(FIELD_TOOLTIPS[2]);
    set_overmap_button_.set_label("Set for Overmap");
    set_overmap_button_.set_tooltip_text(FIELD_TOOLTIPS[3]);
    set_all_button_.set_label("Set for All");
    set_all_button_.set_tooltip_text("Use the selected font for everything.");
    Gtk::Box* set_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    set_row->set_homogeneous(true);
    set_row->append(set_ui_button_);
    set_row->append(set_menus_button_);
    set_row->append(set_map_button_);
    set_row->append(set_overmap_button_);
    set_row->append(set_all_button_);
    content_.append(*set_row);

    // The font lists of fonts.json.
    content_.append(*Gtk::make_managed<Gtk::Separator>());
    content_.append(*make_heading("Font lists"));
    Gtk::Label* lists_hint =
        make_label("The game uses the first font of a list. Characters it lacks come from the next ones, "
                   "and Unifont is always the last resort.");
    lists_hint->set_wrap(true);
    content_.append(*lists_hint);

    std::vector<Glib::ustring> captions(FIELD_CAPTIONS, FIELD_CAPTIONS + 4);
    field_dropdown_.set_model(Gtk::StringList::create(captions));
    field_dropdown_.set_halign(Gtk::Align::START);
    content_.append(field_dropdown_);
    missing_field_label_.set_xalign(0.0f);
    content_.append(missing_field_label_);

    entries_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
    Gtk::Frame* entries_frame = Gtk::make_managed<Gtk::Frame>();
    entries_frame->set_child(entries_list_);
    content_.append(*entries_frame);

    move_up_button_.set_label("Move Up");
    move_down_button_.set_label("Move Down");
    remove_button_.set_label("Remove...");
    remove_button_.set_tooltip_text("Take the selected font out of this list. The font file stays.");
    reset_button_.set_label("Reset All Fonts...");
    reset_button_.set_tooltip_text("Set all font lists back to the game's defaults.");
    Gtk::Box* entry_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    entry_row->append(move_up_button_);
    entry_row->append(move_down_button_);
    entry_row->append(remove_button_);
    Gtk::Box* spacer = Gtk::make_managed<Gtk::Box>();
    spacer->set_hexpand(true);
    entry_row->append(*spacer);
    entry_row->append(reset_button_);
    content_.append(*entry_row);

    // Sizes and other options.json settings.
    content_.append(*Gtk::make_managed<Gtk::Separator>());
    content_.append(*make_heading("Sizes and rendering"));
    build_sizes(content_);

    fonts_list_.signal_row_selected().connect([this](Gtk::ListBoxRow*) { on_font_selected(); });
    add_file_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabFonts::on_add_font_file));
    const std::vector<std::string>& fields = FontManager::font_fields();
    set_ui_button_.signal_clicked().connect([this, fields]() { on_set_font({fields[0]}); });
    set_menus_button_.signal_clicked().connect([this, fields]() { on_set_font({fields[1]}); });
    set_map_button_.signal_clicked().connect([this, fields]() { on_set_font({fields[2]}); });
    set_overmap_button_.signal_clicked().connect([this, fields]() { on_set_font({fields[3]}); });
    set_all_button_.signal_clicked().connect([this, fields]() { on_set_font(fields); });
    reset_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabFonts::on_reset));
    field_dropdown_.property_selected().signal_changed().connect([this]() {
        if (!updating_)
            show_entries();
    });
    entries_list_.signal_row_selected().connect([this](Gtk::ListBoxRow*) { update_entry_buttons(); });
    move_up_button_.signal_clicked().connect([this]() { on_move_entry(-1); });
    move_down_button_.signal_clicked().connect([this]() { on_move_entry(1); });
    remove_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabFonts::on_remove_entry));
}

void TabFonts::build_sizes(Gtk::Box& box)
{
    Gtk::Grid* grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(6);
    grid->set_column_spacing(12);
    grid->attach(*make_label("Size"), 1, 0);
    grid->attach(*make_label("Width (px)"), 2, 0);
    grid->attach(*make_label("Height (px)"), 3, 0);

    const char* const captions[] = {"Game UI and menus:", "Map:", "Overmap:"};
    const char* const tooltips[] = {"Font size for the game UI and the menus.", "Font size for the game world (when not using tiles).",
                                    "Font size for the world map view, a.k.a. the overmap."};
    for (int i = 0; i < 3; ++i)
    {
        // The ranges the game accepts.
        size_spins_[i].set_adjustment(Gtk::Adjustment::create(16, 8, 100, 1));
        size_spins_[i].set_tooltip_text(tooltips[i]);
        width_spins_[i].set_adjustment(Gtk::Adjustment::create(8, 6, 100, 1));
        width_spins_[i].set_tooltip_text("Width of a character cell.");
        height_spins_[i].set_adjustment(Gtk::Adjustment::create(16, 8, 100, 1));
        height_spins_[i].set_tooltip_text("Height of a character cell.");
        grid->attach(*make_label(captions[i]), 0, i + 1);
        grid->attach(size_spins_[i], 1, i + 1);
        grid->attach(width_spins_[i], 2, i + 1);
        grid->attach(height_spins_[i], 3, i + 1);
        size_spins_[i].signal_value_changed().connect(sigc::mem_fun(*this, &TabFonts::on_size_changed));
    }
    box.append(*grid);

    manual_check_.set_label("Set width and height by hand");
    manual_check_.set_tooltip_text("Otherwise they follow the size and the proportions of the font set first.\n"
                                   "The launcher only knows the proportions of its own fonts.");
    manual_check_.signal_toggled().connect(sigc::mem_fun(*this, &TabFonts::on_manual_toggled));
    box.append(manual_check_);

    blending_check_.set_label("Font blending");
    blending_check_.set_tooltip_text("Smoother font rendering.");
    box.append(blending_check_);

    ascii_check_.set_label("Draw lines instead of using line characters (SDL ASCII lines)");
    ascii_check_.set_tooltip_text("For box borders. Use this when your font lacks the line drawing characters.");
    box.append(ascii_check_);

    save_button_.set_label("Save");
    save_button_.set_tooltip_text("Save these settings to options.json.\nThey are also saved when setting a font.");
    save_button_.set_halign(Gtk::Align::START);
    save_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabFonts::on_save));
    box.append(save_button_);
}

void TabFonts::show_unavailable(const Glib::ustring& message)
{
    hint_label_.set_text(message);
    hint_label_.set_visible(true);
    scroller_.set_visible(false);
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

    hint_label_.set_visible(false);
    scroller_.set_visible(true);

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
        size_spins_[i].set_value(dims.size);
        width_spins_[i].set_value(dims.width);
        height_spins_[i].set_value(dims.height);
    }
    manual_check_.set_active(manual);
    std::string blending = ctx_.fonts.get_game_option("FONT_BLENDING");
    blending_check_.set_active(blending == "true" || blending == "True");
    bool has_ascii = ctx_.fonts.has_game_option("USE_DRAW_ASCII_LINES_ROUTINE");
    ascii_check_.set_visible(has_ascii);
    if (has_ascii)
        ascii_check_.set_active(ctx_.fonts.get_game_option("USE_DRAW_ASCII_LINES_ROUTINE") == "true");
    updating_ = false;
    on_manual_toggled();
}

void TabFonts::fill_fonts_list(int select)
{
    updating_ = true;
    while (Gtk::ListBoxRow* row = fonts_list_.get_row_at_index(0))
        fonts_list_.remove(*row);
    const std::vector<FontInfo>& fonts = ctx_.fonts.available_fonts();
    for (size_t i = 0; i < fonts.size(); ++i)
    {
        Gtk::Label* label = make_label(fonts[i].bundled ? fonts[i].name : fonts[i].name + "  (added from a file)");
        label->set_tooltip_text(fonts[i].file);
        fonts_list_.append(*label);
    }
    if (select >= 0)
        fonts_list_.select_row(*fonts_list_.get_row_at_index(select));
    updating_ = false;
    on_font_selected();
}

int TabFonts::selected_font_index()
{
    Gtk::ListBoxRow* row = fonts_list_.get_selected_row();
    if (!row || row->get_index() < 0 || (size_t)row->get_index() >= ctx_.fonts.available_fonts().size())
        return -1;
    return row->get_index();
}

void TabFonts::update_preview()
{
    int index = selected_font_index();
    if (index < 0)
        preview_.show_font("", 0);
    else
        preview_.show_font(ctx_.fonts.font_file_path(ctx_.fonts.available_fonts()[index]),
                           size_spins_[0].get_value_as_int());
}

void TabFonts::on_font_selected()
{
    if (updating_)
        return;
    int index = selected_font_index();
    bool have = (index >= 0);
    const std::vector<std::string>& fields = FontManager::font_fields();
    set_ui_button_.set_sensitive(have && ctx_.fonts.has_font_field(fields[0]));
    set_menus_button_.set_sensitive(have && ctx_.fonts.has_font_field(fields[1]));
    set_map_button_.set_sensitive(have && ctx_.fonts.has_font_field(fields[2]));
    set_overmap_button_.set_sensitive(have && ctx_.fonts.has_font_field(fields[3]));
    set_all_button_.set_sensitive(have);
    description_label_.set_text(have ? ctx_.fonts.available_fonts()[index].description : "");
    update_preview();
}

void TabFonts::on_add_font_file()
{
    Glib::RefPtr<Gtk::FileDialog> dialog = Gtk::FileDialog::create();
    dialog->set_title("Choose a font file");
    Glib::RefPtr<Gtk::FileFilter> filter = Gtk::FileFilter::create();
    filter->set_name("Fonts (.ttf, .otf, .ttc)");
    filter->add_suffix("ttf");
    filter->add_suffix("otf");
    filter->add_suffix("ttc");
    Glib::RefPtr<Gio::ListStore<Gtk::FileFilter>> filters = Gio::ListStore<Gtk::FileFilter>::create();
    filters->append(filter);
    dialog->set_filters(filters);
    dialog->open(parent_, [this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
        try
        {
            Glib::RefPtr<Gio::File> file = dialog->open_finish(result);
            if (file)
                add_font_file(file->get_path());
        }
        catch (const Glib::Error&)
        {
            // Cancelled.
        }
    });
}

void TabFonts::add_font_file(const std::string& path)
{
    std::function<void()> add = [this, path]() {
        int index = ctx_.fonts.add_font_file(path);
        if (index >= 0)
            fill_fonts_list(index);
    };
    if (!ctx_.fonts.add_would_replace(path))
    {
        add();
        return;
    }
    std::string file = std::filesystem::path(path).filename().string();
    dialogs::confirm(parent_, "Replace " + file + "?",
                     "The user font folder already has a font file called " + file + ". It will be replaced.",
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
    guint selected = field_dropdown_.get_selected();
    const std::vector<std::string>& fields = FontManager::font_fields();
    return selected < fields.size() ? fields[selected] : fields[0];
}

void TabFonts::show_entries()
{
    updating_ = true;
    while (Gtk::ListBoxRow* row = entries_list_.get_row_at_index(0))
        entries_list_.remove(*row);

    std::string field = current_field();
    bool present = ctx_.fonts.has_font_field(field);
    missing_field_label_.set_visible(!present);
    missing_field_label_.set_text("This version of the game has no font list for this.");
    entries_ = ctx_.fonts.font_entries(field);

    const std::vector<std::string>& modes = FontManager::hinting_modes();
    std::vector<Glib::ustring> mode_names(modes.begin(), modes.end());
    for (size_t i = 0; i < entries_.size(); ++i)
    {
        const FontEntry& entry = entries_[i];
        Gtk::Box* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
        row->set_margin(4);

        std::string name = std::filesystem::path(entry.path).stem().string();
        Gtk::Label* label = make_label(i == 0 ? name : name + "  (fallback)");
        label->set_tooltip_text(entry.path);
        label->set_hexpand(true);
        row->append(*label);

        row->append(*make_label("Hinting:"));
        Gtk::DropDown* hinting = Gtk::make_managed<Gtk::DropDown>(Gtk::StringList::create(mode_names));
        std::vector<std::string>::const_iterator mode = std::find(modes.begin(), modes.end(), entry.hinting);
        if (mode != modes.end())
        {
            hinting->set_selected(mode - modes.begin());
            hinting->set_tooltip_text("How glyphs are fitted to the pixel grid. Bitmap is sharp and pixelated.");
        }
        else
        {
            hinting->set_selected(GTK_INVALID_LIST_POSITION);
            hinting->set_tooltip_text(entry.hinting.empty() ? "Not set in fonts.json: the game decides."
                                                            : "\"" + entry.hinting + "\" is not a mode this game knows.");
        }
        row->append(*hinting);

        Gtk::CheckButton* antialiasing = Gtk::make_managed<Gtk::CheckButton>("Antialiasing");
        antialiasing->set_active(entry.antialiasing);
        row->append(*antialiasing);

        hinting->property_selected().signal_changed().connect(
            [this, i, hinting, antialiasing]() { on_entry_style_changed(i, hinting, antialiasing); });
        antialiasing->signal_toggled().connect(
            [this, i, hinting, antialiasing]() { on_entry_style_changed(i, hinting, antialiasing); });

        entries_list_.append(*row);
    }
    updating_ = false;
    update_entry_buttons();
}

int TabFonts::selected_entry()
{
    Gtk::ListBoxRow* row = entries_list_.get_selected_row();
    if (!row || row->get_index() < 0 || (size_t)row->get_index() >= entries_.size())
        return -1;
    return row->get_index();
}

void TabFonts::update_entry_buttons()
{
    int index = selected_entry();
    move_up_button_.set_sensitive(index > 0);
    move_down_button_.set_sensitive(index >= 0 && (size_t)index + 1 < entries_.size());
    remove_button_.set_sensitive(index >= 0);
}

void TabFonts::on_entry_style_changed(size_t index, Gtk::DropDown* hinting, Gtk::CheckButton* antialiasing)
{
    if (updating_ || index >= entries_.size())
        return;
    const std::vector<std::string>& modes = FontManager::hinting_modes();
    guint mode = hinting->get_selected();
    // An entry without hinting gets the game's default with its first setting.
    std::string hinting_mode = mode < modes.size() ? modes[mode] : std::string("Default");

    if (ctx_.fonts.set_entry_style(current_field(), index, entries_[index].path, hinting_mode,
                                   antialiasing->get_active()))
    {
        entries_[index].hinting = hinting_mode;
        entries_[index].antialiasing = antialiasing->get_active();
        if (mode >= modes.size())
        {
            updating_ = true;
            hinting->set_selected(0);
            updating_ = false;
        }
        return;
    }
    // Show what the file has now. Not from inside the widget's own signal.
    Glib::signal_idle().connect_once([this]() { show_entries(); });
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
        entries_list_.select_row(*entries_list_.get_row_at_index(index + offset));
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
    std::string name = std::filesystem::path(path).stem().string();
    dialogs::confirm(parent_, "Remove " + name + " from the list?",
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
    if (!manual_check_.get_active())
    {
        for (int i = 0; i < 3; ++i)
        {
            FontDims dims = ctx_.fonts.auto_dims(SIZE_FIELDS[i], size_spins_[i].get_value_as_int());
            width_spins_[i].set_value(dims.width);
            height_spins_[i].set_value(dims.height);
        }
    }
    update_preview();
}

void TabFonts::on_manual_toggled()
{
    if (updating_)
        return;
    bool manual = manual_check_.get_active();
    for (int i = 0; i < 3; ++i)
    {
        width_spins_[i].set_sensitive(manual);
        height_spins_[i].set_sensitive(manual);
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
        dims[i]->size = size_spins_[i].get_value_as_int();
        dims[i]->width = width_spins_[i].get_value_as_int();
        dims[i]->height = height_spins_[i].get_value_as_int();
    }
    options.blending = blending_check_.get_active();
    options.ascii_lines = ascii_check_.get_active();
    ctx_.fonts.save_font_options(options);
}
