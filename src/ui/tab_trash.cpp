#include "ui/tab_trash.h"

#include <filesystem>

#include "core/fs.h"
#include "ui/dialogs.h"

static const char* const NO_INFO_TEXT = "Select an item to see where it came from and what restoring it does.";
static const size_t MAX_LISTED_NAMES = 10;  // In confirmations that list items.

static Gtk::Label* make_label(const Glib::ustring& text)
{
    Gtk::Label* label = Gtk::make_managed<Gtk::Label>(text);
    label->set_xalign(0.0f);
    return label;
}

static Glib::ustring kind_text(const std::string& kind)
{
    if (kind == "soundpack")
        return "Soundpack";
    if (kind == "mod")
        return "Mod";
    if (kind == "backup")
        return "Backup";
    if (kind == "install")
        return "Game install";
    if (kind == "saves")
        return "Saves";
    if (kind == "snapshots")
        return "Snapshots";
    return kind;
}

static Glib::ustring size_text(std::uint64_t bytes)
{
    return Glib::format_size(bytes, Glib::FormatSizeFlags::IEC_UNITS);  // The limit is in MiB.
}

// "Name (12 MB)" lines, at most MAX_LISTED_NAMES of them.
static Glib::ustring list_names(const std::vector<TrashItem>& items)
{
    Glib::ustring text;
    for (size_t i = 0; i < items.size() && i < MAX_LISTED_NAMES; ++i)
        text += "\n" + kind_text(items[i].kind) + ": " + items[i].name + " (" + size_text(items[i].size) + ")";
    if (items.size() > MAX_LISTED_NAMES)
        text += "\n... and " + std::to_string(items.size() - MAX_LISTED_NAMES) + " more";
    return text;
}

static std::uint64_t total_of(const std::vector<TrashItem>& items)
{
    std::uint64_t total = 0;
    for (size_t i = 0; i < items.size(); ++i)
        total += items[i].size;
    return total;
}

TabTrash::TabTrash(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8),
      ctx_(ctx),
      runner_(runner),
      parent_(parent),
      updating_(false),
      content_(Gtk::Orientation::VERTICAL, 8)
{
    build_ui();
}

void TabTrash::build_ui()
{
    set_margin(12);
    content_.set_vexpand(true);
    append(content_);

    summary_label_.set_wrap(true);
    summary_label_.set_xalign(0.0f);
    summary_label_.set_use_markup(true);
    content_.append(summary_label_);

    Gtk::ScrolledWindow* scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroller->set_vexpand(true);
    scroller->set_has_frame(true);
    scroller->set_child(list_);
    list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
    content_.append(*scroller);

    // Not homogeneous: five buttons the width of "Delete Permanently" make the window too wide.
    Gtk::Box* buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    restore_button_.set_label("Restore");
    restore_button_.set_tooltip_text("Put the selected item back where it came from.");
    delete_button_.set_label("Delete Permanently");
    delete_button_.set_tooltip_text("Delete the selected item for good.");
    empty_button_.set_label("Empty Trash");
    empty_button_.set_tooltip_text("Delete everything in the trash for good.");
    free_up_button_.set_label("Free Up Space");
    free_up_button_.set_tooltip_text("Delete the oldest items for good until the trash is under its size limit.");
    open_folder_button_.set_label("Open Folder");
    open_folder_button_.set_tooltip_text("Open the trash folder in your file manager.");
    buttons->append(restore_button_);
    buttons->append(delete_button_);
    buttons->append(empty_button_);
    buttons->append(free_up_button_);
    buttons->append(open_folder_button_);
    content_.append(*buttons);

    info_label_.set_wrap(true);
    info_label_.set_wrap_mode(Pango::WrapMode::WORD_CHAR);  // Paths have no spaces to break at.
    info_label_.set_xalign(0.0f);
    info_label_.set_yalign(0.0f);
    info_label_.set_selectable(true);
    Gtk::ScrolledWindow* info_scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    info_scroller->set_min_content_height(90);
    info_scroller->set_has_frame(true);
    info_scroller->set_child(info_label_);
    content_.append(*info_scroller);

    list_.signal_row_selected().connect([this](Gtk::ListBoxRow*) { on_selection_changed(); });
    restore_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabTrash::on_restore_clicked));
    delete_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabTrash::on_delete_clicked));
    empty_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabTrash::on_empty_clicked));
    free_up_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabTrash::on_free_up_clicked));
    open_folder_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabTrash::on_open_folder_clicked));
}

void TabTrash::refresh()
{
    items_ = ctx_.trash.list();

    updating_ = true;
    while (Gtk::ListBoxRow* row = list_.get_row_at_index(0))
        list_.remove(*row);
    for (size_t i = 0; i < items_.size(); ++i)
    {
        const TrashItem& item = items_[i];
        Gtk::Box* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
        Gtk::Label* kind = make_label(kind_text(item.kind));
        kind->set_width_chars(12);
        Gtk::Label* name = make_label(item.name);
        name->set_hexpand(true);
        name->set_ellipsize(Pango::EllipsizeMode::END);
        Gtk::Label* game = make_label(item.game);
        game->set_width_chars(5);
        Gtk::Label* when = make_label(item.deleted_at.substr(0, 16));  // To the minute.
        Gtk::Label* size = make_label(size_text(item.size));
        size->set_width_chars(9);
        size->set_xalign(1.0f);
        row->append(*kind);
        row->append(*name);
        row->append(*game);
        row->append(*when);
        row->append(*size);
        row->set_tooltip_text(to_utf8(item.original_path));
        list_.append(*row);
    }
    updating_ = false;

    info_label_.set_text(NO_INFO_TEXT);
    update_summary();
    update_buttons();
}

void TabTrash::update_summary()
{
    std::uint64_t total = total_of(items_);
    std::uint64_t limit = (std::uint64_t)std::max(0, ctx_.settings.get_int("trash_max_size_mb")) * 1024 * 1024;

    Glib::ustring text;
    if (items_.empty())
        text = "The trash is empty.";
    else
        text = std::to_string(items_.size()) + (items_.size() == 1 ? " item, " : " items, ") + size_text(total) + " of "
               + size_text(limit) + ".";
    text = Glib::Markup::escape_text(text);
    if (total > limit)
        text += " <b>Over the size limit:</b> use Free Up Space, or change the limit in Settings.";
    if (!ctx_.settings.get_bool("use_trash"))
        text += "\n<b>Moving deleted things to the trash is turned off</b> in Settings, so deleting is permanent.";
    summary_label_.set_markup(text);
}

int TabTrash::selected_item()
{
    Gtk::ListBoxRow* row = list_.get_selected_row();
    if (!row || row->get_index() < 0 || (size_t)row->get_index() >= items_.size())
        return -1;
    return row->get_index();
}

void TabTrash::update_buttons()
{
    bool have = (selected_item() >= 0);
    restore_button_.set_sensitive(have);
    delete_button_.set_sensitive(have);
    empty_button_.set_sensitive(!items_.empty());
    free_up_button_.set_sensitive(!ctx_.trash.over_limit().empty());
}

void TabTrash::on_selection_changed()
{
    if (updating_)
        return;
    update_buttons();

    int index = selected_item();
    if (index < 0)
    {
        info_label_.set_text(NO_INFO_TEXT);
        return;
    }
    const TrashItem& item = items_[index];
    RestorePlan plan = ctx_.trash.plan_restore(item.id);

    Glib::ustring restore;
    switch (plan.kind)
    {
    case RestorePlan::ORIGINAL:
        restore = "Goes back to " + to_utf8(plan.dest);
        break;
    case RestorePlan::RENAMED:
        if (item.kind == "saves")
            restore = "The worlds go back into " + to_utf8(plan.dest) + ". Worlds whose names are taken get \" (restored)\" added.";
        else
            restore = "Goes back as " + to_utf8(plan.dest) + ", as the old place is taken."
                      + (Trash::can_choose_name(item.kind) ? " Restoring asks for the name." : "");
        break;
    case RestorePlan::CONFLICT:
        restore = plan.reason + " Restoring asks whether to replace it.";
        break;
    case RestorePlan::IMPOSSIBLE:
        restore = "Can't be restored: " + plan.reason;
        break;
    }
    info_label_.set_text("Deleted: " + item.deleted_at.substr(0, 19) + "\nFrom: " + to_utf8(item.original_path)
                         + "\nRestore: " + restore);
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is done.
void TabTrash::run_task(const std::function<void()>& work)
{
    content_.set_sensitive(false);
    bool started = runner_.run(work, [this]() {
        content_.set_sensitive(true);
        refresh();
    });
    if (!started)
        content_.set_sensitive(true);
}

void TabTrash::on_restore_clicked()
{
    int index = selected_item();
    if (index < 0)
        return;
    TrashItem item = items_[index];
    std::string id = item.id;
    RestorePlan plan = ctx_.trash.plan_restore(id);

    switch (plan.kind)
    {
    case RestorePlan::ORIGINAL:
        run_task([this, id]() { ctx_.trash.restore(id, false); });
        break;
    case RestorePlan::RENAMED:
    {
        if (Trash::can_choose_name(item.kind))
        {
            // The free name it would get is the default; the user can pick another.
            std::string parent = std::filesystem::path(plan.dest).parent_path().string();
            std::string suggested = std::filesystem::path(plan.dest).filename().string();
            dialogs::ask_text(
                parent_, "Restore " + item.name + "?",
                "Its old place is taken, so it goes back under another name in\n" + parent, suggested, "Restore",
                [this, id](const std::string& name) { return ctx_.trash.check_restore_name(id, name); },
                [this, id](const std::string& name) {
                    run_task([this, id, name]() { ctx_.trash.restore(id, false, name); });
                });
            break;
        }
        Glib::ustring detail =
            (item.kind == "saves")
                ? "The worlds go back into your current saves. Worlds whose names are taken get \" (restored)\" "
                  "added, so nothing is overwritten."
                : "Its old place is taken, so it goes back as\n" + to_utf8(plan.dest);
        dialogs::confirm(parent_, "Restore " + item.name + "?", detail, "Restore",
                         [this, id]() { run_task([this, id]() { ctx_.trash.restore(id, false); }); });
        break;
    }
    case RestorePlan::CONFLICT:
    {
        bool use_trash = ctx_.settings.get_bool("use_trash");
        Glib::ustring detail = plan.reason + "\n\nReplacing it: " + plan.conflict_name + " "
                               + dialogs::removal_text(use_trash) + ", then " + item.name + " is restored.";
        dialogs::confirm(parent_, "Replace " + plan.conflict_name + "?", detail, "Replace",
                         [this, id]() { run_task([this, id]() { ctx_.trash.restore(id, true); }); });
        break;
    }
    case RestorePlan::IMPOSSIBLE:
        dialogs::tell(parent_, "Can't restore " + item.name, plan.reason);
        break;
    }
}

void TabTrash::on_delete_clicked()
{
    int index = selected_item();
    if (index < 0)
        return;
    TrashItem item = items_[index];
    std::vector<std::string> ids(1, item.id);
    dialogs::confirm(parent_, "Delete " + item.name + " for good?",
                     kind_text(item.kind) + ": " + item.name + " (" + size_text(item.size)
                         + ")\n\nThis can't be undone.",
                     "Delete Permanently", [this, ids]() { run_task([this, ids]() { ctx_.trash.delete_items(ids); }); });
}

void TabTrash::on_empty_clicked()
{
    if (items_.empty())
        return;
    dialogs::confirm(parent_, "Empty the trash?",
                     "All " + std::to_string(items_.size()) + " items (" + size_text(total_of(items_))
                         + ") will be deleted for good. This can't be undone.",
                     "Empty Trash", [this]() { run_task([this]() { ctx_.trash.empty(); }); });
}

void TabTrash::on_free_up_clicked()
{
    ask_free_up();
}

void TabTrash::ask_free_up()
{
    std::vector<TrashItem> extra = ctx_.trash.over_limit();
    if (extra.empty())
    {
        ctx_.status.post("The trash is not over its size limit.");
        return;
    }
    std::vector<std::string> ids;
    for (size_t i = 0; i < extra.size(); ++i)
        ids.push_back(extra[i].id);
    dialogs::confirm(parent_, "Delete the oldest items for good?",
                     "To get the trash under its size limit, these " + std::to_string(extra.size()) + " items ("
                         + size_text(total_of(extra)) + ") will be deleted for good:\n" + list_names(extra)
                         + "\n\nThis can't be undone.",
                     "Delete Permanently", [this, ids]() { run_task([this, ids]() { ctx_.trash.delete_items(ids); }); });
}

void TabTrash::on_open_folder_clicked()
{
    std::error_code ec;
    if (!std::filesystem::is_directory(ctx_.paths.trash_dir(), ec))
    {
        ctx_.status.post("There is no trash folder yet: nothing has been moved to the trash.");
        return;
    }
    dialogs::open_folder(parent_, ctx_.paths.trash_dir());
}
