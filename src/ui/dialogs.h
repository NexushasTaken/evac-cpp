#ifndef CATAPULT_DIALOGS_H
#define CATAPULT_DIALOGS_H

#include <gtkmm.h>

#include <functional>
#include <string>
#include <vector>

namespace dialogs
{

// Shows a message with buttons; answer gets the number of the button pressed.
// The first one must be the cancel button: it is the default, and the answer
// when the dialog is dismissed.
void ask(Gtk::Window& parent, const Glib::ustring& title, const Glib::ustring& detail,
         const std::vector<Glib::ustring>& buttons, const std::function<void(int)>& answer);

// Cancel and one other button; yes is called only when that one is pressed.
void confirm(Gtk::Window& parent, const Glib::ustring& title, const Glib::ustring& detail, const Glib::ustring& button,
             const std::function<void()>& yes);

// Just a message, with an OK button.
void tell(Gtk::Window& parent, const Glib::ustring& title, const Glib::ustring& detail);

// Asks for one line of text. check says why the text can't be used ("" if it
// can); the button stays disabled until it can. done is called only when the
// button is pressed.
void ask_text(Gtk::Window& parent, const Glib::ustring& title, const Glib::ustring& detail,
              const Glib::ustring& initial, const Glib::ustring& button,
              const std::function<std::string(const std::string&)>& check,
              const std::function<void(const std::string&)>& done);

// Wording for confirmations, depending on the "use_trash" setting.
std::string removal_text(bool use_trash);    // "will be moved to the trash" / "will be deleted for good"
std::string removal_button(bool use_trash);  // "Move to Trash" / "Delete"

void open_folder(Gtk::Window& parent, const std::string& dir);

}  // namespace dialogs

#endif
