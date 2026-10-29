#include "ui/tab_backups.h"

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
#include <memory>

#include "core/fs.h"
#include "core/git.h"
#include "core/platform.h"
#include "ui/dialogs.h"
#include "ui/qt_helpers.h"

static const char* const NO_INFO_TEXT =
    "Select an existing backup to see its details here.";

TabBackups::TabBackups(AppContext& ctx, TaskRunner& runner, QWidget& parent)
    : ctx_(ctx), runner_(runner), parent_(parent), updating_(false) {
  build_ui();
}

void TabBackups::build_ui() {
  QVBoxLayout* outer = new QVBoxLayout(this);
  outer->setContentsMargins(12, 12, 12, 12);
  hint_label_ = make_label("");
  hint_label_->setWordWrap(true);
  hint_label_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  outer->addWidget(hint_label_);
  content_ = new QWidget();
  outer->addWidget(content_, 1);

  QVBoxLayout* content = new QVBoxLayout(content_);
  content->setContentsMargins(0, 0, 0, 0);
  content->setSpacing(8);
  content->addWidget(make_label("Save directory backups:"));
  list_ = new QListWidget();
  list_->setSelectionMode(QAbstractItemView::SingleSelection);
  content->addWidget(list_, 1);

  QHBoxLayout* buttons = new QHBoxLayout();
  buttons->setSpacing(6);
  restore_button_ = new QPushButton("Restore");
  restore_button_->setToolTip(
      "Replace your current saves with the selected backup.");
  delete_button_ = new QPushButton("Delete");
  delete_button_->setToolTip("Delete the selected backup.");
  refresh_button_ = new QPushButton("Refresh");
  buttons->addWidget(restore_button_, 1);
  buttons->addWidget(delete_button_, 1);
  buttons->addWidget(refresh_button_, 1);
  content->addLayout(buttons);

  info_view_ = new QTextBrowser();
  info_view_->setOpenExternalLinks(true);
  info_view_->setFixedHeight(90);
  content->addWidget(info_view_);

  QFrame* separator = new QFrame();
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  content->addWidget(separator);
  content->addWidget(make_label("Manual backup"));
  QHBoxLayout* create_row = new QHBoxLayout();
  create_row->setSpacing(6);
  create_row->addWidget(make_label("Name:"));
  name_entry_ = new QLineEdit();
  name_entry_->setToolTip(
      "Characters that are not allowed in file names, and spaces at the start "
      "or end, are not allowed here.");
  create_row->addWidget(name_entry_, 1);
  create_button_ = new QPushButton("Create");
  create_button_->setToolTip("Back up the saved games of the current game.");
  create_row->addWidget(create_button_);
  content->addLayout(create_row);

  connect(list_, &QListWidget::itemSelectionChanged, this,
          &TabBackups::on_selection_changed);
  connect(name_entry_, &QLineEdit::textChanged, this,
          &TabBackups::on_name_changed);
  connect(name_entry_, &QLineEdit::returnPressed, this,
          &TabBackups::on_create_clicked);
  connect(create_button_, &QPushButton::clicked, this,
          &TabBackups::on_create_clicked);
  connect(restore_button_, &QPushButton::clicked, this,
          &TabBackups::on_restore_clicked);
  connect(delete_button_, &QPushButton::clicked, this,
          &TabBackups::on_delete_clicked);
  connect(refresh_button_, &QPushButton::clicked, this,
          &TabBackups::reload_list);
}

void TabBackups::show_unavailable(const QString& message) {
  hint_label_->setText(message);
  hint_label_->setVisible(true);
  content_->setVisible(false);
}

void TabBackups::refresh() {
  std::string game = ctx_.settings.get_string("game");
  Paths::InstallsSummary installs = ctx_.paths.installs_summary();
  if (installs.find(game) == installs.end()) {
    show_unavailable("Install the game first (on the Game tab).");
    return;
  }
  hint_label_->setVisible(false);
  content_->setVisible(true);

  std::tm local = platform::local_time(std::time(nullptr));
  name_entry_->setText(qstr(BackupManager::default_backup_name(local)));

  reload_list();
}

void TabBackups::reload_list() {
  backups_ = ctx_.backups.list();

  updating_ = true;
  list_->clear();
  for (size_t i = 0; i < backups_.size(); ++i)
    list_->addItem(qstr(backups_[i].name));
  updating_ = false;

  info_view_->setPlainText(NO_INFO_TEXT);
  update_buttons();
}

int TabBackups::selected_backup() {
  int row = selected_row(list_);
  if (row < 0 || (size_t)row >= backups_.size()) return -1;
  return row;
}

void TabBackups::update_buttons() {
  bool have = (selected_backup() >= 0);
  restore_button_->setEnabled(have);
  delete_button_->setEnabled(have);
  create_button_->setEnabled(
      BackupManager::is_valid_backup_name(str(name_entry_->text())));
}

void TabBackups::on_selection_changed() {
  if (updating_) return;
  update_buttons();

  int index = selected_backup();
  if (index < 0) {
    info_view_->setPlainText(NO_INFO_TEXT);
    return;
  }

  const BackupSummary& backup = backups_[index];
  std::string worlds;
  for (size_t i = 0; i < backup.worlds.size(); ++i)
    worlds += (i > 0 ? ", " : "") + backup.worlds[i];

  QString url = QUrl::fromLocalFile(qstr(backup.path)).toString();
  QString text = "<u>Location:</u><br><a href=\"" + url.toHtmlEscaped() +
                 "\">" + escape(to_utf8(backup.path)) +
                 "</a><br><br><u>Worlds:</u><br>" + escape(worlds);
  info_view_->setHtml(text);
}

// Red entry text for a name that can't be used.
void TabBackups::on_name_changed() {
  std::string name = str(name_entry_->text());
  bool valid = BackupManager::is_valid_backup_name(name);
  name_entry_->setStyleSheet(
      name.empty() || valid ? QString() : QString("color: #d02020;"));
  update_buttons();
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is
// done.
void TabBackups::run_task(const std::function<void()>& work) {
  content_->setEnabled(false);
  bool started = runner_.run(work, [this]() {
    content_->setEnabled(true);
    reload_list();
  });
  if (!started) content_->setEnabled(true);
}

void TabBackups::on_create_clicked() {
  std::string name = str(name_entry_->text());
  if (!BackupManager::is_valid_backup_name(name)) return;
  run_task([this, name]() { ctx_.backups.backup_current(name); });
}

void TabBackups::on_restore_clicked() {
  int index = selected_backup();
  if (index < 0) return;
  std::string name = backups_[index].name;

  // Running git status on every world can take a while, so the worlds with
  // changes not in a snapshot are found on the worker thread.
  std::shared_ptr<std::vector<std::string> > changed =
      std::make_shared<std::vector<std::string> >();
  content_->setEnabled(false);
  bool started = runner_.run(
      [this, changed]() {
        if (git::available()) *changed = ctx_.snapshots.worlds_with_changes();
      },
      [this, changed, name]() {
        content_->setEnabled(true);
        confirm_restore(name, *changed);
      });
  if (!started) content_->setEnabled(true);
}

void TabBackups::confirm_restore(const std::string& name,
                                 const std::vector<std::string>& changed) {
  // This replaces the current saves, so make sure that is what the user wants.
  std::string current =
      ctx_.settings.get_bool("use_trash")
          ? "Your current saves of this game will be moved to the trash"
          : "Your current saves of this game will be deleted for good";
  std::string detail = current + " and replaced with the backup \"" + name +
                       "\". Create a backup of the current saves first if you "
                       "want to keep them.";
  if (!changed.empty()) {
    std::string worlds;
    for (size_t i = 0; i < changed.size(); ++i)
      worlds += (i == 0 ? "" : (i + 1 == changed.size() ? " and " : ", ")) +
                changed[i];
    detail += "\n\n" + worlds + (changed.size() == 1 ? " has" : " have") +
              " changes not in a snapshot. Take a snapshot on the Snapshots "
              "tab first to keep them there.";
  }
  dialogs::confirm(parent_, "Restore this backup?", qstr(detail), "Restore",
                   [this, name]() {
                     run_task([this, name]() { ctx_.backups.restore(name); });
                   });
}

void TabBackups::on_delete_clicked() {
  int index = selected_backup();
  if (index < 0) return;
  std::string name = backups_[index].name;
  bool use_trash = ctx_.settings.get_bool("use_trash");
  dialogs::confirm(
      parent_, "Delete the backup?",
      qstr("The backup \"" + name + "\" " + dialogs::removal_text(use_trash) +
           "."),
      qstr(dialogs::removal_button(use_trash)), [this, name]() {
        run_task([this, name]() { ctx_.backups.delete_backup(name); });
      });
}
