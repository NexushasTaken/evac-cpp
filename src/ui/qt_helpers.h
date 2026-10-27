#ifndef EVAC_QT_HELPERS_H
#define EVAC_QT_HELPERS_H

#include <QBrush>
#include <QString>

#include <cstdint>
#include <filesystem>
#include <string>

class QAbstractButton;
class QAbstractItemView;
class QLabel;
class QWidget;

// Small helpers shared by the tabs. The core uses UTF-8 std::string and
// std::filesystem::path; Qt uses QString. Convert where the two meet.

QString qstr(const std::string& utf8);
QString qstr(const std::filesystem::path& path);
std::string str(const QString& text);

// Escapes text for a rich text (HTML) label, and keeps its line breaks.
QString escape(const std::string& utf8);

// "1.5 MB" (SI units, like the file managers) or "1.4 MiB" (iec).
QString format_size(std::uint64_t bytes, bool iec = false);

// A left-aligned label with plain text.
QLabel* make_label(const QString& text);

// A bold label, for the heading of a section.
QLabel* make_heading(const QString& text);

// The row of the first selected item, or -1.
int selected_row(QAbstractItemView* view);

// The text color of disabled widgets, for list rows that should look greyed
// out but stay selectable.
QBrush dim_brush(QWidget* widget);

// An icon from the desktop's icon theme. Themes don't exist everywhere (Windows,
// macOS), so the button gets the text instead when the icon isn't there.
void set_icon(QAbstractButton* button, const char* theme_name, const QString& fallback_text);

#endif
