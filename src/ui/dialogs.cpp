#include "ui/dialogs.h"

namespace dialogs
{

void ask(Gtk::Window& parent, const Glib::ustring& title, const Glib::ustring& detail,
         const std::vector<Glib::ustring>& buttons, const std::function<void(int)>& answer)
{
    Glib::RefPtr<Gtk::AlertDialog> dialog = Gtk::AlertDialog::create(title);
    dialog->set_detail(detail);
    dialog->set_buttons(buttons);
    dialog->set_cancel_button(0);
    dialog->set_default_button(0);
    dialog->choose(parent, [dialog, answer](const Glib::RefPtr<Gio::AsyncResult>& result) {
        int choice = 0;
        try
        {
            choice = dialog->choose_finish(result);
        }
        catch (const Glib::Error&)
        {
            choice = 0;  // Dismissed.
        }
        answer(choice);
    });
}

void confirm(Gtk::Window& parent, const Glib::ustring& title, const Glib::ustring& detail, const Glib::ustring& button,
             const std::function<void()>& yes)
{
    std::vector<Glib::ustring> buttons;
    buttons.push_back("Cancel");
    buttons.push_back(button);
    ask(parent, title, detail, buttons, [yes](int choice) {
        if (choice == 1)
            yes();
    });
}

void tell(Gtk::Window& parent, const Glib::ustring& title, const Glib::ustring& detail)
{
    Glib::RefPtr<Gtk::AlertDialog> dialog = Gtk::AlertDialog::create(title);
    dialog->set_detail(detail);
    dialog->show(parent);
}

std::string removal_text(bool use_trash)
{
    return use_trash ? "will be moved to the trash" : "will be deleted for good";
}

std::string removal_button(bool use_trash)
{
    return use_trash ? "Move to Trash" : "Delete";
}

void open_folder(Gtk::Window& parent, const std::string& dir)
{
    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(dir);
    if (!file->query_exists())
        return;
    Glib::RefPtr<Gtk::FileLauncher> launcher = Gtk::FileLauncher::create(file);
    launcher->launch(parent, [launcher](const Glib::RefPtr<Gio::AsyncResult>& result) {
        try
        {
            launcher->launch_finish(result);
        }
        catch (const Glib::Error&)
        {
        }
    });
}

}  // namespace dialogs
