#include "ui/licenses_dialog.h"

#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QVBoxLayout>

#include "core/licenses.h"
#include "ui/qt_helpers.h"

LicensesDialog::LicensesDialog(QWidget& parent, const std::filesystem::path& data_dir) : QDialog(&parent)
{
    setWindowTitle("Open source licenses");
    setModal(true);
    resize(720, 600);

    QPlainTextEdit* text_view = new QPlainTextEdit();
    text_view->setReadOnly(true);
    text_view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    text_view->document()->setDocumentMargin(12);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(text_view);

    QTextBlockFormat heading_block;
    heading_block.setAlignment(Qt::AlignHCenter);
    heading_block.setTopMargin(12);
    heading_block.setBottomMargin(12);
    QTextCharFormat heading;
    heading.setFontWeight(QFont::Bold);
    heading.setFontPointSize(text_view->font().pointSizeF() * 1.3);
    QTextBlockFormat body_block;
    QTextCharFormat body;

    QTextCursor cursor(text_view->document());
    std::vector<LicenseSection> sections = load_license_sections(data_dir);
    for (size_t i = 0; i < sections.size(); ++i)
    {
        if (i > 0)
            cursor.insertBlock(heading_block, heading);
        else
            cursor.setBlockFormat(heading_block);
        cursor.insertText(qstr(sections[i].heading), heading);
        cursor.insertBlock(body_block, body);
        cursor.insertText(qstr(sections[i].text + "\n"), body);
    }
    text_view->moveCursor(QTextCursor::Start);
}
