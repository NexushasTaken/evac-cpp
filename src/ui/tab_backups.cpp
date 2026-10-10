#include "ui/tab_backups.h"

#include <ctime>

static const char* const NO_INFO_TEXT = "Select an existing backup to see its details here.";

static Gtk::Label* make_label(const Glib::ustring& text)
{
    Gtk::Label* label = Gtk::make_managed<Gtk::Label>(text);
    label->set_xalign(0.0f);
    return label;
}

TabBackups::TabBackups(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8),
      ctx_(ctx),
      runner_(runner),
      parent_(parent),
      updating_(false),
      content_(Gtk::Orientation::VERTICAL, 8)
{
    build_ui();
}

void TabBackups::build_ui()
{
    set_margin(12);
    hint_label_.set_wrap(true);
    hint_label_.set_xalign(0.0f);
    append(hint_label_);
    content_.set_vexpand(true);
    append(content_);

    content_.append(*make_label("Save directory backups:"));
    Gtk::ScrolledWindow* scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroller->set_vexpand(true);
    scroller->set_has_frame(true);
    scroller->set_child(list_);
    list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
    content_.append(*scroller);

    Gtk::Box* buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    buttons->set_homogeneous(true);
    restore_button_.set_label("Restore");
    restore_button_.set_tooltip_text("Replace your current saves with the selected backup.");
    delete_button_.set_label("Delete");
    delete_button_.set_tooltip_text("Delete the selected backup.");
    refresh_button_.set_label("Refresh");
    buttons->append(restore_button_);
    buttons->append(delete_button_);
    buttons->append(refresh_button_);
    content_.append(*buttons);

    info_label_.set_wrap(true);
    info_label_.set_xalign(0.0f);
    info_label_.set_yalign(0.0f);
    info_label_.set_use_markup(true);
    info_label_.set_selectable(true);
    Gtk::ScrolledWindow* info_scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    info_scroller->set_min_content_height(90);
    info_scroller->set_has_frame(true);
    info_scroller->set_child(info_label_);
    content_.append(*info_scroller);

    content_.append(*Gtk::make_managed<Gtk::Separator>());
    content_.append(*make_label("Manual backup"));
    Gtk::Box* create_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    create_row->append(*make_label("Name:"));
    name_entry_.set_hexpand(true);
    name_entry_.set_tooltip_text("Characters that are not allowed in file names, and spaces at the start or end, are not allowed here.");
    create_row->append(name_entry_);
    create_button_.set_label("Create");
    create_button_.set_tooltip_text("Back up the saved games of the current game.");
    create_row->append(create_button_);
    content_.append(*create_row);

    list_.signal_row_selected().connect([this](Gtk::ListBoxRow*) { on_selection_changed(); });
    name_entry_.signal_changed().connect(sigc::mem_fun(*this, &TabBackups::on_name_changed));
    name_entry_.signal_activate().connect(sigc::mem_fun(*this, &TabBackups::on_create_clicked));
    create_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabBackups::on_create_clicked));
    restore_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabBackups::on_restore_clicked));
    delete_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabBackups::on_delete_clicked));
    refresh_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabBackups::reload_list));
}

void TabBackups::show_unavailable(const Glib::ustring& message)
{
    hint_label_.set_text(message);
    hint_label_.set_visible(true);
    content_.set_visible(false);
}

void TabBackups::refresh()
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

    std::time_t now = std::time(nullptr);
    std::tm local;
    localtime_r(&now, &local);
    name_entry_.set_text(BackupManager::default_backup_name(local));

    reload_list();
}

void TabBackups::reload_list()
{
    backups_ = ctx_.backups.list();

    updating_ = true;
    while (Gtk::ListBoxRow* row = list_.get_row_at_index(0))
        list_.remove(*row);
    for (size_t i = 0; i < backups_.size(); ++i)
        list_.append(*make_label(backups_[i].name));
    updating_ = false;

    info_label_.set_markup(NO_INFO_TEXT);
    update_buttons();
}

int TabBackups::selected_backup()
{
    Gtk::ListBoxRow* row = list_.get_selected_row();
    if (!row || row->get_index() < 0 || (size_t)row->get_index() >= backups_.size())
        return -1;
    return row->get_index();
}

void TabBackups::update_buttons()
{
    bool have = (selected_backup() >= 0);
    restore_button_.set_sensitive(have);
    delete_button_.set_sensitive(have);
    create_button_.set_sensitive(BackupManager::is_valid_backup_name(name_entry_.get_text()));
}

void TabBackups::on_selection_changed()
{
    if (updating_)
        return;
    update_buttons();

    int index = selected_backup();
    if (index < 0)
    {
        info_label_.set_markup(NO_INFO_TEXT);
        return;
    }

    const BackupSummary& backup = backups_[index];
    std::string worlds;
    for (size_t i = 0; i < backup.worlds.size(); ++i)
        worlds += (i > 0 ? ", " : "") + backup.worlds[i];

    Glib::ustring text = "<u>Location:</u>\n<a href=\"" + Glib::Markup::escape_text(Glib::filename_to_uri(backup.path)) + "\">"
                         + Glib::Markup::escape_text(backup.path) + "</a>\n\n<u>Worlds:</u>\n"
                         + Glib::Markup::escape_text(worlds);
    info_label_.set_markup(text);
}

// Red entry text for a name that can't be used (the theme decides what "error" looks like).
void TabBackups::on_name_changed()
{
    std::string name = name_entry_.get_text();
    bool valid = BackupManager::is_valid_backup_name(name);
    if (name.empty() || valid)
        name_entry_.remove_css_class("error");
    else
        name_entry_.add_css_class("error");
    update_buttons();
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is done.
void TabBackups::run_task(const std::function<void()>& work)
{
    content_.set_sensitive(false);
    bool started = runner_.run(work, [this]() {
        content_.set_sensitive(true);
        reload_list();
    });
    if (!started)
        content_.set_sensitive(true);
}

void TabBackups::on_create_clicked()
{
    std::string name = name_entry_.get_text();
    if (!BackupManager::is_valid_backup_name(name))
        return;
    run_task([this, name]() { ctx_.backups.backup_current(name); });
}

void TabBackups::on_restore_clicked()
{
    int index = selected_backup();
    if (index < 0)
        return;
    std::string name = backups_[index].name;

    // This replaces the current saves, so make sure that is what the user wants.
    Glib::RefPtr<Gtk::AlertDialog> dialog = Gtk::AlertDialog::create("Restore this backup?");
    dialog->set_detail("Your current saves of this game will be replaced with the backup \"" + name
                       + "\". Create a backup of the current saves first if you want to keep them.");
    std::vector<Glib::ustring> buttons;
    buttons.push_back("Cancel");
    buttons.push_back("Restore");
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
            choice = 0;
        }
        if (choice == 1)
            run_task([this, name]() { ctx_.backups.restore(name); });
    });
}

void TabBackups::on_delete_clicked()
{
    int index = selected_backup();
    if (index < 0)
        return;
    std::string name = backups_[index].name;
    run_task([this, name]() { ctx_.backups.delete_backup(name); });
}
