#ifndef EVAC_FONT_PREVIEW_H
#define EVAC_FONT_PREVIEW_H

#include <gtkmm.h>

#include <string>

// Shows text in the font of a file at a pixel size, as a rough idea of how the
// game shows it: the game renders fonts itself, with the hinting and fallback
// fonts from fonts.json, which this does not.
// The file is made known to Pango (through fontconfig) the first time.
class FontPreview : public Gtk::Frame
{
public:
    FontPreview();

    // An empty path clears the preview.
    void show_font(const std::string& path, int pixel_size);

private:
    Gtk::Label label_;
};

#endif
