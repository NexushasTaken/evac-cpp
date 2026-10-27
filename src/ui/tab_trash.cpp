#include "ui/tab_trash.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>

#include "core/fs.h"
#include "ui/dialogs.h"
#include "ui/qt_helpers.h"

static const char* const NO_INFO_TEXT = "Select an item to see where it came from and what restoring it does.";
static const size_t MAX_LISTED_NAMES = 10;  // In confirmations that list items.

// The columns of the list.
enum
{
    COL_KIND,
    COL_NAME,
    COL_GAME,
    COL_WHEN,
    COL_SIZE,
    NUM_COLUMNS
};

static std::string kind_text(const std::string& kind)
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

static std::string size_text(std::uint64_t bytes)
{
    return str(format_size(bytes, true));  // The limit is in MiB.
}

// "Name (12 MB)" lines, at most MAX_LISTED_NAMES of them.
static std::string list_names(const std::vector<TrashItem>& items)
{
    std::string text;
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

TabTrash::TabTrash(AppContext& ctx, TaskRunner& runner, QWidget& parent)
    : ctx_(ctx), runner_(runner), parent_(parent), updating_(false)
{
    build_ui();
}

void TabTrash::build_ui()
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    content_ = new QWidget();
    outer->addWidget(content_, 1);

    QVBoxLayout* content = new QVBoxLayout(content_);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(8);
    summary_label_ = new QLabel();
    summary_label_->setTextFormat(Qt::RichText);
    summary_label_->setWordWrap(true);
    content->addWidget(summary_label_);

    list_ = new QTreeWidget();
    list_->setColumnCount(NUM_COLUMNS);
    list_->setHeaderHidden(true);
    list_->setRootIsDecorated(false);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->header()->setStretchLastSection(false);
    list_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    list_->header()->setSectionResizeMode(COL_NAME, QHeaderView::Stretch);
    content->addWidget(list_, 1);

    // Not equal widths: five buttons the width of "Delete Permanently" make the window too wide.
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->setSpacing(6);
    restore_button_ = new QPushButton("Restore");
    restore_button_->setToolTip("Put the selected item back where it came from.");
    delete_button_ = new QPushButton("Delete Permanently");
    delete_button_->setToolTip("Delete the selected item for good.");
    empty_button_ = new QPushButton("Empty Trash");
    empty_button_->setToolTip("Delete everything in the trash for good.");
    free_up_button_ = new QPushButton("Free Up Space");
    free_up_button_->setToolTip("Delete the oldest items for good until the trash is under its size limit.");
    open_folder_button_ = new QPushButton("Open Folder");
    open_folder_button_->setToolTip("Open the trash folder in your file manager.");
    buttons->addWidget(restore_button_);
    buttons->addWidget(delete_button_);
    buttons->addWidget(empty_button_);
    buttons->addWidget(free_up_button_);
    buttons->addWidget(open_folder_button_);
    buttons->addStretch(1);
    content->addLayout(buttons);

    info_view_ = new QTextBrowser();
    info_view_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);  // Paths have no spaces to break at.
    info_view_->setFixedHeight(90);
    content->addWidget(info_view_);

    connect(list_, &QTreeWidget::itemSelectionChanged, this, &TabTrash::on_selection_changed);
    connect(restore_button_, &QPushButton::clicked, this, &TabTrash::on_restore_clicked);
    connect(delete_button_, &QPushButton::clicked, this, &TabTrash::on_delete_clicked);
    connect(empty_button_, &QPushButton::clicked, this, &TabTrash::on_empty_clicked);
    connect(free_up_button_, &QPushButton::clicked, this, &TabTrash::on_free_up_clicked);
    connect(open_folder_button_, &QPushButton::clicked, this, &TabTrash::on_open_folder_clicked);
}

void TabTrash::refresh()
{
    items_ = ctx_.trash.list();

    updating_ = true;
    list_->clear();
    for (size_t i = 0; i < items_.size(); ++i)
    {
        const TrashItem& item = items_[i];
        QTreeWidgetItem* row = new QTreeWidgetItem(list_);
        row->setText(COL_KIND, qstr(kind_text(item.kind)));
        row->setText(COL_NAME, qstr(item.name));
        row->setText(COL_GAME, qstr(item.game));
        row->setText(COL_WHEN, qstr(item.deleted_at.substr(0, 16)));  // To the minute.
        row->setText(COL_SIZE, qstr(size_text(item.size)));
        row->setTextAlignment(COL_SIZE, Qt::AlignRight | Qt::AlignVCenter);
        for (int col = 0; col < NUM_COLUMNS; ++col)
            row->setToolTip(col, qstr(item.original_path));
    }
    updating_ = false;

    info_view_->setPlainText(NO_INFO_TEXT);
    update_summary();
    update_buttons();
}

void TabTrash::update_summary()
{
    std::uint64_t total = total_of(items_);
    std::uint64_t limit = (std::uint64_t)std::max(0, ctx_.settings.get_int("trash_max_size_mb")) * 1024 * 1024;

    std::string plain;
    if (items_.empty())
        plain = "The trash is empty.";
    else
        plain = std::to_string(items_.size()) + (items_.size() == 1 ? " item, " : " items, ") + size_text(total) + " of "
                + size_text(limit) + ".";
    QString text = escape(plain);
    if (total > limit)
        text += " <b>Over the size limit:</b> use Free Up Space, or change the limit in Settings.";
    if (!ctx_.settings.get_bool("use_trash"))
        text += "<br><b>Moving deleted things to the trash is turned off</b> in Settings, so deleting is permanent.";
    summary_label_->setText(text);
}

int TabTrash::selected_item()
{
    int row = selected_row(list_);
    if (row < 0 || (size_t)row >= items_.size())
        return -1;
    return row;
}

void TabTrash::update_buttons()
{
    bool have = (selected_item() >= 0);
    restore_button_->setEnabled(have);
    delete_button_->setEnabled(have);
    empty_button_->setEnabled(!items_.empty());
    free_up_button_->setEnabled(!ctx_.trash.over_limit().empty());
}

void TabTrash::on_selection_changed()
{
    if (updating_)
        return;
    update_buttons();

    int index = selected_item();
    if (index < 0)
    {
        info_view_->setPlainText(NO_INFO_TEXT);
        return;
    }
    const TrashItem& item = items_[index];
    RestorePlan plan = ctx_.trash.plan_restore(item.id);

    std::string restore;
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
    info_view_->setPlainText(qstr("Deleted: " + item.deleted_at.substr(0, 19) + "\nFrom: " + to_utf8(item.original_path)
                                  + "\nRestore: " + restore));
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is done.
void TabTrash::run_task(const std::function<void()>& work)
{
    content_->setEnabled(false);
    bool started = runner_.run(work, [this]() {
        content_->setEnabled(true);
        refresh();
    });
    if (!started)
        content_->setEnabled(true);
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
            std::string parent = to_utf8(plan.dest.parent_path());
            std::string suggested = to_utf8(plan.dest.filename());
            dialogs::ask_text(
                parent_, qstr("Restore " + item.name + "?"),
                qstr("Its old place is taken, so it goes back under another name in\n" + parent), qstr(suggested),
                "Restore", [this, id](const std::string& name) { return ctx_.trash.check_restore_name(id, name); },
                [this, id](const std::string& name) {
                    run_task([this, id, name]() { ctx_.trash.restore(id, false, name); });
                });
            break;
        }
        std::string detail = (item.kind == "saves")
                                 ? "The worlds go back into your current saves. Worlds whose names are taken get \" (restored)\" "
                                   "added, so nothing is overwritten."
                                 : "Its old place is taken, so it goes back as\n" + to_utf8(plan.dest);
        dialogs::confirm(parent_, qstr("Restore " + item.name + "?"), qstr(detail), "Restore",
                         [this, id]() { run_task([this, id]() { ctx_.trash.restore(id, false); }); });
        break;
    }
    case RestorePlan::CONFLICT:
    {
        bool use_trash = ctx_.settings.get_bool("use_trash");
        std::string detail = plan.reason + "\n\nReplacing it: " + plan.conflict_name + " "
                             + dialogs::removal_text(use_trash) + ", then " + item.name + " is restored.";
        dialogs::confirm(parent_, qstr("Replace " + plan.conflict_name + "?"), qstr(detail), "Replace",
                         [this, id]() { run_task([this, id]() { ctx_.trash.restore(id, true); }); });
        break;
    }
    case RestorePlan::IMPOSSIBLE:
        dialogs::tell(parent_, qstr("Can't restore " + item.name), qstr(plan.reason));
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
    dialogs::confirm(parent_, qstr("Delete " + item.name + " for good?"),
                     qstr(kind_text(item.kind) + ": " + item.name + " (" + size_text(item.size)
                          + ")\n\nThis can't be undone."),
                     "Delete Permanently", [this, ids]() { run_task([this, ids]() { ctx_.trash.delete_items(ids); }); });
}

void TabTrash::on_empty_clicked()
{
    if (items_.empty())
        return;
    dialogs::confirm(parent_, "Empty the trash?",
                     qstr("All " + std::to_string(items_.size()) + " items (" + size_text(total_of(items_))
                          + ") will be deleted for good. This can't be undone."),
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
                     qstr("To get the trash under its size limit, these " + std::to_string(extra.size()) + " items ("
                          + size_text(total_of(extra)) + ") will be deleted for good:\n" + list_names(extra)
                          + "\n\nThis can't be undone."),
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
    dialogs::open_folder(ctx_.paths.trash_dir());
}
