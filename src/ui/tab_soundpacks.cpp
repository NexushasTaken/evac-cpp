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
    : m_ctx(ctx), m_runner(runner), m_parent(parent), m_updating(false) {
  build_ui();
}

void TabSoundpacks::build_ui() {
  QVBoxLayout* outer = new QVBoxLayout(this);
  outer->setContentsMargins(12, 12, 12, 12);
  m_hint_label = make_label("");
  m_hint_label->setWordWrap(true);
  m_hint_label->setAlignment(
      Qt::AlignLeft | Qt::AlignTop);  // Not in the middle of the empty tab.
  outer->addWidget(m_hint_label);
  m_content = new QWidget();
  outer->addWidget(m_content, 1);

  QHBoxLayout* columns = new QHBoxLayout(m_content);
  columns->setContentsMargins(0, 0, 0, 0);
  columns->setSpacing(12);

  // Installed packs.
  QVBoxLayout* left = new QVBoxLayout();
  left->setSpacing(6);
  left->addWidget(make_label("Installed:"));
  m_installed_list = new QListWidget();
  m_installed_list->setSelectionMode(QAbstractItemView::SingleSelection);
  left->addWidget(m_installed_list, 1);
  m_show_stock_check = new QCheckBox("Show stock");
  m_show_stock_check->setToolTip(
      "Show soundpacks from data/sound on the list.");
  left->addWidget(m_show_stock_check);
  m_toggle_button = new QPushButton("Disable Selected");
  m_toggle_button->setToolTip(
      "Turn the selected soundpack off or on without deleting it.");
  left->addWidget(m_toggle_button);
  m_delete_button = new QPushButton("Delete Selected");
  m_delete_button->setToolTip(
      "Remove the selected soundpack from your game installation.");
  left->addWidget(m_delete_button);
  columns->addLayout(left, 1);

  // Packs that can be downloaded.
  QVBoxLayout* right = new QVBoxLayout();
  right->setSpacing(6);
  right->addWidget(make_label("Downloadable:"));
  m_available_list = new QListWidget();
  m_available_list->setSelectionMode(QAbstractItemView::SingleSelection);
  right->addWidget(m_available_list, 1);
  m_install_button = new QPushButton("Install Selected");
  m_install_button->setToolTip(
      "Download the selected soundpack and add it to your game installation.");
  right->addWidget(m_install_button);
  m_install_from_file_button = new QPushButton("Install From File...");
  m_install_from_file_button->setToolTip(
      "Install the selected soundpack from a .zip you have already "
      "downloaded.");
  right->addWidget(m_install_from_file_button);
  columns->addLayout(right, 1);

  // The list of downloadable packs never changes.
  const std::vector<SoundpackSource>& packs = available_soundpacks();
  for (size_t i = 0; i < packs.size(); ++i)
    m_available_list->addItem(qstr(packs[i].name));

  connect(m_show_stock_check, &QCheckBox::toggled, this,
          &TabSoundpacks::on_show_stock_toggled);
  connect(m_installed_list, &QListWidget::itemSelectionChanged, this,
          &TabSoundpacks::on_installed_selected);
  connect(m_available_list, &QListWidget::itemSelectionChanged, this,
          &TabSoundpacks::on_available_selected);
  connect(m_toggle_button, &QPushButton::clicked, this,
          &TabSoundpacks::on_toggle_clicked);
  connect(m_delete_button, &QPushButton::clicked, this,
          &TabSoundpacks::on_delete_clicked);
  connect(m_install_button, &QPushButton::clicked, this,
          &TabSoundpacks::on_install_clicked);
  connect(m_install_from_file_button, &QPushButton::clicked, this,
          &TabSoundpacks::on_install_from_file_clicked);
}

void TabSoundpacks::show_unavailable(const QString& message) {
  m_hint_label->setText(message);
  m_hint_label->setVisible(true);
  m_content->setVisible(false);
}

void TabSoundpacks::refresh() {
  std::string game = m_ctx.settings.get_string("game");
  Paths::InstallsSummary installs = m_ctx.paths.installs_summary();
  if (installs.find(game) == installs.end()) {
    show_unavailable("Install the game first (on the Game tab).");
    return;
  }
  m_hint_label->setVisible(false);
  m_content->setVisible(true);

  m_updating = true;
  m_show_stock_check->setChecked(m_ctx.settings.get_bool("show_stock_sound"));
  m_updating = false;

  refresh_installed();
}

void TabSoundpacks::refresh_installed() {
  m_installed = m_ctx.soundpacks.get_installed(
      m_ctx.settings.get_bool("show_stock_sound"));

  m_updating = true;
  m_installed_list->clear();
  // Packs can share a name (a restored copy, a copy of a stock pack...): tell
  // those apart by their folder, after the name.
  std::map<std::string, int> name_count;
  for (size_t i = 0; i < m_installed.size(); ++i)
    ++name_count[m_installed[i].name];

  for (size_t i = 0; i < m_installed.size(); ++i) {
    const InstalledSoundpack& pack = m_installed[i];
    std::string folder = to_utf8(pack.location.filename());
    std::string text = pack.name;
    if (name_count[pack.name] > 1)
      text += " (" + (pack.is_stock ? std::string("stock") : folder) + ")";
    if (!pack.enabled) text += " (Disabled)";
    QListWidgetItem* item = new QListWidgetItem(qstr(text), m_installed_list);
    item->setToolTip(qstr((pack.description.empty() ? "No description provided."
                                                    : pack.description) +
                          "\nFolder: " + folder));
    if (pack.is_stock)
      item->setForeground(
          dim_brush(m_installed_list));  // Grayed out, like in the original.
  }
  m_updating = false;

  update_buttons();
}

int TabSoundpacks::selected_installed() {
  int row = selected_row(m_installed_list);
  if (row < 0 || (size_t)row >= m_installed.size()) return -1;
  return row;
}

int TabSoundpacks::selected_available() {
  int row = selected_row(m_available_list);
  if (row < 0 || (size_t)row >= available_soundpacks().size()) return -1;
  return row;
}

void TabSoundpacks::update_buttons() {
  int installed = selected_installed();
  bool can_change = (installed >= 0 && !m_installed[installed].is_stock);
  m_delete_button->setEnabled(can_change);
  m_toggle_button->setEnabled(can_change);
  bool disabled = (installed >= 0 && !m_installed[installed].enabled);
  m_toggle_button->setText(disabled ? "Enable Selected" : "Disable Selected");

  int available = selected_available();
  m_install_button->setEnabled(available >= 0);
  m_install_from_file_button->setEnabled(available >= 0);
  // By the directory the install writes to, not by name: a stock pack with
  // the same name is not replaced.
  bool reinstall = (available >= 0 && m_ctx.soundpacks.has_user_copy(
                                          available_soundpacks()[available]));
  m_install_button->setText(reinstall ? "Reinstall Selected"
                                      : "Install Selected");
}

void TabSoundpacks::on_show_stock_toggled() {
  if (m_updating) return;
  m_ctx.settings.set_bool("show_stock_sound", m_show_stock_check->isChecked());
  m_ctx.settings.save();
  refresh_installed();
}

void TabSoundpacks::on_installed_selected() {
  if (!m_updating) update_buttons();
}

void TabSoundpacks::on_available_selected() {
  if (!m_updating) update_buttons();
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is
// done.
void TabSoundpacks::run_task(const std::function<void()>& work) {
  m_content->setEnabled(false);
  bool started = m_runner.run(work, [this]() {
    m_content->setEnabled(true);
    refresh_installed();
  });
  if (!started) m_content->setEnabled(true);
}

void TabSoundpacks::on_toggle_clicked() {
  int index = selected_installed();
  if (index < 0 || m_installed[index].is_stock) return;
  std::filesystem::path location = m_installed[index].location;
  bool enable = !m_installed[index].enabled;
  run_task([this, location, enable]() {
    m_ctx.soundpacks.set_enabled(location, enable);
  });
}

void TabSoundpacks::on_delete_clicked() {
  int index = selected_installed();
  if (index < 0 || m_installed[index].is_stock) return;
  std::string name = m_installed[index].name;
  std::filesystem::path location = m_installed[index].location;

  // Show the folder too: two packs can have the same name.
  bool use_trash = m_ctx.settings.get_bool("use_trash");
  dialogs::confirm(m_parent, "Delete the soundpack?",
                   qstr(name + " (" + to_utf8(location.filename()) + ") " +
                        dialogs::removal_text(use_trash) + "."),
                   qstr(dialogs::removal_button(use_trash)),
                   [this, location]() {
                     run_task([this, location]() {
                       m_ctx.soundpacks.delete_pack(location);
                     });
                   });
}

void TabSoundpacks::on_install_clicked() { install_selected(""); }

void TabSoundpacks::install_selected(const std::string& from_file) {
  int index = selected_available();
  if (index < 0) return;
  SoundpackSource pack = available_soundpacks()[index];
  bool reinstall = m_ctx.soundpacks.has_user_copy(pack);
  bool keep_archive = !from_file.empty();  // Never delete the user's own file.

  std::function<void()> install = [this, pack, from_file, reinstall,
                                   keep_archive]() {
    run_task([this, pack, from_file, reinstall, keep_archive]() {
      m_ctx.soundpacks.install_pack(pack, to_path(from_file), reinstall,
                                    keep_archive);
    });
  };

  // Reinstalling replaces the installed copy.
  std::string title = (reinstall ? "Reinstall " : "Install ") + pack.name + "?";
  std::string detail =
      reinstall
          ? "The installed copy " +
                dialogs::removal_text(m_ctx.settings.get_bool("use_trash")) +
                ", and a fresh copy is installed."
          : std::string();
  std::string button = reinstall ? "Reinstall" : "Install";

  if (!from_file.empty()) {
    // Nothing to download.
    if (!reinstall)
      install();
    else
      dialogs::confirm(m_parent, qstr(title), qstr(detail), qstr(button),
                       install);
    return;
  }

  DownloadQuestion question;
  question.title = title;
  question.detail = detail;
  question.button = button;
  question.url = pack.url;
  question.filename = pack.filename;
  confirm_download(m_ctx, m_runner, m_parent, *m_content, question, install);
}

void TabSoundpacks::on_install_from_file_clicked() {
  if (selected_available() < 0) return;

  QString downloads =
      QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  QString file = QFileDialog::getOpenFileName(
      &m_parent, "Choose the soundpack archive", downloads);
  if (!file.isEmpty()) install_selected(str(file));
}
