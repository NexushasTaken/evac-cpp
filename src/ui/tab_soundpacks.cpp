#include "ui/tab_soundpacks.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <map>

#include "core/fs.h"
#include "ui/dialogs.h"
#include "ui/download_prompt.h"
#include "ui/qt_helpers.h"

TabSoundpacks::TabSoundpacks(AppContext& ctx, TaskRunner& runner,
                             QWidget& parent)
    : ctx_(ctx), runner_(runner), parent_(parent), updating_(false) {
  build_ui();
}

void TabSoundpacks::build_ui() {
  QVBoxLayout* outer = new QVBoxLayout(this);
  outer->setContentsMargins(12, 12, 12, 12);
  hint_label_ = make_label("");
  hint_label_->setWordWrap(true);
  hint_label_->setAlignment(
      Qt::AlignLeft | Qt::AlignTop);  // Not in the middle of the empty tab.
  outer->addWidget(hint_label_);
  content_ = new QWidget();
  outer->addWidget(content_, 1);

  QHBoxLayout* columns = new QHBoxLayout(content_);
  columns->setContentsMargins(0, 0, 0, 0);
  columns->setSpacing(12);

  // Installed packs.
  QVBoxLayout* left = new QVBoxLayout();
  left->setSpacing(6);
  left->addWidget(make_label("Installed:"));
  installed_list_ = new QListWidget();
  installed_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  left->addWidget(installed_list_, 1);
  show_stock_check_ = new QCheckBox("Show stock");
  show_stock_check_->setToolTip("Show soundpacks from data/sound on the list.");
  left->addWidget(show_stock_check_);
  toggle_button_ = new QPushButton("Disable Selected");
  toggle_button_->setToolTip(
      "Turn the selected soundpack off or on without deleting it.");
  left->addWidget(toggle_button_);
  delete_button_ = new QPushButton("Delete Selected");
  delete_button_->setToolTip(
      "Remove the selected soundpack from your game installation.");
  left->addWidget(delete_button_);
  columns->addLayout(left, 1);

  // Packs that can be downloaded.
  QVBoxLayout* right = new QVBoxLayout();
  right->setSpacing(6);
  right->addWidget(make_label("Downloadable:"));
  available_list_ = new QListWidget();
  available_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  right->addWidget(available_list_, 1);
  install_button_ = new QPushButton("Install Selected");
  install_button_->setToolTip(
      "Download the selected soundpack and add it to your game installation.");
  right->addWidget(install_button_);
  install_from_file_button_ = new QPushButton("Install From File...");
  install_from_file_button_->setToolTip(
      "Install the selected soundpack from a .zip you have already "
      "downloaded.");
  right->addWidget(install_from_file_button_);
  columns->addLayout(right, 1);

  // The list of downloadable packs never changes.
  const std::vector<SoundpackSource>& packs = available_soundpacks();
  for (size_t i = 0; i < packs.size(); ++i)
    available_list_->addItem(qstr(packs[i].name));

  connect(show_stock_check_, &QCheckBox::toggled, this,
          &TabSoundpacks::on_show_stock_toggled);
  connect(installed_list_, &QListWidget::itemSelectionChanged, this,
          &TabSoundpacks::on_installed_selected);
  connect(available_list_, &QListWidget::itemSelectionChanged, this,
          &TabSoundpacks::on_available_selected);
  connect(toggle_button_, &QPushButton::clicked, this,
          &TabSoundpacks::on_toggle_clicked);
  connect(delete_button_, &QPushButton::clicked, this,
          &TabSoundpacks::on_delete_clicked);
  connect(install_button_, &QPushButton::clicked, this,
          &TabSoundpacks::on_install_clicked);
  connect(install_from_file_button_, &QPushButton::clicked, this,
          &TabSoundpacks::on_install_from_file_clicked);
}

void TabSoundpacks::show_unavailable(const QString& message) {
  hint_label_->setText(message);
  hint_label_->setVisible(true);
  content_->setVisible(false);
}

void TabSoundpacks::refresh() {
  std::string game = ctx_.settings.get_string("game");
  Paths::InstallsSummary installs = ctx_.paths.installs_summary();
  if (installs.find(game) == installs.end()) {
    show_unavailable("Install the game first (on the Game tab).");
    return;
  }
  hint_label_->setVisible(false);
  content_->setVisible(true);

  updating_ = true;
  show_stock_check_->setChecked(ctx_.settings.get_bool("show_stock_sound"));
  updating_ = false;

  refresh_installed();
}

void TabSoundpacks::refresh_installed() {
  installed_ =
      ctx_.soundpacks.get_installed(ctx_.settings.get_bool("show_stock_sound"));

  updating_ = true;
  installed_list_->clear();
  // Packs can share a name (a restored copy, a copy of a stock pack...): tell
  // those apart by their folder, after the name.
  std::map<std::string, int> name_count;
  for (size_t i = 0; i < installed_.size(); ++i)
    ++name_count[installed_[i].name];

  for (size_t i = 0; i < installed_.size(); ++i) {
    const InstalledSoundpack& pack = installed_[i];
    std::string folder = to_utf8(pack.location.filename());
    std::string text = pack.name;
    if (name_count[pack.name] > 1)
      text += " (" + (pack.is_stock ? std::string("stock") : folder) + ")";
    if (!pack.enabled) text += " (Disabled)";
    QListWidgetItem* item = new QListWidgetItem(qstr(text), installed_list_);
    item->setToolTip(qstr((pack.description.empty() ? "No description provided."
                                                    : pack.description) +
                          "\nFolder: " + folder));
    if (pack.is_stock)
      item->setForeground(
          dim_brush(installed_list_));  // Greyed out, like in the original.
  }
  updating_ = false;

  update_buttons();
}

int TabSoundpacks::selected_installed() {
  int row = selected_row(installed_list_);
  if (row < 0 || (size_t)row >= installed_.size()) return -1;
  return row;
}

int TabSoundpacks::selected_available() {
  int row = selected_row(available_list_);
  if (row < 0 || (size_t)row >= available_soundpacks().size()) return -1;
  return row;
}

void TabSoundpacks::update_buttons() {
  int installed = selected_installed();
  bool can_change = (installed >= 0 && !installed_[installed].is_stock);
  delete_button_->setEnabled(can_change);
  toggle_button_->setEnabled(can_change);
  bool disabled = (installed >= 0 && !installed_[installed].enabled);
  toggle_button_->setText(disabled ? "Enable Selected" : "Disable Selected");

  int available = selected_available();
  install_button_->setEnabled(available >= 0);
  install_from_file_button_->setEnabled(available >= 0);
  // By the directory the install writes to, not by name: a stock pack with
  // the same name is not replaced.
  bool reinstall = (available >= 0 && ctx_.soundpacks.has_user_copy(
                                          available_soundpacks()[available]));
  install_button_->setText(reinstall ? "Reinstall Selected"
                                     : "Install Selected");
}

void TabSoundpacks::on_show_stock_toggled() {
  if (updating_) return;
  ctx_.settings.set_bool("show_stock_sound", show_stock_check_->isChecked());
  ctx_.settings.save();
  refresh_installed();
}

void TabSoundpacks::on_installed_selected() {
  if (!updating_) update_buttons();
}

void TabSoundpacks::on_available_selected() {
  if (!updating_) update_buttons();
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is
// done.
void TabSoundpacks::run_task(const std::function<void()>& work) {
  content_->setEnabled(false);
  bool started = runner_.run(work, [this]() {
    content_->setEnabled(true);
    refresh_installed();
  });
  if (!started) content_->setEnabled(true);
}

void TabSoundpacks::on_toggle_clicked() {
  int index = selected_installed();
  if (index < 0 || installed_[index].is_stock) return;
  std::filesystem::path location = installed_[index].location;
  bool enable = !installed_[index].enabled;
  run_task([this, location, enable]() {
    ctx_.soundpacks.set_enabled(location, enable);
  });
}

void TabSoundpacks::on_delete_clicked() {
  int index = selected_installed();
  if (index < 0 || installed_[index].is_stock) return;
  std::string name = installed_[index].name;
  std::filesystem::path location = installed_[index].location;

  // Show the folder too: two packs can have the same name.
  bool use_trash = ctx_.settings.get_bool("use_trash");
  dialogs::confirm(
      parent_, "Delete the soundpack?",
      qstr(name + " (" + to_utf8(location.filename()) + ") " +
           dialogs::removal_text(use_trash) + "."),
      qstr(dialogs::removal_button(use_trash)), [this, location]() {
        run_task([this, location]() { ctx_.soundpacks.delete_pack(location); });
      });
}

void TabSoundpacks::on_install_clicked() { install_selected(""); }

void TabSoundpacks::install_selected(const std::string& from_file) {
  int index = selected_available();
  if (index < 0) return;
  SoundpackSource pack = available_soundpacks()[index];
  bool reinstall = ctx_.soundpacks.has_user_copy(pack);
  bool keep_archive = !from_file.empty();  // Never delete the user's own file.

  std::function<void()> install = [this, pack, from_file, reinstall,
                                   keep_archive]() {
    run_task([this, pack, from_file, reinstall, keep_archive]() {
      ctx_.soundpacks.install_pack(pack, to_path(from_file), reinstall,
                                   keep_archive);
    });
  };

  // Reinstalling replaces the installed copy.
  std::string title = (reinstall ? "Reinstall " : "Install ") + pack.name + "?";
  std::string detail =
      reinstall
          ? "The installed copy " +
                dialogs::removal_text(ctx_.settings.get_bool("use_trash")) +
                ", and a fresh copy is installed."
          : std::string();
  std::string button = reinstall ? "Reinstall" : "Install";

  if (!from_file.empty()) {
    // Nothing to download.
    if (!reinstall)
      install();
    else
      dialogs::confirm(parent_, qstr(title), qstr(detail), qstr(button),
                       install);
    return;
  }

  DownloadQuestion question;
  question.title = title;
  question.detail = detail;
  question.button = button;
  question.url = pack.url;
  question.filename = pack.filename;
  confirm_download(ctx_, runner_, parent_, *content_, question, install);
}

void TabSoundpacks::on_install_from_file_clicked() {
  if (selected_available() < 0) return;

  QString downloads =
      QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  QString file = QFileDialog::getOpenFileName(
      &parent_, "Choose the soundpack archive", downloads);
  if (!file.isEmpty()) install_selected(str(file));
}
