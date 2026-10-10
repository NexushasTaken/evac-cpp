#include "ui/tab_soundpacks.h"

static Gtk::Label* make_label(const Glib::ustring& text)
{
    Gtk::Label* label = Gtk::make_managed<Gtk::Label>(text);
    label->set_xalign(0.0f);
    return label;
}

TabSoundpacks::TabSoundpacks(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8),
      ctx_(ctx),
      runner_(runner),
      parent_(parent),
      updating_(false),
      content_(Gtk::Orientation::HORIZONTAL, 12)
{
    build_ui();
}

void TabSoundpacks::build_ui()
{
    set_margin(12);
    hint_label_.set_wrap(true);
    hint_label_.set_xalign(0.0f);
    append(hint_label_);
    content_.set_vexpand(true);
    append(content_);

    // Installed packs.
    Gtk::Box* left = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    left->set_hexpand(true);
    left->append(*make_label("Installed:"));
    Gtk::ScrolledWindow* installed_scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    installed_scroller->set_vexpand(true);
    installed_scroller->set_has_frame(true);
    installed_scroller->set_child(installed_list_);
    left->append(*installed_scroller);
    show_stock_check_.set_label("Show stock");
    show_stock_check_.set_tooltip_text("Show soundpacks from data/sound on the list.");
    left->append(show_stock_check_);
    delete_button_.set_label("Delete Selected");
    delete_button_.set_tooltip_text("Remove the selected soundpack from your game installation.");
    left->append(delete_button_);
    content_.append(*left);

    // Packs that can be downloaded.
    Gtk::Box* right = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    right->set_hexpand(true);
    right->append(*make_label("Downloadable:"));
    Gtk::ScrolledWindow* available_scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    available_scroller->set_vexpand(true);
    available_scroller->set_has_frame(true);
    available_scroller->set_child(available_list_);
    right->append(*available_scroller);
    install_button_.set_label("Install Selected");
    install_button_.set_tooltip_text("Download the selected soundpack and add it to your game installation.");
    right->append(install_button_);
    install_from_file_button_.set_label("Install From File...");
    install_from_file_button_.set_tooltip_text("Install the selected soundpack from a .zip you have already downloaded.");
    right->append(install_from_file_button_);
    content_.append(*right);

    // The list of downloadable packs never changes.
    const std::vector<SoundpackSource>& packs = available_soundpacks();
    for (size_t i = 0; i < packs.size(); ++i)
        available_list_.append(*make_label(packs[i].name));

    installed_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
    available_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);

    show_stock_check_.signal_toggled().connect(sigc::mem_fun(*this, &TabSoundpacks::on_show_stock_toggled));
    installed_list_.signal_row_selected().connect([this](Gtk::ListBoxRow*) { on_installed_selected(); });
    available_list_.signal_row_selected().connect([this](Gtk::ListBoxRow*) { on_available_selected(); });
    delete_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabSoundpacks::on_delete_clicked));
    install_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabSoundpacks::on_install_clicked));
    install_from_file_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabSoundpacks::on_install_from_file_clicked));
}

void TabSoundpacks::show_unavailable(const Glib::ustring& message)
{
    hint_label_.set_text(message);
    hint_label_.set_visible(true);
    content_.set_visible(false);
}

void TabSoundpacks::refresh()
{
    std::string game = ctx_.settings.get_string("game");
    Paths::InstallsSummary installs = ctx_.paths.installs_summary();
    if (installs.find(game) == installs.end())
    {
        show_unavailable("Install the game first (on the Game tab).");
        return;
    }
    hint_label_.set_visible(false);
    content_.set_visible(true);

    updating_ = true;
    show_stock_check_.set_active(ctx_.settings.get_bool("show_stock_sound"));
    updating_ = false;

    refresh_installed();
}

void TabSoundpacks::refresh_installed()
{
    installed_ = ctx_.soundpacks.get_installed(ctx_.settings.get_bool("show_stock_sound"));

    updating_ = true;
    while (Gtk::ListBoxRow* row = installed_list_.get_row_at_index(0))
        installed_list_.remove(*row);
    for (size_t i = 0; i < installed_.size(); ++i)
    {
        Gtk::Label* label = make_label(installed_[i].name);
        label->set_tooltip_text(installed_[i].description.empty() ? "No description provided." : installed_[i].description);
        if (installed_[i].is_stock)
            label->set_sensitive(false);  // Greyed out, like in the original.
        installed_list_.append(*label);
    }
    updating_ = false;

    update_buttons();
}

bool TabSoundpacks::is_installed(const std::string& pack_name)
{
    for (size_t i = 0; i < installed_.size(); ++i)
    {
        if (installed_[i].name == pack_name)
            return true;
    }
    return false;
}

int TabSoundpacks::selected_installed()
{
    Gtk::ListBoxRow* row = installed_list_.get_selected_row();
    if (!row || row->get_index() < 0 || (size_t)row->get_index() >= installed_.size())
        return -1;
    return row->get_index();
}

int TabSoundpacks::selected_available()
{
    Gtk::ListBoxRow* row = available_list_.get_selected_row();
    if (!row || row->get_index() < 0 || (size_t)row->get_index() >= available_soundpacks().size())
        return -1;
    return row->get_index();
}

void TabSoundpacks::update_buttons()
{
    int installed = selected_installed();
    delete_button_.set_sensitive(installed >= 0 && !installed_[installed].is_stock);

    int available = selected_available();
    install_button_.set_sensitive(available >= 0);
    install_from_file_button_.set_sensitive(available >= 0);
    bool reinstall = (available >= 0 && is_installed(available_soundpacks()[available].name));
    install_button_.set_label(reinstall ? "Reinstall Selected" : "Install Selected");
}

void TabSoundpacks::on_show_stock_toggled()
{
    if (updating_)
        return;
    ctx_.settings.set_bool("show_stock_sound", show_stock_check_.get_active());
    ctx_.settings.save();
    refresh_installed();
}

void TabSoundpacks::on_installed_selected()
{
    if (!updating_)
        update_buttons();
}

void TabSoundpacks::on_available_selected()
{
    if (!updating_)
        update_buttons();
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is done.
void TabSoundpacks::run_task(const std::function<void()>& work)
{
    content_.set_sensitive(false);
    bool started = runner_.run(work, [this]() {
        content_.set_sensitive(true);
        refresh_installed();
    });
    if (!started)
        content_.set_sensitive(true);
}

void TabSoundpacks::on_delete_clicked()
{
    int index = selected_installed();
    if (index < 0 || installed_[index].is_stock)
        return;
    std::string name = installed_[index].name;

    Glib::RefPtr<Gtk::AlertDialog> dialog = Gtk::AlertDialog::create("Delete the soundpack?");
    dialog->set_detail("Deleting " + name + ".");
    std::vector<Glib::ustring> buttons;
    buttons.push_back("Cancel");
    buttons.push_back("Delete");
    dialog->set_buttons(buttons);
    dialog->set_cancel_button(0);
    dialog->set_default_button(0);
    dialog->choose(parent_, [this, dialog, name](const Glib::RefPtr<Gio::AsyncResult>& result) {
        int choice = 0;
        try
        {
            choice = dialog->choose_finish(result);
        }
        catch (const Glib::Error&)
        {
            choice = 0;  // Dismissed.
        }
        if (choice == 1)
            run_task([this, name]() { ctx_.soundpacks.delete_pack(name); });
    });
}

void TabSoundpacks::on_install_clicked()
{
    install_selected("");
}

void TabSoundpacks::install_selected(const std::string& from_file)
{
    int index = selected_available();
    if (index < 0)
        return;
    SoundpackSource pack = available_soundpacks()[index];
    bool reinstall = is_installed(pack.name);
    bool keep_archive = !from_file.empty();  // Never delete the user's own file.

    run_task([this, pack, from_file, reinstall, keep_archive]() {
        ctx_.soundpacks.install_pack(pack, from_file, reinstall, keep_archive);
    });
}

void TabSoundpacks::on_install_from_file_clicked()
{
    if (selected_available() < 0)
        return;

    Glib::RefPtr<Gtk::FileDialog> dialog = Gtk::FileDialog::create();
    dialog->set_title("Choose the soundpack archive");
    std::string downloads = Glib::get_user_special_dir(Glib::UserDirectory::DOWNLOAD);
    if (!downloads.empty())
        dialog->set_initial_folder(Gio::File::create_for_path(downloads));
    dialog->open(parent_, [this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
        try
        {
            Glib::RefPtr<Gio::File> file = dialog->open_finish(result);
            if (file)
                install_selected(file->get_path());
        }
        catch (const Glib::Error&)
        {
            // Cancelled.
        }
    });
}
