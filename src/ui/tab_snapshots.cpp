#include "ui/tab_snapshots.h"

#include <QAction>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <ctime>
#include <memory>

#include "core/fs.h"
#include "core/git.h"
#include "core/platform.h"
#include "ui/dialogs.h"
#include "ui/qt_helpers.h"

static const char* const NO_DETAILS_TEXT = "Select a snapshot to see its details here.";
static const char* const BUNDLE_FILTER = "Git bundles (.bundle) (*.bundle)";

static QString format_time(std::int64_t time, const char* format)
{
    return QDateTime::fromSecsSinceEpoch(time).toString(format);
}

static QString state_text(const WorldInfo& world)
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

// A list with a text column that takes the room and an other one that fits its content.
static QTreeWidget* make_two_column_list(int stretch_column)
{
    QTreeWidget* list = new QTreeWidget();
    list->setColumnCount(2);
    list->setHeaderHidden(true);
    list->setRootIsDecorated(false);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->header()->setStretchLastSection(false);
    list->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    list->header()->setSectionResizeMode(stretch_column, QHeaderView::Stretch);
    return list;
}

static QFrame* make_separator()
{
    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    return separator;
}

TabSnapshots::TabSnapshots(AppContext& ctx, TaskRunner& runner, QWidget& parent)
    : ctx_(ctx), runner_(runner), parent_(parent), updating_(false)
{
    build_ui();
}

void TabSnapshots::build_ui()
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    hint_label_ = make_label("");
    hint_label_->setWordWrap(true);
    hint_label_->setAlignment(Qt::AlignLeft | Qt::AlignTop);  // Not in the middle of the empty tab.
    outer->addWidget(hint_label_);
    content_ = new QWidget();
    outer->addWidget(content_, 1);

    QVBoxLayout* content = new QVBoxLayout(content_);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(8);

    // Draggable, so long world names can be shown in full.
    QSplitter* columns = new QSplitter(Qt::Horizontal);
    columns->setChildrenCollapsible(false);
    content->addWidget(columns, 1);

    // Left: the worlds.
    QWidget* left = new QWidget();
    QVBoxLayout* left_box = new QVBoxLayout(left);
    left_box->setContentsMargins(0, 0, 6, 0);
    left_box->setSpacing(6);
    left_box->addWidget(make_label("Worlds"));
    worlds_list_ = make_two_column_list(0);
    worlds_list_->setMinimumWidth(220);
    left_box->addWidget(worlds_list_, 1);
    columns->addWidget(left);

    // Right: the snapshots of the selected world.
    QWidget* right = new QWidget();
    QVBoxLayout* right_box = new QVBoxLayout(right);
    right_box->setContentsMargins(6, 0, 0, 0);
    right_box->setSpacing(6);
    snapshots_title_ = make_label("");
    snapshots_title_->setMinimumWidth(1);  // Long world names don't make the column wider.
    // Rarely needed actions on the selected world's snapshots.
    QMenu* menu = new QMenu(this);
    menu->setToolTipsVisible(true);
    export_action_ = menu->addAction("Export snapshots\xe2\x80\xa6");
    export_action_->setToolTip("Save every snapshot of this world to one file (a git bundle), to keep elsewhere or share.");
    import_action_ = menu->addAction("Import snapshots\xe2\x80\xa6");
    import_action_->setToolTip("Make a new world from an exported snapshots file. Existing worlds are never replaced.");
    start_fresh_action_ = menu->addAction("Start fresh\xe2\x80\xa6");
    start_fresh_action_->setToolTip("Move all snapshots of this world to the trash and start a new history with "
                                    "one snapshot (Name and Description below). Frees disk space.");
    delete_action_ = menu->addAction("Delete snapshots\xe2\x80\xa6");
    delete_action_->setToolTip("For a world that no longer exists: remove its snapshots too.");
    QToolButton* menu_button = new QToolButton();
    set_icon(menu_button, "view-more-symbolic", "\xe2\x80\xa6");
    menu_button->setToolTip("More actions");
    menu_button->setPopupMode(QToolButton::InstantPopup);
    menu_button->setMenu(menu);
    QHBoxLayout* title_row = new QHBoxLayout();
    title_row->setSpacing(6);
    title_row->addWidget(snapshots_title_, 1);
    title_row->addWidget(menu_button);
    right_box->addLayout(title_row);
    snapshots_list_ = make_two_column_list(1);
    right_box->addWidget(snapshots_list_, 1);

    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->setSpacing(6);
    restore_button_ = new QPushButton("Restore snapshot");
    restore_button_->setToolTip("Make the world match the selected snapshot. Changes not in a snapshot are kept as a snapshot first.");
    discard_button_ = new QPushButton("Discard changes");
    discard_button_->setToolTip("Throw away the changes since the last snapshot.");
    buttons->addWidget(restore_button_, 1);
    buttons->addWidget(discard_button_, 1);
    right_box->addLayout(buttons);

    details_view_ = new QTextBrowser();
    details_view_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    details_view_->setFixedHeight(90);
    right_box->addWidget(details_view_);
    columns->addWidget(right);
    columns->setStretchFactor(0, 0);
    columns->setStretchFactor(1, 1);
    columns->setSizes(QList<int>() << 340 << 400);

    // Taking snapshots.
    content->addWidget(make_separator());
    QGridLayout* form = new QGridLayout();
    form->setVerticalSpacing(6);
    form->setHorizontalSpacing(6);
    form->addWidget(make_label("Name:"), 0, 0);
    name_entry_ = new QLineEdit();
    form->addWidget(name_entry_, 0, 1);
    form->addWidget(make_label("Description:"), 1, 0);
    description_entry_ = new QLineEdit();
    description_entry_->setPlaceholderText("Optional");
    form->addWidget(description_entry_, 1, 1);
    QHBoxLayout* take_row = new QHBoxLayout();
    take_row->setSpacing(6);
    take_button_ = new QPushButton("Take snapshot");
    take_button_->setToolTip("Take a snapshot of the selected world.");
    take_all_button_ = new QPushButton("Take snapshot of all worlds");
    take_all_button_->setToolTip("Take a snapshot of every world that has changes.");
    take_row->addWidget(take_button_);
    take_row->addWidget(take_all_button_);
    take_row->addStretch(1);
    form->addLayout(take_row, 2, 1);
    content->addLayout(form);

    // Footer: size and the command to use the repository from a terminal.
    content->addWidget(make_separator());
    QHBoxLayout* footer = new QHBoxLayout();
    footer->setSpacing(12);
    footer_label_ = make_label("");
    command_entry_ = new QLineEdit();
    command_entry_->setReadOnly(true);
    command_entry_->setFrame(false);
    command_entry_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    // Looks like a label (selectable, and it scrolls instead of making the window wider).
    QPalette label_like = command_entry_->palette();
    label_like.setColor(QPalette::Base, label_like.color(QPalette::Window));
    command_entry_->setPalette(label_like);
    copy_button_ = new QPushButton("Copy");
    copy_button_->setToolTip("Copy the git command for this world, to use its snapshots from a terminal.");
    footer->addWidget(footer_label_);
    footer->addWidget(command_entry_, 1);
    footer->addWidget(copy_button_);
    content->addLayout(footer);

    connect(worlds_list_, &QTreeWidget::itemSelectionChanged, this, &TabSnapshots::on_world_selected);
    connect(snapshots_list_, &QTreeWidget::itemSelectionChanged, this, &TabSnapshots::on_snapshot_selected);
    connect(name_entry_, &QLineEdit::textChanged, this, &TabSnapshots::update_buttons);
    connect(name_entry_, &QLineEdit::returnPressed, this, &TabSnapshots::on_take_clicked);
    connect(description_entry_, &QLineEdit::returnPressed, this, &TabSnapshots::on_take_clicked);
    connect(take_button_, &QPushButton::clicked, this, &TabSnapshots::on_take_clicked);
    connect(take_all_button_, &QPushButton::clicked, this, &TabSnapshots::on_take_all_clicked);
    connect(restore_button_, &QPushButton::clicked, this, &TabSnapshots::on_restore_clicked);
    connect(discard_button_, &QPushButton::clicked, this, &TabSnapshots::on_discard_clicked);
    connect(copy_button_, &QPushButton::clicked, this, &TabSnapshots::on_copy_clicked);
    connect(export_action_, &QAction::triggered, this, &TabSnapshots::on_export_clicked);
    connect(import_action_, &QAction::triggered, this, &TabSnapshots::on_import_clicked);
    connect(start_fresh_action_, &QAction::triggered, this, &TabSnapshots::on_start_fresh_clicked);
    connect(delete_action_, &QAction::triggered, this, &TabSnapshots::on_delete_clicked);
}

void TabSnapshots::show_unavailable(const QString& message)
{
    hint_label_->setText(message);
    hint_label_->setVisible(true);
    content_->setVisible(false);
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
    hint_label_->setVisible(false);
    content_->setVisible(true);

    std::tm local = platform::local_time(std::time(nullptr));
    name_entry_->setText(qstr(SnapshotManager::default_snapshot_name(local)));
    description_entry_->setText("");

    reload();
}

// git status for every world can take a while, so it runs on the worker thread.
void TabSnapshots::reload()
{
    std::shared_ptr<std::vector<WorldInfo> > loaded = std::make_shared<std::vector<WorldInfo> >();
    content_->setEnabled(false);
    bool started = runner_.run([this, loaded]() { *loaded = ctx_.snapshots.list_worlds(); },
                               [this, loaded]() {
                                   content_->setEnabled(true);
                                   fill_worlds(*loaded);
                                   ask_about_deleted_worlds();
                               });
    if (!started)
        content_->setEnabled(true);
}

void TabSnapshots::fill_worlds(const std::vector<WorldInfo>& worlds)
{
    worlds_ = worlds;

    updating_ = true;
    worlds_list_->clear();
    QBrush dim = dim_brush(worlds_list_);
    int reselect = -1;
    for (size_t i = 0; i < worlds_.size(); ++i)
    {
        const WorldInfo& world = worlds_[i];
        QTreeWidgetItem* row = new QTreeWidgetItem(worlds_list_);
        row->setText(0, qstr(world.name));
        row->setText(1, state_text(world));
        row->setForeground(1, dim);
        if (world.state == WorldInfo::DELETED)
            row->setForeground(0, dim);
        row->setToolTip(0, qstr(world.name));
        if (world.name == selected_name_)
            reselect = (int)i;
    }
    if (reselect < 0 && !worlds_.empty())
        reselect = 0;
    if (reselect >= 0)
        worlds_list_->setCurrentItem(worlds_list_->topLevelItem(reselect));
    updating_ = false;

    on_world_selected();
}

const WorldInfo* TabSnapshots::selected_world()
{
    int row = selected_row(worlds_list_);
    if (row < 0 || (size_t)row >= worlds_.size())
        return nullptr;
    return &worlds_[row];
}

const Snapshot* TabSnapshots::selected_snapshot()
{
    int row = selected_row(snapshots_list_);
    if (row < 0 || (size_t)row >= snapshots_.size())
        return nullptr;
    return &snapshots_[row];
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
    snapshots_list_->clear();
    QBrush dim = dim_brush(snapshots_list_);
    for (size_t i = 0; i < snapshots_.size(); ++i)
    {
        QTreeWidgetItem* row = new QTreeWidgetItem(snapshots_list_);
        row->setText(0, format_time(snapshots_[i].time, "MMM dd HH:mm"));
        row->setForeground(0, dim);
        row->setText(1, qstr(snapshots_[i].name));
    }
    updating_ = false;

    if (!world)
    {
        snapshots_title_->setText(worlds_.empty() ? "There are no worlds yet." : "Select a world.");
        footer_label_->setText("");
        command_entry_->setText("");
    }
    else
    {
        snapshots_title_->setText(qstr("Snapshots of " + world->name));
        if (snapshots_.empty())
            footer_label_->setText("No snapshots yet");
        else
            footer_label_->setText(qstr(std::to_string(snapshots_.size()) + (snapshots_.size() == 1 ? " snapshot, " : " snapshots, "))
                                   + format_size(ctx_.snapshots.history_size(world->name)));
        command_entry_->setText(qstr(ctx_.snapshots.git_command(world->name)));
        command_entry_->setCursorPosition(0);
        command_entry_->setToolTip(command_entry_->text());
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
        details_view_->setPlainText(NO_DETAILS_TEXT);
        return;
    }
    int files = ctx_.snapshots.changed_files(world->name, snap->hash);
    QString line = format_time(snap->time, "MMM dd yyyy, HH:mm");
    if (files >= 0)
        line += qstr(" \xc2\xb7 " + std::to_string(files) + (files == 1 ? " file changed" : " files changed"));
    QString text = "<b>" + escape(snap->name) + "</b><br>" + line.toHtmlEscaped();
    if (!snap->description.empty())
        text += "<br><br>" + escape(snap->description);
    details_view_->setHtml(text);
}

void TabSnapshots::update_buttons()
{
    const WorldInfo* world = selected_world();
    bool deleted = world && world->state == WorldInfo::DELETED;
    bool has_name = !name_entry_->text().isEmpty();
    bool any_world = false;
    for (size_t i = 0; i < worlds_.size(); ++i)
        any_world = any_world || worlds_[i].state != WorldInfo::DELETED;

    restore_button_->setEnabled(world && selected_snapshot());
    discard_button_->setEnabled(world && world->state == WorldInfo::CHANGED);
    take_button_->setEnabled(world && !deleted && has_name);
    take_all_button_->setEnabled(any_world && has_name);
    copy_button_->setEnabled(world != nullptr);
    export_action_->setEnabled(world && !snapshots_.empty());
    start_fresh_action_->setEnabled(world && !deleted && !snapshots_.empty() && has_name);
    delete_action_->setEnabled(deleted);
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
        QStringList buttons;
        buttons << "Cancel" << qstr(dialogs::removal_button(use_trash));
        dialogs::ask(parent_, qstr(name + " no longer exists"),
                     qstr("Delete its snapshots too? They " + dialogs::removal_text(use_trash)
                          + ".\n\nCancel keeps them: the world stays in the list as deleted, and restoring one of its "
                            "snapshots brings it back."),
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
    content_->setEnabled(false);
    bool started = runner_.run(work, [this]() {
        content_->setEnabled(true);
        reload();
    });
    if (!started)
        content_->setEnabled(true);
}

// Says so in a dialog, instead of failing with a git error.
bool TabSnapshots::check_writable()
{
    std::string reason;
    if (ctx_.snapshots.can_write(reason))
        return true;
    dialogs::tell(parent_, "Can't change the saves", qstr(reason));
    return false;
}

void TabSnapshots::on_take_clicked()
{
    const WorldInfo* world = selected_world();
    std::string name = str(name_entry_->text());
    if (!world || world->state == WorldInfo::DELETED || name.empty() || !check_writable())
        return;
    std::string world_name = world->name;
    std::string description = str(description_entry_->text());
    run_task([this, world_name, name, description]() { ctx_.snapshots.take_snapshot(world_name, name, description); });
}

void TabSnapshots::on_take_all_clicked()
{
    std::string name = str(name_entry_->text());
    if (name.empty() || !check_writable())
        return;
    std::string description = str(description_entry_->text());
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
    dialogs::confirm(parent_, "Restore this snapshot?", qstr(detail + " Close the game first."), "Restore",
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
                     qstr("The changes to " + world_name + " since the last snapshot" + last
                          + " will be thrown away. They are not in any snapshot, so they can't be brought back."),
                     "Discard", [this, world_name]() {
                         run_task([this, world_name]() { ctx_.snapshots.discard_changes(world_name); });
                     });
}

void TabSnapshots::on_copy_clicked()
{
    const WorldInfo* world = selected_world();
    if (!world)
        return;
    QGuiApplication::clipboard()->setText(qstr(ctx_.snapshots.git_command(world->name)));
    ctx_.status.post("Copied the git command for " + world->name + ".");
}

void TabSnapshots::on_export_clicked()
{
    const WorldInfo* world = selected_world();
    if (!world || snapshots_.empty())
        return;
    std::string world_name = world->name;
    // The file dialog asks before replacing an existing file.
    QString file = QFileDialog::getSaveFileName(&parent_, qstr("Export the snapshots of " + world_name),
                                                qstr(world_name + ".bundle"), BUNDLE_FILTER);
    if (file.isEmpty())
        return;
    std::filesystem::path path = to_path(str(file));
    run_task([this, world_name, path]() { ctx_.snapshots.export_snapshots(world_name, path); });
}

void TabSnapshots::on_import_clicked()
{
    QString file = QFileDialog::getOpenFileName(&parent_, "Import snapshots", QString(), BUNDLE_FILTER);
    if (file.isEmpty())
        return;
    std::filesystem::path path = to_path(str(file));
    std::string world = SnapshotManager::bundle_world_name(path);
    std::string problem = ctx_.snapshots.check_import_name(world);
    if (problem.empty())
    {
        import_bundle(path, world);
        return;
    }
    // Never replaces a world: ask for another name instead.
    dialogs::ask_text(
        parent_, "Name the imported world", qstr(problem + " Choose a name for the world made from this file."),
        qstr(world), "Import", [this](const std::string& name) { return ctx_.snapshots.check_import_name(name); },
        [this, path](const std::string& name) { import_bundle(path, name); });
}

void TabSnapshots::import_bundle(const std::filesystem::path& path, const std::string& world)
{
    if (!check_writable())
        return;
    selected_name_ = world;  // Shown once the reload is done.
    run_task([this, path, world]() { ctx_.snapshots.import_snapshots(path, world); });
}

void TabSnapshots::on_start_fresh_clicked()
{
    const WorldInfo* world = selected_world();
    std::string name = str(name_entry_->text());
    if (!world || world->state == WorldInfo::DELETED || snapshots_.empty() || name.empty() || !check_writable())
        return;
    std::string world_name = world->name;
    std::string description = str(description_entry_->text());
    std::string count = std::to_string(snapshots_.size()) + (snapshots_.size() == 1 ? " snapshot" : " snapshots");
    bool use_trash = ctx_.settings.get_bool("use_trash");
    dialogs::confirm(parent_, qstr("Start fresh with " + world_name + "?"),
                     qstr("Its " + count + " (" + str(format_size(ctx_.snapshots.history_size(world_name))) + ") "
                          + dialogs::removal_text(use_trash) + ". A new history starts with one snapshot of the world "
                          "as it is now, \"" + name + "\". The world itself is not changed."),
                     "Start Fresh", [this, world_name, name, description]() {
                         run_task([this, world_name, name, description]() {
                             ctx_.snapshots.start_fresh(world_name, name, description);
                         });
                     });
}

void TabSnapshots::on_delete_clicked()
{
    const WorldInfo* world = selected_world();
    if (!world || world->state != WorldInfo::DELETED)
        return;
    std::string world_name = world->name;
    std::string count = std::to_string(snapshots_.size()) + (snapshots_.size() == 1 ? " snapshot" : " snapshots");
    bool use_trash = ctx_.settings.get_bool("use_trash");
    dialogs::confirm(parent_, qstr("Delete the snapshots of " + world_name + "?"),
                     qstr(world_name + " no longer exists. Its " + count + " ("
                          + str(format_size(ctx_.snapshots.history_size(world_name))) + ") "
                          + dialogs::removal_text(use_trash) + "."),
                     qstr(dialogs::removal_button(use_trash)), [this, world_name]() {
                         run_task([this, world_name]() { ctx_.snapshots.delete_snapshots(world_name); });
                     });
}
