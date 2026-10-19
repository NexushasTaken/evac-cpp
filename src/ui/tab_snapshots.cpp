#include "ui/tab_snapshots.h"

#include <ctime>
#include <memory>

#include "core/git.h"
#include "ui/dialogs.h"

static const char* const NO_DETAILS_TEXT = "Select a snapshot to see its details here.";

static Gtk::Label* make_label(const Glib::ustring& text)
{
    Gtk::Label* label = Gtk::make_managed<Gtk::Label>(text);
    label->set_xalign(0.0f);
    return label;
}

static Glib::ustring format_time(std::int64_t time, const char* format)
{
    Glib::DateTime when = Glib::DateTime::create_now_local((gint64)time);
    return when ? when.format(format) : Glib::ustring();
}

static Glib::ustring state_text(const WorldInfo& world)
{
    switch (world.state)
    {
    case WorldInfo::NO_SNAPSHOTS:
        return "no snapshots";
    case WorldInfo::CHANGED:
        return "\xe2\x97\x8f changed";  // ●
    case WorldInfo::DELETED:
        return "deleted";
    case WorldInfo::UNCHANGED:
        break;
    }
    return "";
}

TabSnapshots::TabSnapshots(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8),
      ctx_(ctx),
      runner_(runner),
      parent_(parent),
      updating_(false),
      content_(Gtk::Orientation::VERTICAL, 8)
{
    build_ui();
}

void TabSnapshots::build_ui()
{
    set_margin(12);
    hint_label_.set_wrap(true);
    hint_label_.set_xalign(0.0f);
    append(hint_label_);
    content_.set_vexpand(true);
    append(content_);

    // Draggable, so long world names can be shown in full.
    Gtk::Paned* columns = Gtk::make_managed<Gtk::Paned>(Gtk::Orientation::HORIZONTAL);
    columns->set_vexpand(true);
    columns->set_wide_handle(true);
    columns->set_resize_start_child(false);
    columns->set_shrink_start_child(false);
    columns->set_shrink_end_child(false);
    columns->set_position(340);
    content_.append(*columns);

    // Left: the worlds.
    Gtk::Box* left = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    left->append(*make_label("Worlds"));
    Gtk::ScrolledWindow* worlds_scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    worlds_scroller->set_vexpand(true);
    worlds_scroller->set_has_frame(true);
    worlds_scroller->set_min_content_width(220);
    worlds_scroller->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    worlds_scroller->set_child(worlds_list_);
    worlds_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
    left->append(*worlds_scroller);
    left->set_margin_end(6);
    columns->set_start_child(*left);

    // Right: the snapshots of the selected world.
    Gtk::Box* right = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    right->set_hexpand(true);
    snapshots_title_.set_xalign(0.0f);
    snapshots_title_.set_hexpand(true);
    snapshots_title_.set_ellipsize(Pango::EllipsizeMode::END);
    // Rarely needed actions on the selected world's snapshots.
    Gtk::Popover* popover = Gtk::make_managed<Gtk::Popover>();
    Gtk::Box* popover_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    popover_box->set_margin(6);
    export_button_.set_label("Export snapshots\xe2\x80\xa6");
    export_button_.set_tooltip_text("Save every snapshot of this world to one file (a git bundle), to keep elsewhere or share.");
    popover_box->append(export_button_);
    popover->set_child(*popover_box);
    menu_button_.set_icon_name("view-more-symbolic");
    menu_button_.set_tooltip_text("More actions");
    menu_button_.set_popover(*popover);
    Gtk::Box* title_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    title_row->append(snapshots_title_);
    title_row->append(menu_button_);
    right->append(*title_row);
    Gtk::ScrolledWindow* snapshots_scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    snapshots_scroller->set_vexpand(true);
    snapshots_scroller->set_has_frame(true);
    snapshots_scroller->set_child(snapshots_list_);
    snapshots_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
    right->append(*snapshots_scroller);

    Gtk::Box* buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    buttons->set_homogeneous(true);
    restore_button_.set_label("Restore snapshot");
    restore_button_.set_tooltip_text("Make the world match the selected snapshot. Changes not in a snapshot are kept as a snapshot first.");
    discard_button_.set_label("Discard changes");
    discard_button_.set_tooltip_text("Throw away the changes since the last snapshot.");
    buttons->append(restore_button_);
    buttons->append(discard_button_);
    right->append(*buttons);

    details_label_.set_wrap(true);
    details_label_.set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    details_label_.set_xalign(0.0f);
    details_label_.set_yalign(0.0f);
    details_label_.set_selectable(true);
    Gtk::ScrolledWindow* details_scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    details_scroller->set_min_content_height(90);
    details_scroller->set_has_frame(true);
    details_scroller->set_child(details_label_);
    right->append(*details_scroller);
    right->set_margin_start(6);
    columns->set_end_child(*right);

    // Taking snapshots.
    content_.append(*Gtk::make_managed<Gtk::Separator>());
    Gtk::Grid* form = Gtk::make_managed<Gtk::Grid>();
    form->set_row_spacing(6);
    form->set_column_spacing(6);
    form->attach(*make_label("Name:"), 0, 0);
    name_entry_.set_hexpand(true);
    form->attach(name_entry_, 1, 0);
    form->attach(*make_label("Description:"), 0, 1);
    description_entry_.set_placeholder_text("Optional");
    form->attach(description_entry_, 1, 1);
    Gtk::Box* take_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    take_button_.set_label("Take snapshot");
    take_button_.set_tooltip_text("Take a snapshot of the selected world.");
    take_all_button_.set_label("Take snapshot of all worlds");
    take_all_button_.set_tooltip_text("Take a snapshot of every world that has changes.");
    take_row->append(take_button_);
    take_row->append(take_all_button_);
    form->attach(*take_row, 1, 2);
    content_.append(*form);

    // Footer: size and the command to use the repository from a terminal.
    content_.append(*Gtk::make_managed<Gtk::Separator>());
    Gtk::Box* footer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    footer_label_.set_xalign(0.0f);
    command_label_.set_xalign(0.0f);
    command_label_.set_hexpand(true);
    command_label_.set_ellipsize(Pango::EllipsizeMode::MIDDLE);
    command_label_.set_selectable(true);
    command_label_.add_css_class("monospace");
    copy_button_.set_label("Copy");
    copy_button_.set_tooltip_text("Copy the git command for this world, to use its snapshots from a terminal.");
    footer->append(footer_label_);
    footer->append(command_label_);
    footer->append(copy_button_);
    content_.append(*footer);

    worlds_list_.signal_row_selected().connect([this](Gtk::ListBoxRow*) { on_world_selected(); });
    snapshots_list_.signal_row_selected().connect([this](Gtk::ListBoxRow*) { on_snapshot_selected(); });
    name_entry_.signal_changed().connect(sigc::mem_fun(*this, &TabSnapshots::update_buttons));
    name_entry_.signal_activate().connect(sigc::mem_fun(*this, &TabSnapshots::on_take_clicked));
    description_entry_.signal_activate().connect(sigc::mem_fun(*this, &TabSnapshots::on_take_clicked));
    take_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabSnapshots::on_take_clicked));
    take_all_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabSnapshots::on_take_all_clicked));
    restore_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabSnapshots::on_restore_clicked));
    discard_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabSnapshots::on_discard_clicked));
    copy_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabSnapshots::on_copy_clicked));
    export_button_.signal_clicked().connect([this, popover]() {
        popover->popdown();
        on_export_clicked();
    });
}

void TabSnapshots::show_unavailable(const Glib::ustring& message)
{
    hint_label_.set_text(message);
    hint_label_.set_visible(true);
    content_.set_visible(false);
}

void TabSnapshots::refresh()
{
    std::string game = ctx_.settings.get_string("game");
    Paths::InstallsSummary installs = ctx_.paths.installs_summary();
    if (installs.find(game) == installs.end())
    {
        show_unavailable("Install the game first (on the Game tab).");
        return;
    }
    if (!git::available())
    {
        show_unavailable("Snapshots need git 2.23 or newer, and it was not found. Install git with your package "
                         "manager, then come back to this tab.");
        return;
    }
    hint_label_.set_visible(false);
    content_.set_visible(true);

    std::time_t now = std::time(nullptr);
    std::tm local;
    localtime_r(&now, &local);
    name_entry_.set_text(SnapshotManager::default_snapshot_name(local));
    description_entry_.set_text("");

    reload();
}

// git status for every world can take a while, so it runs on the worker thread.
void TabSnapshots::reload()
{
    std::shared_ptr<std::vector<WorldInfo> > loaded = std::make_shared<std::vector<WorldInfo> >();
    content_.set_sensitive(false);
    bool started = runner_.run([this, loaded]() { *loaded = ctx_.snapshots.list_worlds(); },
                               [this, loaded]() {
                                   content_.set_sensitive(true);
                                   fill_worlds(*loaded);
                                   ask_about_deleted_worlds();
                               });
    if (!started)
        content_.set_sensitive(true);
}

void TabSnapshots::fill_worlds(const std::vector<WorldInfo>& worlds)
{
    worlds_ = worlds;

    updating_ = true;
    while (Gtk::ListBoxRow* row = worlds_list_.get_row_at_index(0))
        worlds_list_.remove(*row);
    int reselect = -1;
    for (size_t i = 0; i < worlds_.size(); ++i)
    {
        const WorldInfo& world = worlds_[i];
        Gtk::Box* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
        Gtk::Label* name = make_label(world.name);
        name->set_hexpand(true);
        name->set_ellipsize(Pango::EllipsizeMode::END);
        Gtk::Label* state = make_label(state_text(world));
        state->add_css_class("dim-label");
        if (world.state == WorldInfo::DELETED)
            name->add_css_class("dim-label");
        row->append(*name);
        row->append(*state);
        row->set_tooltip_text(world.name);
        worlds_list_.append(*row);
        if (world.name == selected_name_)
            reselect = (int)i;
    }
    if (reselect < 0 && !worlds_.empty())
        reselect = 0;
    if (reselect >= 0)
        worlds_list_.select_row(*worlds_list_.get_row_at_index(reselect));
    updating_ = false;

    on_world_selected();
}

const WorldInfo* TabSnapshots::selected_world()
{
    Gtk::ListBoxRow* row = worlds_list_.get_selected_row();
    if (!row || row->get_index() < 0 || (size_t)row->get_index() >= worlds_.size())
        return nullptr;
    return &worlds_[row->get_index()];
}

const Snapshot* TabSnapshots::selected_snapshot()
{
    Gtk::ListBoxRow* row = snapshots_list_.get_selected_row();
    if (!row || row->get_index() < 0 || (size_t)row->get_index() >= snapshots_.size())
        return nullptr;
    return &snapshots_[row->get_index()];
}

void TabSnapshots::on_world_selected()
{
    if (updating_)
        return;
    const WorldInfo* world = selected_world();
    selected_name_ = world ? world->name : "";
    fill_snapshots();
}

// Reading one world's history is quick, so this stays on the main thread.
void TabSnapshots::fill_snapshots()
{
    const WorldInfo* world = selected_world();
    snapshots_ = world ? ctx_.snapshots.list_snapshots(world->name) : std::vector<Snapshot>();

    updating_ = true;
    while (Gtk::ListBoxRow* row = snapshots_list_.get_row_at_index(0))
        snapshots_list_.remove(*row);
    for (size_t i = 0; i < snapshots_.size(); ++i)
    {
        Gtk::Box* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
        Gtk::Label* when = make_label(format_time(snapshots_[i].time, "%b %d %H:%M"));
        when->add_css_class("dim-label");
        Gtk::Label* name = make_label(snapshots_[i].name);
        name->set_hexpand(true);
        name->set_ellipsize(Pango::EllipsizeMode::END);
        row->append(*when);
        row->append(*name);
        snapshots_list_.append(*row);
    }
    updating_ = false;

    if (!world)
    {
        snapshots_title_.set_text(worlds_.empty() ? "There are no worlds yet." : "Select a world.");
        footer_label_.set_text("");
        command_label_.set_text("");
    }
    else
    {
        snapshots_title_.set_text("Snapshots of " + world->name);
        if (snapshots_.empty())
            footer_label_.set_text("No snapshots yet");
        else
            footer_label_.set_text(std::to_string(snapshots_.size()) + (snapshots_.size() == 1 ? " snapshot, " : " snapshots, ")
                                   + Glib::format_size(ctx_.snapshots.history_size(world->name)));
        command_label_.set_text(ctx_.snapshots.git_command(world->name));
        command_label_.set_tooltip_text(command_label_.get_text());
    }
    update_details();
    update_buttons();
}

void TabSnapshots::on_snapshot_selected()
{
    if (updating_)
        return;
    update_details();
    update_buttons();
}

void TabSnapshots::update_details()
{
    const WorldInfo* world = selected_world();
    const Snapshot* snap = selected_snapshot();
    if (!world || !snap)
    {
        details_label_.set_text(NO_DETAILS_TEXT);
        return;
    }
    int files = ctx_.snapshots.changed_files(world->name, snap->hash);
    Glib::ustring line = format_time(snap->time, "%b %d %Y, %H:%M");
    if (files >= 0)
        line += " \xc2\xb7 " + std::to_string(files) + (files == 1 ? " file changed" : " files changed");
    Glib::ustring text = "<b>" + Glib::Markup::escape_text(snap->name) + "</b>\n" + Glib::Markup::escape_text(line);
    if (!snap->description.empty())
        text += "\n\n" + Glib::Markup::escape_text(snap->description);
    details_label_.set_markup(text);
}

void TabSnapshots::update_buttons()
{
    const WorldInfo* world = selected_world();
    bool deleted = world && world->state == WorldInfo::DELETED;
    bool has_name = !Glib::ustring(name_entry_.get_text()).empty();
    bool any_world = false;
    for (size_t i = 0; i < worlds_.size(); ++i)
        any_world = any_world || worlds_[i].state != WorldInfo::DELETED;

    restore_button_.set_sensitive(world && selected_snapshot());
    discard_button_.set_sensitive(world && world->state == WorldInfo::CHANGED);
    take_button_.set_sensitive(world && !deleted && has_name);
    take_all_button_.set_sensitive(any_world && has_name);
    copy_button_.set_sensitive(world != nullptr);
    export_button_.set_sensitive(world && !snapshots_.empty());
    menu_button_.set_sensitive(world != nullptr);
}

// A world folder that disappeared (deleted in the game, or by restoring a zip
// backup) while its snapshots stayed. Asks once per world: keep them or trash them.
void TabSnapshots::ask_about_deleted_worlds()
{
    for (size_t i = 0; i < worlds_.size(); ++i)
    {
        const WorldInfo& world = worlds_[i];
        if (world.state != WorldInfo::DELETED || world.kept || asked_.count(world.name))
            continue;

        std::string name = world.name;
        asked_.insert(name);
        bool use_trash = ctx_.settings.get_bool("use_trash");
        std::vector<Glib::ustring> buttons;
        buttons.push_back("Cancel");
        buttons.push_back(dialogs::removal_button(use_trash));
        dialogs::ask(parent_, name + " no longer exists",
                     "Delete its snapshots too? They " + dialogs::removal_text(use_trash)
                         + ".\n\nCancel keeps them: the world stays in the list as deleted, and restoring one of its "
                           "snapshots brings it back.",
                     buttons, [this, name](int choice) {
                         if (choice == 1)
                         {
                             run_task([this, name]() { ctx_.snapshots.delete_snapshots(name); });
                         }
                         else
                         {
                             ctx_.snapshots.mark_kept(name);
                             reload();
                         }
                     });
        return;  // One dialog at a time; the reload after the answer asks about the next one.
    }
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is done.
void TabSnapshots::run_task(const std::function<void()>& work)
{
    content_.set_sensitive(false);
    bool started = runner_.run(work, [this]() {
        content_.set_sensitive(true);
        reload();
    });
    if (!started)
        content_.set_sensitive(true);
}

// Says so in a dialog, instead of failing with a git error.
bool TabSnapshots::check_writable()
{
    std::string reason;
    if (ctx_.snapshots.can_write(reason))
        return true;
    dialogs::tell(parent_, "Can't change the saves", reason);
    return false;
}

void TabSnapshots::on_take_clicked()
{
    const WorldInfo* world = selected_world();
    std::string name = name_entry_.get_text();
    if (!world || world->state == WorldInfo::DELETED || name.empty() || !check_writable())
        return;
    std::string world_name = world->name;
    std::string description = description_entry_.get_text();
    run_task([this, world_name, name, description]() { ctx_.snapshots.take_snapshot(world_name, name, description); });
}

void TabSnapshots::on_take_all_clicked()
{
    std::string name = name_entry_.get_text();
    if (name.empty() || !check_writable())
        return;
    std::string description = description_entry_.get_text();
    run_task([this, name, description]() { ctx_.snapshots.take_snapshot_all(name, description); });
}

void TabSnapshots::on_restore_clicked()
{
    const WorldInfo* world = selected_world();
    const Snapshot* snap = selected_snapshot();
    if (!world || !snap || !check_writable())
        return;
    std::string world_name = world->name;
    std::string hash = snap->hash;

    std::string detail = (world->state == WorldInfo::DELETED)
                             ? world_name + " will be brought back as it was in \"" + snap->name + "\"."
                             : world_name + " will be changed to match \"" + snap->name
                                   + "\". Changes not in a snapshot are first kept as the snapshot \"Before restoring "
                                   + snap->name + "\", so nothing is lost.";
    dialogs::confirm(parent_, "Restore this snapshot?", detail + " Close the game first.", "Restore",
                     [this, world_name, hash]() {
                         run_task([this, world_name, hash]() { ctx_.snapshots.restore(world_name, hash); });
                     });
}

void TabSnapshots::on_discard_clicked()
{
    const WorldInfo* world = selected_world();
    if (!world || world->state != WorldInfo::CHANGED || !check_writable())
        return;
    std::string world_name = world->name;
    std::string last = snapshots_.empty() ? "" : " \"" + snapshots_[0].name + "\"";
    dialogs::confirm(parent_, "Discard changes?",
                     "The changes to " + world_name + " since the last snapshot" + last
                         + " will be thrown away. They are not in any snapshot, so they can't be brought back.",
                     "Discard", [this, world_name]() {
                         run_task([this, world_name]() { ctx_.snapshots.discard_changes(world_name); });
                     });
}

void TabSnapshots::on_copy_clicked()
{
    const WorldInfo* world = selected_world();
    if (!world)
        return;
    get_clipboard()->set_text(ctx_.snapshots.git_command(world->name));
    ctx_.status.post("Copied the git command for " + world->name + ".");
}

void TabSnapshots::on_export_clicked()
{
    const WorldInfo* world = selected_world();
    if (!world || snapshots_.empty())
        return;
    std::string world_name = world->name;
    Glib::RefPtr<Gtk::FileDialog> dialog = Gtk::FileDialog::create();
    dialog->set_title("Export the snapshots of " + world_name);
    dialog->set_initial_name(world_name + ".bundle");
    Glib::RefPtr<Gtk::FileFilter> filter = Gtk::FileFilter::create();
    filter->set_name("Git bundles (.bundle)");
    filter->add_suffix("bundle");
    Glib::RefPtr<Gio::ListStore<Gtk::FileFilter>> filters = Gio::ListStore<Gtk::FileFilter>::create();
    filters->append(filter);
    dialog->set_filters(filters);
    // The file dialog asks before replacing an existing file.
    dialog->save(parent_, [this, dialog, world_name](const Glib::RefPtr<Gio::AsyncResult>& result) {
        Glib::RefPtr<Gio::File> file;
        try
        {
            file = dialog->save_finish(result);
        }
        catch (const Glib::Error&)
        {
            return;  // Cancelled.
        }
        if (!file || file->get_path().empty())
            return;
        std::string path = file->get_path();
        run_task([this, world_name, path]() { ctx_.snapshots.export_snapshots(world_name, path); });
    });
}
