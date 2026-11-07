#ifndef EVAC_DIALOGS_H
#define EVAC_DIALOGS_H

#include <QString>
#include <QStringList>
#include <filesystem>
#include <functional>
#include <string>

class QWidget;

// All of these return right away; the dialog stays open and the function
// given is called when it is answered (the dialogs are window-modal).
namespace dialogs {

// Shows a message with buttons; answer gets the number of the button pressed.
// The first one must be the cancel button: it is the default, and the answer
// when the dialog is dismissed.
void ask(QWidget& parent, const QString& title, const QString& detail,
         const QStringList& buttons, const std::function<void(int)>& answer);

// Cancel and one other button; yes is called only when that one is pressed.
void confirm(QWidget& parent, const QString& title, const QString& detail,
             const QString& button, const std::function<void()>& yes);

// Just a message, with an OK button.
void tell(QWidget& parent, const QString& title, const QString& detail);

// Asks for one line of text. check says why the text can't be used ("" if it
// can); the button stays disabled until it can. done is called only when the
// button is pressed.
void ask_text(QWidget& parent, const QString& title, const QString& detail,
              const QString& initial, const QString& button,
              const std::function<std::string(const std::string&)>& check,
              const std::function<void(const std::string&)>& done);

// Wording for confirmations, depending on the "use_trash" setting.
std::string_view removal_text(bool use_trash);  // "will be moved to the trash"
                                                // / "will be deleted for good"
std::string_view removal_button(bool use_trash);  // "Move to Trash" / "Delete"

// Opens a folder in the file manager. Does nothing if it doesn't exist.
void open_folder(const std::filesystem::path& dir);

}  // namespace dialogs

#endif
