#include "ui/tab_fonts.h"

#include <cstdlib>
#include <filesystem>

static Gtk::Label* make_label(const Glib::ustring& text)
{
    Gtk::Label* label = Gtk::make_managed<Gtk::Label>(text);
    label->set_xalign(0.0f);
    return label;
}

// Game options are strings; a broken value falls back to the smallest size.
static int to_size(const std::string& text)
{
    int value = std::atoi(text.c_str());
    return value < 8 ? 8 : value;
}

TabFonts::TabFonts(AppContext& ctx)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8), ctx_(ctx), updating_(false), content_(Gtk::Orientation::VERTICAL, 8)
{
    build_ui();
}

void TabFonts::build_ui()
{
    set_margin(12);

    hint_label_.set_wrap(true);
    hint_label_.set_xalign(0.0f);
    append(hint_label_);
    append(content_);

    content_.append(*make_label("Available:"));
    Gtk::ScrolledWindow* scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroller->set_min_content_height(170);
    scroller->set_has_frame(true);
    scroller->set_child(fonts_list_);
    fonts_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
    content_.append(*scroller);

    description_label_.set_xalign(0.0f);
    description_label_.set_wrap(true);
    description_label_.set_use_markup(false);
    content_.append(description_label_);

    set_ui_button_.set_label("Set for Game UI");
    set_ui_button_.set_tooltip_text("Use selected font for menus/dialogs/etc.");
    set_map_button_.set_label("Set for Map");
    set_map_button_.set_tooltip_text("Use selected font for game world\n(when not using tiles).");
    set_overmap_button_.set_label("Set for Overmap");
    set_overmap_button_.set_tooltip_text("Use selected font for world map view a.k.a overmap.");
    set_all_button_.set_label("Set for All");
    set_all_button_.set_tooltip_text("Use selected font for everything.");
    Gtk::Box* set_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    set_row->set_homogeneous(true);
    set_row->append(set_ui_button_);
    set_row->append(set_map_button_);
    set_row->append(set_overmap_button_);
    set_row->append(set_all_button_);
    content_.append(*set_row);

    reset_button_.set_label("Reset All Fonts");
    reset_button_.set_tooltip_text("Return default fonts (Terminus => Unifont).");
    content_.append(reset_button_);

    current_config_label_.set_xalign(0.0f);
    current_config_label_.set_use_markup(true);
    current_config_label_.set_selectable(true);
    content_.append(current_config_label_);

    content_.append(*Gtk::make_managed<Gtk::Separator>());
    content_.append(*make_label("Other settings"));

    Gtk::SpinButton* spins[] = {&size_ui_, &size_map_, &size_overmap_};
    const char* const captions[] = {"UI font size:", "Map font size:", "Overmap font size:"};
    const char* const tooltips[] = {
        "Font size value for menus/dialogs/etc.\nFont width and height will be set automatically.",
        "Font size for game world (when not using tiles).\nFont width and height will be set automatically.",
        "Font size for world map view a.k.a overmap.\nFont width and height will be set automatically."};
    for (int i = 0; i < 3; ++i)
    {
        spins[i]->set_adjustment(Gtk::Adjustment::create(8, 8, 64, 1));
        spins[i]->set_tooltip_text(tooltips[i]);
        Gtk::Box* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
        Gtk::Label* caption = make_label(captions[i]);
        caption->set_hexpand(true);
        row->append(*caption);
        row->append(*spins[i]);
        content_.append(*row);
    }

    blending_check_.set_label("Font blending");
    blending_check_.set_tooltip_text("Smoother font rendering.");
    content_.append(blending_check_);

    save_button_.set_label("Save");
    save_button_.set_tooltip_text("Click to save these settings to options.json.\nThey are also saved automatically when setting a font.");
    content_.append(save_button_);

    fonts_list_.signal_row_selected().connect(sigc::mem_fun(*this, &TabFonts::on_font_selected));
    set_ui_button_.signal_clicked().connect([this]() { on_set_font(true, false, false); });
    set_map_button_.signal_clicked().connect([this]() { on_set_font(false, true, false); });
    set_overmap_button_.signal_clicked().connect([this]() { on_set_font(false, false, true); });
    set_all_button_.signal_clicked().connect([this]() { on_set_font(true, true, true); });
    reset_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabFonts::on_reset));
    save_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabFonts::on_save));
}

void TabFonts::show_unavailable(const Glib::ustring& message)
{
    hint_label_.set_text(message);
    hint_label_.set_visible(true);
    content_.set_visible(false);
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
    content_.set_visible(true);

    updating_ = true;
    while (Gtk::ListBoxRow* row = fonts_list_.get_row_at_index(0))
        fonts_list_.remove(*row);
    const std::vector<FontInfo>& fonts = ctx_.fonts.available_fonts();
    for (size_t i = 0; i < fonts.size(); ++i)
    {
        Gtk::Label* label = Gtk::make_managed<Gtk::Label>(fonts[i].name);
        label->set_xalign(0.0f);
        label->set_tooltip_text(fonts[i].description);
        fonts_list_.append(*label);
    }
    updating_ = false;

    description_label_.set_text("");
    set_ui_button_.set_sensitive(false);
    set_map_button_.set_sensitive(false);
    set_overmap_button_.set_sensitive(false);
    set_all_button_.set_sensitive(false);

    size_ui_.set_value(to_size(ctx_.fonts.get_game_option("FONT_SIZE")));
    size_map_.set_value(to_size(ctx_.fonts.get_game_option("MAP_FONT_SIZE")));
    size_overmap_.set_value(to_size(ctx_.fonts.get_game_option("OVERMAP_FONT_SIZE")));
    std::string blending = ctx_.fonts.get_game_option("FONT_BLENDING");
    blending_check_.set_active(blending == "true" || blending == "True");

    show_current_config();
}

void TabFonts::show_current_config()
{
    const char* const fields[] = {"typeface", "map_typeface", "overmap_typeface"};
    const char* const captions[] = {"UI", "Map", "Overmap"};

    Glib::ustring text = "<u>Current fonts + fallbacks:</u>";
    for (int i = 0; i < 3; ++i)
    {
        text += Glib::ustring("\n") + captions[i] + ":";
        std::vector<std::string> files = ctx_.fonts.font_files(fields[i]);
        for (size_t f = 0; f < files.size(); ++f)
        {
            // Show "Hack-Regular" instead of "data/font/Hack-Regular.ttf".
            std::string name = std::filesystem::path(files[f]).stem().string();
            text += (f > 0 ? "  =>  <i>" : "  <i>") + Glib::Markup::escape_text(name) + "</i>";
        }
    }
    current_config_label_.set_markup(text);
}

int TabFonts::selected_font_index()
{
    Gtk::ListBoxRow* row = fonts_list_.get_selected_row();
    if (!row || row->get_index() < 0 || (size_t)row->get_index() >= ctx_.fonts.available_fonts().size())
        return -1;
    return row->get_index();
}

void TabFonts::on_font_selected(Gtk::ListBoxRow*)
{
    if (updating_)
        return;
    int index = selected_font_index();
    bool have = (index >= 0);
    set_ui_button_.set_sensitive(have);
    set_map_button_.set_sensitive(have);
    set_overmap_button_.set_sensitive(have);
    set_all_button_.set_sensitive(have);
    description_label_.set_text(have ? ctx_.fonts.available_fonts()[index].description : "");
}

void TabFonts::on_set_font(bool ui, bool map, bool overmap)
{
    int index = selected_font_index();
    if (index < 0)
        return;
    const std::string& name = ctx_.fonts.available_fonts()[index].name;

    if (ui)
        ctx_.status.post("Setting " + name + " as game UI font.");
    if (map)
        ctx_.status.post("Setting " + name + " as map font.");
    if (overmap)
        ctx_.status.post("Setting " + name + " as overmap font.");

    if (ctx_.fonts.set_font((size_t)index, ui, map, overmap))
        on_save();  // The sizes depend on the fonts, so they are updated too.
    show_current_config();
}

void TabFonts::on_reset()
{
    if (ctx_.fonts.reset_font())
        on_save();
    show_current_config();
}

void TabFonts::on_save()
{
    ctx_.fonts.set_font_sizes(size_ui_.get_value_as_int(), size_map_.get_value_as_int(), size_overmap_.get_value_as_int());
    ctx_.fonts.set_game_option("FONT_BLENDING", blending_check_.get_active() ? "true" : "false");
    ctx_.fonts.write_game_options();
}
