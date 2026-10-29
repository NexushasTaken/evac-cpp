#include "ui/font_preview.h"

#include <QFontDatabase>
#include <QLabel>
#include <QRawFont>
#include <QVBoxLayout>
#include <map>

#include "ui/qt_helpers.h"

static const char SAMPLE[] =
    "The quick brown fox jumps over the lazy dog.\n"
    "0123456789 !?@#$%&*()[]{}<>=+-~\n"
    "┌──┬──┐ │ ░▒▓█ ←↑→↓ ♥ ♦ ☺\n"
    "Съешь же ещё этих мягких булок. Ελληνικά";

// The font in a file ("Hack" "Bold" and so on), or false if Qt can't use it.
// Fonts added here stay known until the launcher exits.
static bool font_for_file(const QString& path, int pixel_size, QFont& font) {
  static std::map<QString, int>
      added;  // Path -> QFontDatabase id, -1 if it failed.
  std::map<QString, int>::const_iterator it = added.find(path);
  int id = (it != added.end())
               ? it->second
               : (added[path] = QFontDatabase::addApplicationFont(path));
  if (id < 0) return false;
  QStringList families = QFontDatabase::applicationFontFamilies(id);
  if (families.isEmpty()) return false;

  // A family has several files (Regular, Bold...): the style picks this one.
  QRawFont raw(path, pixel_size);
  font = QFont(families[0]);
  if (raw.isValid()) font.setStyleName(raw.styleName());
  font.setPixelSize(pixel_size);  // The game's font size is in pixels.
  return true;
}

FontPreview::FontPreview() {
  setTitle("Preview (roughly how it looks; the game draws fonts itself)");
  m_label = make_label("");
  m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_label->setContentsMargins(8, 8, 8, 8);
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->addWidget(m_label);
}

void FontPreview::show_font(const std::filesystem::path& path, int pixel_size) {
  if (path.empty()) {
    m_label->setFont(font());
    m_label->setText("Select a font to see it here.");
    return;
  }

  QFont preview;
  if (!font_for_file(qstr(path), pixel_size, preview)) {
    m_label->setFont(font());
    m_label->setText("This font can't be shown here.");
    return;
  }
  m_label->setText(SAMPLE);
  m_label->setFont(preview);
}
