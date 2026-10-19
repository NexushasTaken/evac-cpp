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

void ask_text(Gtk::Window& parent, const Glib::ustring& title, const Glib::ustring& detail,
              const Glib::ustring& initial, const Glib::ustring& button,
              const std::function<std::string(const std::string&)>& check,
              const std::function<void(const std::string&)>& done)
{
    // Gtk::AlertDialog has no text entry, so this is a small window of its own.
    Gtk::Window* window = new Gtk::Window();
    window->set_title(title);
    window->set_transient_for(parent);
    window->set_modal(true);
    window->set_resizable(false);
    window->set_default_size(420, -1);

    Gtk::Box* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    box->set_margin(16);
    Gtk::Label* heading = Gtk::make_managed<Gtk::Label>();
    heading->set_markup("<b>" + Glib::Markup::escape_text(title) + "</b>");
    heading->set_xalign(0.0f);
    Gtk::Label* text = Gtk::make_managed<Gtk::Label>(detail);
    text->set_wrap(true);
    text->set_max_width_chars(50);
    text->set_xalign(0.0f);
    Gtk::Entry* entry = Gtk::make_managed<Gtk::Entry>();
    entry->set_text(initial);
    entry->set_activates_default(true);
    Gtk::Label* problem = Gtk::make_managed<Gtk::Label>();
    problem->set_wrap(true);
    problem->set_xalign(0.0f);
    problem->add_css_class("error");
    Gtk::Box* buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    buttons->set_halign(Gtk::Align::END);
    buttons->set_margin_top(8);
    Gtk::Button* cancel = Gtk::make_managed<Gtk::Button>("Cancel");
    Gtk::Button* ok = Gtk::make_managed<Gtk::Button>(button);
    ok->add_css_class("suggested-action");
    buttons->append(*cancel);
    buttons->append(*ok);
    box->append(*heading);
    box->append(*text);
    box->append(*entry);
    box->append(*problem);
    box->append(*buttons);
    window->set_child(*box);
    window->set_default_widget(*ok);

    std::function<void()> update = [entry, problem, ok, check]() {
        std::string why = check(entry->get_text());
        problem->set_text(why);
        problem->set_visible(!why.empty());
        ok->set_sensitive(why.empty());
    };
    update();
    entry->signal_changed().connect(update);
    cancel->signal_clicked().connect([window]() { window->close(); });
    ok->signal_clicked().connect([window, entry, check, done]() {
        std::string value = entry->get_text();
        if (!check(value).empty())
            return;
        window->close();
        done(value);
    });
    Glib::RefPtr<Gtk::EventControllerKey> keys = Gtk::EventControllerKey::create();
    keys->signal_key_pressed().connect(
        [window](guint keyval, guint, Gdk::ModifierType) {
            if (keyval != GDK_KEY_Escape)
                return false;
            window->close();
            return true;
        },
        false);
    window->add_controller(keys);
    window->signal_hide().connect([window]() { Glib::signal_idle().connect_once([window]() { delete window; }); });
    window->present();
    entry->grab_focus();
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
