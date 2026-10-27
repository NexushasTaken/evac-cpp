#include "ui/qt_helpers.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLocale>
#include <QWidget>

#include <algorithm>

#include "core/fs.h"

QString qstr(const std::string& utf8)
{
    return QString::fromStdString(utf8);
}

QString qstr(const std::filesystem::path& path)
{
    return QString::fromStdString(to_utf8(path));
}

std::string str(const QString& text)
{
    return text.toStdString();
}

QString escape(const std::string& utf8)
{
    return qstr(utf8).toHtmlEscaped().replace('\n', "<br>");
}

QString format_size(std::uint64_t bytes, bool iec)
{
    return QLocale().formattedDataSize((qint64)bytes, 1, iec ? QLocale::DataSizeIecFormat : QLocale::DataSizeSIFormat);
}

QLabel* make_label(const QString& text)
{
    QLabel* label = new QLabel(text);
    label->setTextFormat(Qt::PlainText);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return label;
}

QLabel* make_heading(const QString& text)
{
    QLabel* label = make_label(text);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

int selected_row(QAbstractItemView* view)
{
    if (!view->selectionModel())
        return -1;
    QModelIndexList rows = view->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return -1;
    int first = rows[0].row();
    for (int i = 1; i < rows.size(); ++i)
        first = std::min(first, rows[i].row());
    return first;
}

QBrush dim_brush(QWidget* widget)
{
    return widget->palette().brush(QPalette::Disabled, QPalette::Text);
}

void set_icon(QAbstractButton* button, const char* theme_name, const QString& fallback_text)
{
    QIcon icon = QIcon::fromTheme(theme_name);
    if (icon.isNull())
        button->setText(fallback_text);
    else
        button->setIcon(icon);
}
