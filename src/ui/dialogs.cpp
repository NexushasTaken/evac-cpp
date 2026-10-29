#include "ui/dialogs.h"

#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <vector>

#include "ui/qt_helpers.h"

namespace dialogs {

// The title is the main text and the detail the smaller text under it.
static QMessageBox* make_message(QWidget& parent, const QString& title,
                                 const QString& detail) {
  QMessageBox* box = new QMessageBox(&parent);
  box->setAttribute(Qt::WA_DeleteOnClose);
  box->setWindowTitle(title);
  box->setTextFormat(Qt::PlainText);
  box->setText(title);
  // As rich text, so that names with "<" in them can't be taken for markup.
  if (!detail.isEmpty())
    box->setInformativeText(
        Qt::convertFromPlainText(detail, Qt::WhiteSpaceNormal));
  return box;
}

void ask(QWidget& parent, const QString& title, const QString& detail,
         const QStringList& buttons, const std::function<void(int)>& answer) {
  QMessageBox* box = make_message(parent, title, detail);
  std::vector<QAbstractButton*> added;
  for (int i = 0; i < buttons.size(); ++i) {
    QPushButton* button = box->addButton(
        buttons[i], i == 0 ? QMessageBox::RejectRole : QMessageBox::AcceptRole);
    added.push_back(button);
    if (i == 0) {
      box->setDefaultButton(button);
      box->setEscapeButton(button);
    }
  }
  QObject::connect(box, &QDialog::finished, box, [box, added, answer](int) {
    int choice = 0;  // Dismissed.
    for (size_t i = 0; i < added.size(); ++i) {
      if (box->clickedButton() == added[i]) choice = (int)i;
    }
    answer(choice);
  });
  box->open();
}

void confirm(QWidget& parent, const QString& title, const QString& detail,
             const QString& button, const std::function<void()>& yes) {
  QStringList buttons;
  buttons << "Cancel" << button;
  ask(parent, title, detail, buttons, [yes](int choice) {
    if (choice == 1) yes();
  });
}

void tell(QWidget& parent, const QString& title, const QString& detail) {
  QMessageBox* box = make_message(parent, title, detail);
  box->setStandardButtons(QMessageBox::Ok);
  box->open();
}

void ask_text(QWidget& parent, const QString& title, const QString& detail,
              const QString& initial, const QString& button,
              const std::function<std::string(const std::string&)>& check,
              const std::function<void(const std::string&)>& done) {
  // QMessageBox has no text entry, so this is a small dialog of its own.
  QDialog* dialog = new QDialog(&parent);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(title);
  dialog->setMinimumWidth(420);

  QVBoxLayout* box = new QVBoxLayout(dialog);
  box->setSpacing(8);
  box->addWidget(make_heading(title));
  QLabel* text = make_label(detail);
  text->setWordWrap(true);
  box->addWidget(text);
  QLineEdit* entry = new QLineEdit(initial);
  box->addWidget(entry);
  QLabel* problem = make_label("");
  problem->setWordWrap(true);
  problem->setStyleSheet("color: #d02020;");
  box->addWidget(problem);
  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
  QPushButton* ok = buttons->addButton(button, QDialogButtonBox::AcceptRole);
  ok->setDefault(true);
  box->addWidget(buttons);

  std::function<void()> update = [entry, problem, ok, check]() {
    std::string why = check(str(entry->text()));
    problem->setText(qstr(why));
    problem->setVisible(!why.empty());
    ok->setEnabled(why.empty());
  };
  update();
  QObject::connect(entry, &QLineEdit::textChanged, dialog, update);
  QObject::connect(buttons, &QDialogButtonBox::rejected, dialog,
                   &QDialog::reject);
  QObject::connect(buttons, &QDialogButtonBox::accepted, dialog,
                   [dialog, entry, check, done]() {
                     std::string value = str(entry->text());
                     if (!check(value).empty()) return;
                     dialog->accept();
                     done(value);
                   });
  dialog->open();
  entry->setFocus();
}

std::string removal_text(bool use_trash) {
  return use_trash ? "will be moved to the trash" : "will be deleted for good";
}

std::string removal_button(bool use_trash) {
  return use_trash ? "Move to Trash" : "Delete";
}

void open_folder(const std::filesystem::path& dir) {
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec)) return;
  QDesktopServices::openUrl(QUrl::fromLocalFile(qstr(dir)));
}

}  // namespace dialogs
