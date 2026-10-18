#include "ui/font_preview.h"

#include <fontconfig/fontconfig.h>
#include <pango/pangofc-fontmap.h>

#include <set>

static const char SAMPLE[] = "The quick brown fox jumps over the lazy dog.\n"
                             "0123456789 !?@#$%&*()[]{}<>=+-~\n"
                             "┌──┬──┐ │ ░▒▓█ ←↑→↓ ♥ ♦ ☺\n"
                             "Съешь же ещё этих мягких булок. Ελληνικά";

// The Pango description ("Hack Bold" and so on) of the font in a file, or ""
// if Pango can't use it. Fonts added here stay known until the launcher exits.
static std::string description_for_file(Gtk::Widget& widget, const std::string& path)
{
    PangoFontMap* map = widget.get_pango_context()->get_font_map()->gobj();
    if (!PANGO_IS_FC_FONT_MAP(map))
        return "";
    PangoFcFontMap* fc_map = PANGO_FC_FONT_MAP(map);
    FcConfig* config = pango_fc_font_map_get_config(fc_map);

    static std::set<std::string> added;
    if (added.find(path) == added.end())
    {
        if (!FcConfigAppFontAddFile(config, (const FcChar8*)path.c_str()))
            return "";
        pango_fc_font_map_config_changed(fc_map);
        added.insert(path);
    }

    FcPattern* pattern = FcPatternCreate();
    FcPatternAddString(pattern, FC_FILE, (const FcChar8*)path.c_str());
    FcObjectSet* objects = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_WEIGHT, FC_SLANT, FC_WIDTH, (char*)nullptr);
    FcFontSet* found = FcFontList(config, pattern, objects);
    FcObjectSetDestroy(objects);
    FcPatternDestroy(pattern);

    std::string result;
    if (found && found->nfont > 0)
    {
        PangoFontDescription* desc = pango_fc_font_description_from_pattern(found->fonts[0], FALSE);
        char* text = pango_font_description_to_string(desc);
        result = text;
        g_free(text);
        pango_font_description_free(desc);
    }
    if (found)
        FcFontSetDestroy(found);
    return result;
}

FontPreview::FontPreview()
{
    set_label("Preview (roughly how it looks; the game draws fonts itself)");
    label_.set_xalign(0.0f);
    label_.set_margin(8);
    label_.set_selectable(true);
    set_child(label_);
}

void FontPreview::show_font(const std::string& path, int pixel_size)
{
    Pango::AttrList none;
    if (path.empty())
    {
        label_.set_attributes(none);
        label_.set_text("Select a font to see it here.");
        return;
    }

    std::string description = description_for_file(label_, path);
    if (description.empty())
    {
        label_.set_attributes(none);
        label_.set_text("This font can't be shown here.");
        return;
    }

    Pango::AttrList attributes;
    Pango::Attribute font = Pango::Attribute::create_attr_font_desc(Pango::FontDescription(description));
    attributes.insert(font);
    // The game's font size is in pixels.
    Pango::Attribute size = Pango::Attribute::create_attr_size_absolute(pixel_size * PANGO_SCALE);
    attributes.insert(size);
    label_.set_text(SAMPLE);
    label_.set_attributes(attributes);
}
