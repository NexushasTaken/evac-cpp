#include "ui/tab_snapshots.h"

#include <qbrush.h>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qtextoption.h>

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
#include <cstdint>
#include <ctime>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/fs.h"
#include "core/git.h"
#include "core/paths.h"
#include "core/platform.h"
#include "core/snapshot_manager.h"
#include "ui/app_context.h"
#include "ui/dialogs.h"
#include "ui/qt_helpers.h"
#include "ui/task_runner.h"

static const char* const NO_DETAILS_TEXT =
    "Select a snapshot to see its details here.";
static const char* const BUNDLE_FILTER = "Git bundles (.bundle) (*.bundle)";

static QString format_time(std::int64_t time, const char* format) {
  return QDateTime::fromSecsSinceEpoch(time).toString(format);
}

static QString state_text(const WorldInfo& world) {
  switch (world.state) {
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

// A list with a text column that takes the room and an other one that fits its
// content.
static QTreeWidget* make_two_column_list(int stretch_column) {
  auto* list = new QTreeWidget();
  list->setColumnCount(2);
  list->setHeaderHidden(true);
  list->setRootIsDecorated(false);
  list->setSelectionMode(QAbstractItemView::SingleSelection);
  list->header()->setStretchLastSection(false);
  list->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  list->header()->setSectionResizeMode(stretch_column, QHeaderView::Stretch);
  return list;
}

static QFrame* make_separator() {
  auto* separator = new QFrame();
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  return separator;
}

TabSnapshots::TabSnapshots(AppContext& ctx, TaskRunner& runner, QWidget& parent)
    : m_ctx(ctx), m_runner(runner), m_parent(parent) {
  build_ui();
}

void TabSnapshots::build_ui() {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(12, 12, 12, 12);
  m_hint_label = make_label("");
  m_hint_label->setWordWrap(true);
  m_hint_label->setAlignment(
      Qt::AlignLeft | Qt::AlignTop);  // Not in the middle of the empty tab.
  outer->addWidget(m_hint_label);
  m_content = new QWidget();
  outer->addWidget(m_content, 1);

  auto* content = new QVBoxLayout(m_content);
  content->setContentsMargins(0, 0, 0, 0);
  content->setSpacing(8);

  // Draggable, so long world names can be shown in full.
  auto* columns = new QSplitter(Qt::Horizontal);
  columns->setChildrenCollapsible(false);
  content->addWidget(columns, 1);

  // Left: the worlds.
  auto* left = new QWidget();
  auto* left_box = new QVBoxLayout(left);
  left_box->setContentsMargins(0, 0, 6, 0);
  left_box->setSpacing(6);
  left_box->addWidget(make_label("Worlds"));
  m_worlds_list = make_two_column_list(0);
  m_worlds_list->setMinimumWidth(220);
  left_box->addWidget(m_worlds_list, 1);
  columns->addWidget(left);

  // Right: the snapshots of the selected world.
  auto* right = new QWidget();
  auto* right_box = new QVBoxLayout(right);
  right_box->setContentsMargins(6, 0, 0, 0);
  right_box->setSpacing(6);
  m_snapshots_title = make_label("");
  m_snapshots_title->setMinimumWidth(
      1);  // Long world names don't make the column wider.
  // Rarely needed actions on the selected world's snapshots.
  auto* menu = new QMenu(this);
  menu->setToolTipsVisible(true);
  m_export_action = menu->addAction("Export snapshots\xe2\x80\xa6");
  m_export_action->setToolTip(
      "Save every snapshot of this world to one file (a git bundle), to keep "
      "elsewhere or share.");
  m_import_action = menu->addAction("Import snapshots\xe2\x80\xa6");
  m_import_action->setToolTip(
      "Make a new world from an exported snapshots file. Existing worlds are "
      "never replaced.");
  m_start_fresh_action = menu->addAction("Start fresh\xe2\x80\xa6");
  m_start_fresh_action->setToolTip(
      "Move all snapshots of this world to the trash and start a new history "
      "with "
      "one snapshot (Name and Description below). Frees disk space.");
  m_delete_action = menu->addAction("Delete snapshots\xe2\x80\xa6");
  m_delete_action->setToolTip(
      "For a world that no longer exists: remove its snapshots too.");
  auto* menu_button = new QToolButton();
  set_icon(menu_button, "view-more-symbolic", "\xe2\x80\xa6");
  menu_button->setToolTip("More actions");
  menu_button->setPopupMode(QToolButton::InstantPopup);
  menu_button->setMenu(menu);
  auto* title_row = new QHBoxLayout();
  title_row->setSpacing(6);
  title_row->addWidget(m_snapshots_title, 1);
  title_row->addWidget(menu_button);
  right_box->addLayout(title_row);
  m_snapshots_list = make_two_column_list(1);
  right_box->addWidget(m_snapshots_list, 1);

  auto* buttons = new QHBoxLayout();
  buttons->setSpacing(6);
  m_restore_button = new QPushButton("Restore snapshot");
  m_restore_button->setToolTip(
      "Make the world match the selected snapshot. Changes not in a snapshot "
      "are kept as a snapshot first.");
  m_discard_button = new QPushButton("Discard changes");
  m_discard_button->setToolTip(
      "Throw away the changes since the last snapshot.");
  buttons->addWidget(m_restore_button, 1);
  buttons->addWidget(m_discard_button, 1);
  right_box->addLayout(buttons);

  m_details_view = new QTextBrowser();
  m_details_view->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  m_details_view->setFixedHeight(90);
  right_box->addWidget(m_details_view);
  columns->addWidget(right);
  columns->setStretchFactor(0, 0);
  columns->setStretchFactor(1, 1);
  columns->setSizes(QList<int>() << 340 << 400);

  // Taking snapshots.
  content->addWidget(make_separator());
  auto* form = new QGridLayout();
  form->setVerticalSpacing(6);
  form->setHorizontalSpacing(6);
  form->addWidget(make_label("Name:"), 0, 0);
  m_name_entry = new QLineEdit();
  form->addWidget(m_name_entry, 0, 1);
  form->addWidget(make_label("Description:"), 1, 0);
  m_description_entry = new QLineEdit();
  m_description_entry->setPlaceholderText("Optional");
  form->addWidget(m_description_entry, 1, 1);
  auto* take_row = new QHBoxLayout();
  take_row->setSpacing(6);
  m_take_button = new QPushButton("Take snapshot");
  m_take_button->setToolTip("Take a snapshot of the selected world.");
  m_take_all_button = new QPushButton("Take snapshot of all worlds");
  m_take_all_button->setToolTip(
      "Take a snapshot of every world that has changes.");
  take_row->addWidget(m_take_button);
  take_row->addWidget(m_take_all_button);
  take_row->addStretch(1);
  form->addLayout(take_row, 2, 1);
  content->addLayout(form);

  // Footer: size and the command to use the repository from a terminal.
  content->addWidget(make_separator());
  auto* footer = new QHBoxLayout();
  footer->setSpacing(12);
  m_footer_label = make_label("");
  m_command_entry = new QLineEdit();
  m_command_entry->setReadOnly(true);
  m_command_entry->setFrame(false);
  m_command_entry->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  // Looks like a label (selectable, and it scrolls instead of making the window
  // wider).
  QPalette label_like = m_command_entry->palette();
  label_like.setColor(QPalette::Base, label_like.color(QPalette::Window));
  m_command_entry->setPalette(label_like);
  m_copy_button = new QPushButton("Copy");
  m_copy_button->setToolTip(
      "Copy the git command for this world, to use its snapshots from a "
      "terminal.");
  footer->addWidget(m_footer_label);
  footer->addWidget(m_command_entry, 1);
  footer->addWidget(m_copy_button);
  content->addLayout(footer);

  connect(m_worlds_list, &QTreeWidget::itemSelectionChanged, this,
          &TabSnapshots::on_world_selected);
  connect(m_snapshots_list, &QTreeWidget::itemSelectionChanged, this,
          &TabSnapshots::on_snapshot_selected);
  connect(m_name_entry, &QLineEdit::textChanged, this,
          &TabSnapshots::update_buttons);
  connect(m_name_entry, &QLineEdit::returnPressed, this,
          &TabSnapshots::on_take_clicked);
  connect(m_description_entry, &QLineEdit::returnPressed, this,
          &TabSnapshots::on_take_clicked);
  connect(m_take_button, &QPushButton::clicked, this,
          &TabSnapshots::on_take_clicked);
  connect(m_take_all_button, &QPushButton::clicked, this,
          &TabSnapshots::on_take_all_clicked);
  connect(m_restore_button, &QPushButton::clicked, this,
          &TabSnapshots::on_restore_clicked);
  connect(m_discard_button, &QPushButton::clicked, this,
          &TabSnapshots::on_discard_clicked);
  connect(m_copy_button, &QPushButton::clicked, this,
          &TabSnapshots::on_copy_clicked);
  connect(m_export_action, &QAction::triggered, this,
          &TabSnapshots::on_export_clicked);
  connect(m_import_action, &QAction::triggered, this,
          &TabSnapshots::on_import_clicked);
  connect(m_start_fresh_action, &QAction::triggered, this,
          &TabSnapshots::on_start_fresh_clicked);
  connect(m_delete_action, &QAction::triggered, this,
          &TabSnapshots::on_delete_clicked);
}

void TabSnapshots::show_unavailable(const QString& message) {
  m_hint_label->setText(message);
  m_hint_label->setVisible(true);
  m_content->setVisible(false);
}

void TabSnapshots::refresh() {
  const std::string game = m_ctx.settings.get_string("game");
  const Paths::InstallsSummary installs = m_ctx.paths.installs_summary();
  if (!installs.contains(game)) {
    show_unavailable("Install the game first (on the Game tab).");
    return;
  }
  if (!git::available()) {
    show_unavailable(
        "Snapshots need git 2.23 or newer, and it was not found. Install git "
        "with your package "
        "manager, then come back to this tab.");
    return;
  }
  m_hint_label->setVisible(false);
  m_content->setVisible(true);

  std::tm local = platform::local_time(std::time(nullptr));
  m_name_entry->setText(qstr(SnapshotManager::default_snapshot_name(local)));
  m_description_entry->setText("");

  reload();
}

// git status for every world can take a while, so it runs on the worker thread.
void TabSnapshots::reload() {
  const std::shared_ptr<std::vector<WorldInfo> > loaded =
      std::make_shared<std::vector<WorldInfo> >();
  m_content->setEnabled(false);
  const bool started =
      m_runner.run([this, loaded] { *loaded = m_ctx.snapshots.list_worlds(); },
                   [this, loaded] {
                     m_content->setEnabled(true);
                     fill_worlds(*loaded);
                     ask_about_deleted_worlds();
                   });
  if (!started) {
    m_content->setEnabled(true);
  }
}

void TabSnapshots::fill_worlds(const std::vector<WorldInfo>& worlds) {
  m_worlds = worlds;

  m_updating = true;
  m_worlds_list->clear();
  const QBrush dim = dim_brush(m_worlds_list);
  int reselect = -1;
  for (size_t i = 0; i < m_worlds.size(); ++i) {
    const WorldInfo& world = m_worlds[i];
    auto* row = new QTreeWidgetItem(m_worlds_list);
    row->setText(0, qstr(world.name));
    row->setText(1, state_text(world));
    row->setForeground(1, dim);
    if (world.state == WorldInfo::DELETED) {
      row->setForeground(0, dim);
    }
    row->setToolTip(0, qstr(world.name));
    if (world.name == m_selected_name) {
      reselect = static_cast<int>(i);
    }
  }
  if (reselect < 0 && !m_worlds.empty()) {
    reselect = 0;
  }
  if (reselect >= 0) {
    m_worlds_list->setCurrentItem(m_worlds_list->topLevelItem(reselect));
  }
  m_updating = false;

  on_world_selected();
}

const WorldInfo* TabSnapshots::selected_world() {
  const int row = selected_row(m_worlds_list);
  if (row < 0 || static_cast<size_t>(row) >= m_worlds.size()) {
    return nullptr;
  }
  return &m_worlds[row];
}

const Snapshot* TabSnapshots::selected_snapshot() {
  const int row = selected_row(m_snapshots_list);
  if (row < 0 || static_cast<size_t>(row) >= m_snapshots.size()) {
    return nullptr;
  }
  return &m_snapshots[row];
}

void TabSnapshots::on_world_selected() {
  if (m_updating) {
    return;
  }
  const WorldInfo* world = selected_world();
  m_selected_name = (world != nullptr) ? world->name : "";
  fill_snapshots();
}

// Reading one world's history is quick, so this stays on the main thread.
void TabSnapshots::fill_snapshots() {
  const WorldInfo* world = selected_world();
  m_snapshots = (world != nullptr) ? m_ctx.snapshots.list_snapshots(world->name)
                                   : std::vector<Snapshot>();

  m_updating = true;
  m_snapshots_list->clear();
  const QBrush dim = dim_brush(m_snapshots_list);
  for (const auto& m_snapshot : m_snapshots) {
    auto* row = new QTreeWidgetItem(m_snapshots_list);
    row->setText(0, format_time(m_snapshot.time, "MMM dd HH:mm"));
    row->setForeground(0, dim);
    row->setText(1, qstr(m_snapshot.name));
  }
  m_updating = false;

  if (world == nullptr) {
    m_snapshots_title->setText(m_worlds.empty() ? "There are no worlds yet."
                                                : "Select a world.");
    m_footer_label->setText("");
    m_command_entry->setText("");
  } else {
    m_snapshots_title->setText(qstr("Snapshots of " + world->name));
    if (m_snapshots.empty()) {
      m_footer_label->setText("No snapshots yet");
    } else {
      m_footer_label->setText(
          qstr(std::to_string(m_snapshots.size()) +
               (m_snapshots.size() == 1 ? " snapshot, " : " snapshots, ")) +
          format_size(m_ctx.snapshots.history_size(world->name)));
    }
    m_command_entry->setText(qstr(m_ctx.snapshots.git_command(world->name)));
    m_command_entry->setCursorPosition(0);
    m_command_entry->setToolTip(m_command_entry->text());
  }
  update_details();
  update_buttons();
}

void TabSnapshots::on_snapshot_selected() {
  if (m_updating) {
    return;
  }
  update_details();
  update_buttons();
}

void TabSnapshots::update_details() {
  const WorldInfo* world = selected_world();
  const Snapshot* snap = selected_snapshot();
  if ((world == nullptr) || (snap == nullptr)) {
    m_details_view->setPlainText(NO_DETAILS_TEXT);
    return;
  }
  const int files = m_ctx.snapshots.changed_files(world->name, snap->hash);
  QString line = format_time(snap->time, "MMM dd yyyy, HH:mm");
  if (files >= 0) {
    line += qstr(" \xc2\xb7 " + std::to_string(files) +
                 (files == 1 ? " file changed" : " files changed"));
  }
  QString text = "<b>" + escape(snap->name) + "</b><br>" + line.toHtmlEscaped();
  if (!snap->description.empty()) {
    text += "<br><br>" + escape(snap->description);
  }
  m_details_view->setHtml(text);
}

void TabSnapshots::update_buttons() {
  const WorldInfo* world = selected_world();
  const bool deleted = (world != nullptr) && world->state == WorldInfo::DELETED;
  const bool has_name = !m_name_entry->text().isEmpty();
  bool any_world = false;
  for (const auto& m_world : m_worlds) {
    any_world = any_world || m_world.state != WorldInfo::DELETED;
  }

  m_restore_button->setEnabled((world != nullptr) &&
                               (selected_snapshot() != nullptr));
  m_discard_button->setEnabled((world != nullptr) &&
                               world->state == WorldInfo::CHANGED);
  m_take_button->setEnabled((world != nullptr) && !deleted && has_name);
  m_take_all_button->setEnabled(any_world && has_name);
  m_copy_button->setEnabled(world != nullptr);
  m_export_action->setEnabled((world != nullptr) && !m_snapshots.empty());
  m_start_fresh_action->setEnabled((world != nullptr) && !deleted &&
                                   !m_snapshots.empty() && has_name);
  m_delete_action->setEnabled(deleted);
}

// A world folder that disappeared (deleted in the game, or by restoring a zip
// backup) while its snapshots stayed. Asks once per world: keep them or trash
// them.
void TabSnapshots::ask_about_deleted_worlds() {
  for (const auto& world : m_worlds) {
    if (m_asked.contains(world.name) || world.state != WorldInfo::DELETED ||
        world.kept) {
      continue;
    }

    const std::string name = world.name;
    m_asked.insert(name);
    const bool use_trash = m_ctx.settings.get_bool("use_trash");
    QStringList buttons;
    buttons << "Cancel" << qstr(dialogs::removal_button(use_trash));
    dialogs::ask(
        m_parent, qstr(name + " no longer exists"),
        qstr(std::format("Delete its snapshots too? They {}.\n\nCancel keeps "
                         "them: the world stays in the list as deleted, and "
                         "restoring one of its snapshots brings it back.",
                         dialogs::removal_text(use_trash))),
        buttons, [this, name](int choice) {
          if (choice == 1) {
            run_task([this, name] { m_ctx.snapshots.delete_snapshots(name); });
          } else {
            m_ctx.snapshots.mark_kept(name);
            reload();
          }
        });
    return;  // One dialog at a time; the reload after the answer asks about the
             // next one.
  }
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is
// done.
void TabSnapshots::run_task(const std::function<void()>& work) {
  m_content->setEnabled(false);
  const bool started = m_runner.run(work, [this] {
    m_content->setEnabled(true);
    reload();
  });
  if (!started) {
    m_content->setEnabled(true);
  }
}

// Says so in a dialog, instead of failing with a git error.
bool TabSnapshots::check_writable() {
  std::string reason;
  if (m_ctx.snapshots.can_write(reason)) {
    return true;
  }
  dialogs::tell(m_parent, "Can't change the saves", qstr(reason));
  return false;
}

void TabSnapshots::on_take_clicked() {
  const WorldInfo* world = selected_world();
  const std::string name = str(m_name_entry->text());
  if ((world == nullptr) || world->state == WorldInfo::DELETED ||
      name.empty() || !check_writable()) {
    return;
  }
  const std::string world_name = world->name;
  const std::string description = str(m_description_entry->text());
  run_task([this, world_name, name, description] {
    m_ctx.snapshots.take_snapshot(world_name, name, description);
  });
}

void TabSnapshots::on_take_all_clicked() {
  const std::string name = str(m_name_entry->text());
  if (name.empty() || !check_writable()) {
    return;
  }
  const std::string description = str(m_description_entry->text());
  run_task([this, name, description] {
    m_ctx.snapshots.take_snapshot_all(name, description);
  });
}

void TabSnapshots::on_restore_clicked() {
  const WorldInfo* world = selected_world();
  const Snapshot* snap = selected_snapshot();
  if ((world == nullptr) || (snap == nullptr) || !check_writable()) {
    return;
  }
  const std::string world_name = world->name;
  const std::string hash = snap->hash;

  const std::string detail =
      (world->state == WorldInfo::DELETED)
          ? world_name + " will be brought back as it was in \"" + snap->name +
                "\"."
          : world_name + " will be changed to match \"" + snap->name +
                "\". Changes not in a snapshot are first kept as the snapshot "
                "\"Before restoring " +
                snap->name + "\", so nothing is lost.";
  dialogs::confirm(m_parent, "Restore this snapshot?",
                   qstr(detail + " Close the game first."), "Restore",
                   [this, world_name, hash] {
                     run_task([this, world_name, hash] {
                       m_ctx.snapshots.restore(world_name, hash);
                     });
                   });
}

void TabSnapshots::on_discard_clicked() {
  const WorldInfo* world = selected_world();
  if ((world == nullptr) || world->state != WorldInfo::CHANGED ||
      !check_writable()) {
    return;
  }
  const std::string world_name = world->name;
  const std::string last =
      m_snapshots.empty() ? "" : " \"" + m_snapshots[0].name + "\"";
  dialogs::confirm(
      m_parent, "Discard changes?",
      qstr("The changes to " + world_name + " since the last snapshot" + last +
           " will be thrown away. They are not in any snapshot, so they can't "
           "be brought back."),
      "Discard", [this, world_name] {
        run_task([this, world_name] {
          m_ctx.snapshots.discard_changes(world_name);
        });
      });
}

void TabSnapshots::on_copy_clicked() {
  const WorldInfo* world = selected_world();
  if (world == nullptr) {
    return;
  }
  QGuiApplication::clipboard()->setText(
      qstr(m_ctx.snapshots.git_command(world->name)));
  m_ctx.status.post("Copied the git command for " + world->name + ".");
}

void TabSnapshots::on_export_clicked() {
  const WorldInfo* world = selected_world();
  if ((world == nullptr) || m_snapshots.empty()) {
    return;
  }
  const std::string world_name = world->name;
  // The file dialog asks before replacing an existing file.
  const QString file = QFileDialog::getSaveFileName(
      &m_parent, qstr("Export the snapshots of " + world_name),
      qstr(world_name + ".bundle"), BUNDLE_FILTER);
  if (file.isEmpty()) {
    return;
  }
  const std::filesystem::path path = to_path(str(file));
  run_task([this, world_name, path] {
    m_ctx.snapshots.export_snapshots(world_name, path);
  });
}

void TabSnapshots::on_import_clicked() {
  const QString file = QFileDialog::getOpenFileName(
      &m_parent, "Import snapshots", QString(), BUNDLE_FILTER);
  if (file.isEmpty()) {
    return;
  }
  const std::filesystem::path path = to_path(str(file));
  const std::string world = SnapshotManager::bundle_world_name(path);
  const std::string problem = m_ctx.snapshots.check_import_name(world);
  if (problem.empty()) {
    import_bundle(path, world);
    return;
  }
  // Never replaces a world: ask for another name instead.
  dialogs::ask_text(
      m_parent, "Name the imported world",
      qstr(problem + " Choose a name for the world made from this file."),
      qstr(world), "Import",
      [this](const std::string& name) {
        return m_ctx.snapshots.check_import_name(name);
      },
      [this, path](const std::string& name) { import_bundle(path, name); });
}

void TabSnapshots::import_bundle(const std::filesystem::path& path,
                                 const std::string& world) {
  if (!check_writable()) {
    return;
  }
  m_selected_name = world;  // Shown once the reload is done.
  run_task(
      [this, path, world] { m_ctx.snapshots.import_snapshots(path, world); });
}

void TabSnapshots::on_start_fresh_clicked() {
  const WorldInfo* world = selected_world();
  std::string name = str(m_name_entry->text());
  if ((world == nullptr) || world->state == WorldInfo::DELETED ||
      m_snapshots.empty() || name.empty() || !check_writable()) {
    return;
  }
  const std::string world_name = world->name;
  const std::string description = str(m_description_entry->text());
  std::string count = std::to_string(m_snapshots.size()) +
                      (m_snapshots.size() == 1 ? " snapshot" : " snapshots");
  const bool use_trash = m_ctx.settings.get_bool("use_trash");
  dialogs::confirm(
      m_parent, qstr("Start fresh with " + world_name + "?"),
      qstr(std::format(
          "Its {} ({}) {}. A new history starts with one snapshot of the world "
          "as it is now, \"{}\". The world itself is not changed.",
          count, str(format_size(m_ctx.snapshots.history_size(world_name))),
          dialogs::removal_text(use_trash), name)),
      "Start Fresh", [this, world_name, name, description] {
        run_task([this, world_name, name, description] {
          m_ctx.snapshots.start_fresh(world_name, name, description);
        });
      });
}

void TabSnapshots::on_delete_clicked() {
  const WorldInfo* world = selected_world();
  if ((world == nullptr) || world->state != WorldInfo::DELETED) {
    return;
  }
  std::string world_name = world->name;
  std::string count = std::to_string(m_snapshots.size()) +
                      (m_snapshots.size() == 1 ? " snapshot" : " snapshots");
  const bool use_trash = m_ctx.settings.get_bool("use_trash");
  dialogs::confirm(
      m_parent, qstr("Delete the snapshots of " + world_name + "?"),
      qstr(std::format(
          "{} no longer exists. Its {} ({}) {}.", world_name, count,
          str(format_size(m_ctx.snapshots.history_size(world_name))),
          dialogs::removal_text(use_trash))),
      qstr(dialogs::removal_button(use_trash)), [this, world_name] {
        run_task([this, world_name] {
          m_ctx.snapshots.delete_snapshots(world_name);
        });
      });
}
