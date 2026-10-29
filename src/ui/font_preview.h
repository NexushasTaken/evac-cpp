#ifndef EVAC_FONT_PREVIEW_H
#define EVAC_FONT_PREVIEW_H

#include <QGroupBox>
#include <filesystem>

class QLabel;

// Shows text in the font of a file at a pixel size, as a rough idea of how the
// game shows it: the game renders fonts itself, with the hinting and fallback
// fonts from fonts.json, which this does not.
// The file is made known to Qt (QFontDatabase) the first time.
class FontPreview : public QGroupBox {
 public:
  FontPreview();

  // An empty path clears the preview.
  void show_font(const std::filesystem::path& path, int pixel_size);

 private:
  QLabel* label_;
};

#endif
