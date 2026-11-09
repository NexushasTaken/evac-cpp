#include "ui/tab_backups.h"

#include <qnamespace.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>
#include <ctime>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/backup_manager.h"
#include "core/fs.h"
#include "core/git.h"
#include "core/paths.h"
#include "core/platform.h"
#include "ui/app_context.h"
#include "ui/dialogs.h"
#include "ui/qt_helpers.h"
#include "ui/task_runner.h"

static const char* const NO_INFO_TEXT =
    "Select an existing backup to see its details here.";

TabBackups::TabBackups(AppContext& ctx, TaskRunner& runner, QWidget& parent)
    : m_ctx(ctx), m_runner(runner), m_parent(parent) {
  build_ui();
}

void TabBackups::build_ui() {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(12, 12, 12, 12);
  m_hint_label = make_label("");
  m_hint_label->setWordWrap(true);
  m_hint_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  outer->addWidget(m_hint_label);
  m_content = new QWidget();
  outer->addWidget(m_content, 1);

  auto* content = new QVBoxLayout(m_content);
  content->setContentsMargins(0, 0, 0, 0);
  content->setSpacing(8);
  content->addWidget(make_label("Save directory backups:"));
  m_list = new QListWidget();
  m_list->setSelectionMode(QAbstractItemView::SingleSelection);
  content->addWidget(m_list, 1);

  auto* buttons = new QHBoxLayout();
  buttons->setSpacing(6);
  m_restore_button = new QPushButton("Restore");
  m_restore_button->setToolTip(
      "Replace your current saves with the selected backup.");
  m_delete_button = new QPushButton("Delete");
  m_delete_button->setToolTip("Delete the selected backup.");
  m_refresh_button = new QPushButton("Refresh");
  buttons->addWidget(m_restore_button, 1);
  buttons->addWidget(m_delete_button, 1);
  buttons->addWidget(m_refresh_button, 1);
  content->addLayout(buttons);

  m_info_view = new QTextBrowser();
  m_info_view->setOpenExternalLinks(true);
  m_info_view->setFixedHeight(90);
  content->addWidget(m_info_view);

  auto* separator = new QFrame();
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  content->addWidget(separator);
  content->addWidget(make_label("Manual backup"));
  auto* create_row = new QHBoxLayout();
  create_row->setSpacing(6);
  create_row->addWidget(make_label("Name:"));
  m_name_entry = new QLineEdit();
  m_name_entry->setToolTip(
      "Characters that are not allowed in file names, and spaces at the start "
      "or end, are not allowed here.");
  create_row->addWidget(m_name_entry, 1);
  m_create_button = new QPushButton("Create");
  m_create_button->setToolTip("Back up the saved games of the current game.");
  create_row->addWidget(m_create_button);
  content->addLayout(create_row);

  connect(m_list, &QListWidget::itemSelectionChanged, this,
          &TabBackups::on_selection_changed);
  connect(m_name_entry, &QLineEdit::textChanged, this,
          &TabBackups::on_name_changed);
  connect(m_name_entry, &QLineEdit::returnPressed, this,
          &TabBackups::on_create_clicked);
  connect(m_create_button, &QPushButton::clicked, this,
          &TabBackups::on_create_clicked);
  connect(m_restore_button, &QPushButton::clicked, this,
          &TabBackups::on_restore_clicked);
  connect(m_delete_button, &QPushButton::clicked, this,
          &TabBackups::on_delete_clicked);
  connect(m_refresh_button, &QPushButton::clicked, this,
          &TabBackups::reload_list);
}

void TabBackups::show_unavailable(const QString& message) {
  m_hint_label->setText(message);
  m_hint_label->setVisible(true);
  m_content->setVisible(false);
}

void TabBackups::refresh() {
  const std::string game = m_ctx.settings.get_string("game");
  const Paths::InstallsSummary installs = m_ctx.paths.installs_summary();
  if (!installs.contains(game)) {
    show_unavailable("Install the game first (on the Game tab).");
    return;
  }
  m_hint_label->setVisible(false);
  m_content->setVisible(true);

  std::tm local = platform::local_time(std::time(nullptr));
  m_name_entry->setText(qstr(BackupManager::default_backup_name(local)));

  reload_list();
}

void TabBackups::reload_list() {
  m_backups = m_ctx.backups.list();

  m_updating = true;
  m_list->clear();
  for (const auto& m_backup : m_backups) {
    m_list->addItem(qstr(m_backup.name));
  }
  m_updating = false;

  m_info_view->setPlainText(NO_INFO_TEXT);
  update_buttons();
}

int TabBackups::selected_backup() {
  const int row = selected_row(m_list);
  if (row < 0 || static_cast<size_t>(row) >= m_backups.size()) {
    return -1;
  }
  return row;
}

void TabBackups::update_buttons() {
  const bool have = (selected_backup() >= 0);
  m_restore_button->setEnabled(have);
  m_delete_button->setEnabled(have);
  m_create_button->setEnabled(
      BackupManager::is_valid_backup_name(str(m_name_entry->text())));
}

void TabBackups::on_selection_changed() {
  if (m_updating) {
    return;
  }
  update_buttons();

  const int index = selected_backup();
  if (index < 0) {
    m_info_view->setPlainText(NO_INFO_TEXT);
    return;
  }

  const BackupSummary& backup = m_backups.at(index);
  std::string worlds;
  for (size_t i = 0; i < backup.worlds.size(); ++i) {
    worlds += (i > 0 ? ", " : "") + backup.worlds.at(i);
  }

  const QString url = QUrl::fromLocalFile(qstr(backup.path)).toString();
  const QString text = "<u>Location:</u><br><a href=\"" + url.toHtmlEscaped() +
                       "\">" + escape(to_utf8(backup.path)) +
                       "</a><br><br><u>Worlds:</u><br>" + escape(worlds);
  m_info_view->setHtml(text);
}

// Red entry text for a name that can't be used.
void TabBackups::on_name_changed() {
  const std::string name = str(m_name_entry->text());
  const bool valid = BackupManager::is_valid_backup_name(name);
  m_name_entry->setStyleSheet(
      name.empty() || valid ? QString() : QString("color: #d02020;"));
  update_buttons();
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is
// done.
void TabBackups::run_task(const std::function<void()>& work) {
  m_content->setEnabled(false);
  const bool started = m_runner.run(work, [this] {
    m_content->setEnabled(true);
    reload_list();
  });
  if (!started) {
    m_content->setEnabled(true);
  }
}

void TabBackups::on_create_clicked() {
  const std::string name = str(m_name_entry->text());
  if (!BackupManager::is_valid_backup_name(name)) {
    return;
  }
  run_task([this, name] { m_ctx.backups.backup_current(name); });
}

void TabBackups::on_restore_clicked() {
  const int index = selected_backup();
  if (index < 0) {
    return;
  }
  const std::string name = m_backups.at(index).name;

  // Running git status on every world can take a while, so the worlds with
  // changes not in a snapshot are found on the worker thread.
  const std::shared_ptr<std::vector<std::string> > changed =
      std::make_shared<std::vector<std::string> >();
  m_content->setEnabled(false);
  const bool started = m_runner.run(
      [this, changed] {
        if (git::available()) {
          *changed = m_ctx.snapshots.worlds_with_changes();
        }
      },
      [this, changed, name] {
        m_content->setEnabled(true);
        confirm_restore(name, *changed);
      });
  if (!started) {
    m_content->setEnabled(true);
  }
}

void TabBackups::confirm_restore(const std::string& name,
                                 const std::vector<std::string>& changed) {
  // This replaces the current saves, so make sure that is what the user wants.
  const std::string current =
      m_ctx.settings.get_bool("use_trash")
          ? "Your current saves of this game will be moved to the trash"
          : "Your current saves of this game will be deleted for good";
  std::string detail = current + " and replaced with the backup \"" + name +
                       "\". Create a backup of the current saves first if you "
                       "want to keep them.";
  if (!changed.empty()) {
    std::string worlds;
    for (size_t i = 0; i < changed.size(); ++i) {
      worlds += (i == 0 ? "" : (i + 1 == changed.size() ? " and " : ", ")) +
                changed.at(i);
    }
    detail += "\n\n" + worlds + (changed.size() == 1 ? " has" : " have") +
              " changes not in a snapshot. Take a snapshot on the Snapshots "
              "tab first to keep them there.";
  }
  dialogs::confirm(m_parent, "Restore this backup?", qstr(detail), "Restore",
                   [this, name] {
                     run_task([this, name] { m_ctx.backups.restore(name); });
                   });
}

void TabBackups::on_delete_clicked() {
  const int index = selected_backup();
  if (index < 0) {
    return;
  }
  std::string name = m_backups.at(index).name;
  const bool use_trash = m_ctx.settings.get_bool("use_trash");
  dialogs::confirm(
      m_parent, "Delete the backup?",
      qstr(std::format("The backup \"{}\" {}.", name,
                       dialogs::removal_text(use_trash))),
      qstr(dialogs::removal_button(use_trash)), [this, name] {
        run_task([this, name] { m_ctx.backups.delete_backup(name); });
      });
}
