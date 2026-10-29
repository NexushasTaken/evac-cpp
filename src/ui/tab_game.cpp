#include "ui/tab_game.h"

#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "core/fs.h"
#include "core/games.h"
#include "ui/dialogs.h"
#include "ui/download_prompt.h"
#include "ui/qt_helpers.h"

TabGame::TabGame(AppContext& ctx, TaskRunner& runner, QWidget& parent)
    : m_ctx(ctx),
      m_runner(runner),
      m_parent(parent),
      m_updating(false),
      m_had_install(-1),
      m_asking_missing(false) {
  build_ui();
}

void TabGame::build_ui() {
  QVBoxLayout* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  m_content = new QWidget();
  outer->addWidget(m_content);
  outer->addStretch(1);
  QVBoxLayout* content = new QVBoxLayout(m_content);
  content->setContentsMargins(12, 12, 12, 12);
  content->setSpacing(12);

  // 1. Game choice and description.
  m_game_dropdown = new QComboBox();
  const std::vector<GameInfo>& all = all_games();
  for (size_t i = 0; i < all.size(); ++i)
    m_game_dropdown->addItem(qstr(all[i].name));
  m_game_dropdown->setToolTip("Choose which game to manage or play.");

  QHBoxLayout* game_row = new QHBoxLayout();
  game_row->setSpacing(8);
  game_row->addWidget(make_label("Game:"));
  game_row->addWidget(m_game_dropdown, 1);
  content->addLayout(game_row);

  m_description = new QLabel();
  m_description->setTextFormat(Qt::RichText);
  m_description->setWordWrap(true);
  m_description->setOpenExternalLinks(true);
  m_description->setTextInteractionFlags(Qt::TextBrowserInteraction);
  content->addWidget(m_description);

  // Which userdata folder the game uses (saves, config, user mods, ...).
  m_userdata_dropdown = new QComboBox();
  m_userdata_add_button = new QToolButton();
  set_icon(m_userdata_add_button, "list-add", "Add");
  m_userdata_add_button->setToolTip(
      "Add a user data folder. It can be anywhere,\nand empty: the game fills "
      "it in.");
  m_userdata_rename_button = new QToolButton();
  set_icon(m_userdata_rename_button, "document-edit", "Rename");
  m_userdata_rename_button->setToolTip(
      "Rename the selected user data folder in this list.\n(The folder itself "
      "is not renamed.)");
  m_userdata_remove_button = new QToolButton();
  set_icon(m_userdata_remove_button, "list-remove", "Remove");
  m_userdata_remove_button->setToolTip(
      "Remove the selected user data folder from this list.\n(Its files are "
      "not deleted.)");
  QHBoxLayout* userdata_row = new QHBoxLayout();
  userdata_row->setSpacing(8);
  userdata_row->addWidget(make_label("User data:"));
  userdata_row->addWidget(m_userdata_dropdown, 1);
  userdata_row->addWidget(m_userdata_add_button);
  userdata_row->addWidget(m_userdata_rename_button);
  userdata_row->addWidget(m_userdata_remove_button);
  content->addLayout(userdata_row);

  // 2. Play: the thing you do most often, so it comes first.
  QVBoxLayout* play_box = new QVBoxLayout();
  play_box->setSpacing(8);
  m_active_title = make_label("");
  QFont title_font = m_active_title->font();
  title_font.setBold(true);
  title_font.setPointSizeF(title_font.pointSizeF() * 1.4);
  m_active_title->setFont(title_font);
  play_box->addWidget(m_active_title);

  m_play_button = new QPushButton("Play");
  m_play_button->setToolTip("Launch the game.");
  m_resume_button = new QPushButton("Resume Last World");
  m_resume_button->setToolTip(
      "Skip main menu and quick-load last played world.");

  // The folder buttons are rarely needed, so they hide in a menu.
  QMenu* folder_menu = new QMenu(this);
  folder_menu->setToolTipsVisible(true);
  QAction* game_dir_action = folder_menu->addAction("Game folder");
  game_dir_action->setToolTip(
      "Open game installation directory (contains\ngame executable and "
      "built-in game content).");
  QAction* user_dir_action = folder_menu->addAction("User data folder");
  user_dir_action->setToolTip(
      "Open user data directory (contains saves, \ngame configuration, user "
      "mods, sound, etc.)");
  m_folder_menu_button = new QPushButton("Open folder");
  m_folder_menu_button->setMenu(folder_menu);
  connect(game_dir_action, &QAction::triggered, this,
          [this]() { dialogs::open_folder(m_ctx.paths.game_dir()); });
  connect(user_dir_action, &QAction::triggered, this,
          [this]() { dialogs::open_folder(m_ctx.paths.userdata()); });

  QHBoxLayout* play_row = new QHBoxLayout();
  play_row->setSpacing(8);
  play_row->addWidget(m_play_button, 1);
  play_row->addWidget(m_resume_button, 1);
  play_row->addWidget(m_folder_menu_button);
  play_box->addLayout(play_row);
  content->addLayout(play_box);

  // 3. The installs, only shown when there is more than one (or if asked for).
  m_installs_box = new QWidget();
  QVBoxLayout* installs = new QVBoxLayout(m_installs_box);
  installs->setContentsMargins(0, 0, 0, 0);
  installs->setSpacing(4);
  installs->addWidget(make_label("Installed versions:"));
  m_installs_list = new QTreeWidget();
  m_installs_list->setColumnCount(2);
  m_installs_list->setHeaderHidden(true);
  m_installs_list->setRootIsDecorated(false);
  m_installs_list->setSelectionMode(QAbstractItemView::SingleSelection);
  m_installs_list->header()->setStretchLastSection(false);
  m_installs_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_installs_list->header()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  m_installs_list->setMinimumHeight(90);
  installs->addWidget(m_installs_list);
  m_make_active_button = new QPushButton("Make active");
  m_make_active_button->setToolTip(
      "Set the selected game install as active (i.e. in use).");
  m_delete_button = new QPushButton("Delete");
  m_delete_button->setToolTip(
      "Delete the selected copy of the game.\n(Saves, settings and other user "
      "data\nare stored separately and will not be lost.)");
  QHBoxLayout* installs_buttons = new QHBoxLayout();
  installs_buttons->setSpacing(8);
  installs_buttons->addWidget(m_make_active_button, 1);
  installs_buttons->addWidget(m_delete_button, 1);
  installs->addLayout(installs_buttons);
  content->addWidget(m_installs_box);

  // 4. Install / update. A grid keeps the labels and widgets aligned.
  m_stable_button = new QRadioButton("Stable");
  m_stable_button->setToolTip("Stable release channel (DDA and BN only).");
  m_experimental_button = new QRadioButton("Experimental");
  m_experimental_button->setToolTip("Experimental release channel.");
  m_refresh_button = new QToolButton();
  set_icon(m_refresh_button, "view-refresh", "Refresh");
  m_refresh_button->setToolTip(
      "Re-retrieve the list of available game releases\nfrom GitHub (only for "
      "experimental releases).");

  QHBoxLayout* channel_box = new QHBoxLayout();
  channel_box->setSpacing(8);
  channel_box->addWidget(m_stable_button);
  channel_box->addWidget(m_experimental_button);
  channel_box->addStretch(1);
  channel_box->addWidget(m_refresh_button);

  m_builds_dropdown = new QComboBox();
  m_builds_dropdown->setToolTip(
      "Choose which release of the game to install or update to.");

  m_install_new_button = new QPushButton("Install as New");
  m_install_new_button->setToolTip(
      "Install the selected build as a new, separate copy of the game.");
  m_update_button = new QPushButton("Update Active Install");
  m_update_button->setToolTip(
      "Replace the game install currently set as active\nwith the selected "
      "build.");
  QHBoxLayout* install_buttons = new QHBoxLayout();
  install_buttons->setSpacing(8);
  install_buttons->addWidget(m_install_new_button, 1);
  install_buttons->addWidget(m_update_button, 1);

  m_install_section = new QWidget();
  QGridLayout* grid = new QGridLayout(m_install_section);
  grid->setContentsMargins(0, 8, 0, 0);
  grid->setVerticalSpacing(8);
  grid->setHorizontalSpacing(8);
  grid->addWidget(make_label("Channel:"), 0, 0);
  grid->addLayout(channel_box, 0, 1);
  grid->addWidget(make_label("Build:"), 1, 0);
  grid->addWidget(m_builds_dropdown, 1, 1);
  grid->addLayout(install_buttons, 2, 0, 1, 2);
  grid->setColumnStretch(1, 1);

  m_install_expander = new QToolButton();
  m_install_expander->setText("Install / update");
  m_install_expander->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_install_expander->setAutoRaise(true);
  m_install_expander->setCheckable(true);
  connect(m_install_expander, &QToolButton::toggled, this,
          &TabGame::set_install_expanded);
  content->addWidget(m_install_expander, 0, Qt::AlignLeft);
  content->addWidget(m_install_section);
  set_install_expanded(false);

  // Signals. These are connected after the initial state is set, see start().
}

void TabGame::set_install_expanded(bool expanded) {
  if (m_install_expander->isChecked() != expanded)
    m_install_expander->setChecked(
        expanded);  // Calls this again through the signal.
  m_install_expander->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
  m_install_section->setVisible(expanded);
}

void TabGame::start() {
  const std::vector<GameInfo>& all = all_games();
  std::string game = m_ctx.settings.get_string("game");
  for (size_t i = 0; i < all.size(); ++i) {
    if (all[i].key == game) m_game_dropdown->setCurrentIndex((int)i);
  }
  m_description->setVisible(m_ctx.settings.get_bool("show_game_desc"));

  connect(m_game_dropdown, &QComboBox::currentIndexChanged, this,
          &TabGame::on_game_selected);
  connect(m_stable_button, &QRadioButton::toggled, this,
          &TabGame::on_channel_toggled);
  connect(m_builds_dropdown, &QComboBox::currentIndexChanged, this,
          &TabGame::on_build_selected);
  connect(m_refresh_button, &QToolButton::clicked, this,
          &TabGame::fetch_releases);
  connect(m_install_new_button, &QPushButton::clicked, this,
          &TabGame::on_install_new_clicked);
  connect(m_update_button, &QPushButton::clicked, this,
          &TabGame::on_update_clicked);
  connect(m_play_button, &QPushButton::clicked, this,
          &TabGame::on_play_clicked);
  connect(m_resume_button, &QPushButton::clicked, this,
          &TabGame::on_resume_clicked);
  connect(m_make_active_button, &QPushButton::clicked, this,
          &TabGame::on_make_active_clicked);
  connect(m_delete_button, &QPushButton::clicked, this,
          &TabGame::on_delete_clicked);
  connect(m_installs_list, &QTreeWidget::itemSelectionChanged, this,
          &TabGame::on_installs_row_selected);
  connect(m_installs_list, &QTreeWidget::itemActivated, this,
          &TabGame::on_installs_row_activated);
  connect(m_userdata_dropdown, &QComboBox::currentIndexChanged, this,
          &TabGame::on_userdata_selected);
  connect(m_userdata_add_button, &QToolButton::clicked, this,
          &TabGame::on_userdata_add_clicked);
  connect(m_userdata_rename_button, &QToolButton::clicked, this,
          &TabGame::on_userdata_rename_clicked);
  connect(m_userdata_remove_button, &QToolButton::clicked, this,
          &TabGame::on_userdata_remove_clicked);

  apply_game_choice();
  // The window is not shown yet, so ask once it is.
  QTimer::singleShot(0, this, [this]() { check_missing_userdata(); });
}

void TabGame::apply_settings() {
  m_description->setVisible(m_ctx.settings.get_bool("show_game_desc"));
  refresh_installed();
  check_missing_userdata();
}

std::string TabGame::release_key() {
  return ReleaseManager::make_key(m_ctx.settings.get_string("game"),
                                  m_ctx.settings.get_string("channel"));
}

// Runs a slow job on the worker thread, with the whole tab disabled until it
// is done. `after` runs on the main thread when the job has finished.
void TabGame::run_task(const std::function<void()>& work,
                       const std::function<void()>& after) {
  m_content->setEnabled(false);
  bool started = m_runner.run(work, [this, after]() {
    m_content->setEnabled(true);
    after();
  });
  if (!started) m_content->setEnabled(true);
}

void TabGame::fetch_releases() {
  std::string key = release_key();
  run_task([this, key]() { m_ctx.releases.fetch(key); },
           [this]() { reload_builds_list(); });
}

// Makes the widgets match the settings (game and channel) and loads the builds.
void TabGame::apply_game_choice() {
  m_updating = true;

  const GameInfo& info = find_game(m_ctx.settings.get_string("game"));
  if (!info.has_stable_channel &&
      m_ctx.settings.get_string("channel") != "experimental")
    m_ctx.settings.set_string("channel", "experimental");
  std::string channel = m_ctx.settings.get_string("channel");

  m_stable_button->setEnabled(info.has_stable_channel);
  m_experimental_button->setEnabled(info.has_stable_channel);
  if (channel == "stable")
    m_stable_button->setChecked(true);
  else
    m_experimental_button->setChecked(true);
  m_refresh_button->setEnabled(channel == "experimental");

  m_description->setText(qstr(info.description));

  const std::vector<GameInfo>& all = all_games();
  for (size_t i = 0; i < all.size(); ++i) {
    if (all[i].key == info.key) m_game_dropdown->setCurrentIndex((int)i);
  }

  m_updating = false;

  if (m_ctx.releases.get(release_key()).empty())
    fetch_releases();
  else
    reload_builds_list();
}

void TabGame::reload_builds_list() {
  m_updating = true;
  m_releases = m_ctx.releases.get(release_key());

  m_builds_dropdown->clear();
  for (size_t i = 0; i < m_releases.size(); ++i)
    m_builds_dropdown->addItem(qstr(m_releases[i].name));
  m_updating = false;

  refresh_installed();
}

// Updates everything that depends on the installed games and the selected
// build.
void TabGame::refresh_installed() {
  m_updating = true;

  std::string game = m_ctx.settings.get_string("game");
  m_installs = m_ctx.paths.installs_summary();
  Paths::InstallsSummary::const_iterator g = m_installs.find(game);
  bool have_install = (g != m_installs.end());
  std::string active = m_ctx.settings.get_string("active_install_" + game);

  // The list of installs.
  m_installs_list->clear();
  m_install_names.clear();
  if (have_install) {
    QBrush dim = dim_brush(m_installs_list);
    std::map<std::string, std::filesystem::path>::const_iterator it;
    for (it = g->second.begin(); it != g->second.end(); ++it) {
      QTreeWidgetItem* row = new QTreeWidgetItem(m_installs_list);
      row->setText(0, qstr(it->first));
      if (it->first == active) {
        row->setText(1, "active");
        row->setForeground(1, dim);
      }
      QString tooltip = qstr("Location:\n" + to_utf8(it->second) +
                             "\n(double-click to open folder)");
      row->setToolTip(0, tooltip);
      row->setToolTip(1, tooltip);
      m_install_names.push_back(it->first);
    }
  }
  m_make_active_button->setEnabled(false);
  m_delete_button->setEnabled(false);
  m_installs_box->setVisible((have_install && g->second.size() > 1) ||
                             m_ctx.settings.get_bool("always_show_installs"));

  // The play section.
  m_active_title->setText(have_install ? qstr(active)
                                       : QString("No game installed yet"));
  m_active_title->setEnabled(have_install);  // Dimmed when there is nothing.
  m_folder_menu_button->setVisible(have_install);
  m_play_button->setEnabled(have_install);
  m_resume_button->setEnabled(have_install && m_ctx.launcher.has_last_world());

  // The install buttons depend on the selected build.
  int selected = m_builds_dropdown->currentIndex();
  bool build_selected = (selected >= 0 && (size_t)selected < m_releases.size());
  bool same_build_installed =
      build_selected && have_install &&
      g->second.find(m_releases[selected].name) != g->second.end() &&
      !m_ctx.settings.get_bool("update_to_same_build_allowed");

  m_install_new_button->setEnabled(build_selected && !same_build_installed);
  m_update_button->setEnabled(build_selected && have_install &&
                              !same_build_installed);

  // Open the install section when there is nothing installed, close it once
  // there is.
  if (m_had_install != (have_install ? 1 : 0)) {
    m_had_install = have_install ? 1 : 0;
    set_install_expanded(!have_install);
  }

  m_updating = false;
  reload_userdata_list();
  m_ctx.settings.save();
}

void TabGame::reload_userdata_list() {
  m_updating = true;
  std::filesystem::path active = m_ctx.paths.userdata();
  std::vector<UserdataFolder> folders = m_ctx.paths.custom_userdata();

  std::vector<std::string> labels;
  m_userdata_paths.clear();
  labels.push_back("Default");
  m_userdata_paths.push_back(m_ctx.paths.default_userdata());
  int selected = 0;
  for (size_t i = 0; i < folders.size(); ++i) {
    labels.push_back(folders[i].label);
    m_userdata_paths.push_back(folders[i].path);
    if (folders[i].path == active) selected = (int)(i + 1);
  }
  // Refilled only when the list changed. This also runs while the dropdown
  // handles a choice.
  if (labels != m_userdata_labels) {
    m_userdata_dropdown->clear();
    for (size_t i = 0; i < labels.size(); ++i)
      m_userdata_dropdown->addItem(qstr(labels[i]));
    m_userdata_labels = labels;
  }
  if (m_userdata_dropdown->currentIndex() != selected)
    m_userdata_dropdown->setCurrentIndex(selected);
  m_userdata_dropdown->setToolTip(
      qstr("The user data folder the game uses (saves, configuration,\nuser "
           "mods, sound, etc.):\n" +
           to_utf8(active)));

  bool custom = (selected != 0);
  m_userdata_rename_button->setEnabled(custom);
  m_userdata_remove_button->setEnabled(custom);
  m_updating = false;
}

std::string TabGame::userdata_label(const std::filesystem::path& path) {
  std::vector<UserdataFolder> folders = m_ctx.paths.custom_userdata();
  for (size_t i = 0; i < folders.size(); ++i) {
    if (folders[i].path == path) return folders[i].label;
  }
  return "Default";
}

void TabGame::on_userdata_selected() {
  if (m_updating) return;
  int index = m_userdata_dropdown->currentIndex();
  if (index < 0 || (size_t)index >= m_userdata_paths.size()) return;
  // A job on another tab may be working in the current userdata.
  if (m_runner.busy()) {
    m_ctx.status.post(
        "Wait for the current task to finish before changing the user data "
        "folder.",
        MSG_WARN);
    reload_userdata_list();
    return;
  }
  std::filesystem::path path = m_userdata_paths[index];
  m_ctx.paths.set_active_userdata(path);
  m_ctx.status.post("Using the user data folder \"" + userdata_label(path) +
                    "\": " + to_utf8(path));
  refresh_installed();
  check_missing_userdata();
}

void TabGame::on_userdata_add_clicked() {
  if (m_runner.busy()) {
    m_ctx.status.post(
        "Wait for the current task to finish before changing the user data "
        "folder.",
        MSG_WARN);
    return;
  }
  QString dir =
      QFileDialog::getExistingDirectory(&m_parent, "Choose a user data folder");
  if (dir.isEmpty()) return;
  std::filesystem::path path = to_path(str(dir));
  dialogs::ask_text(
      m_parent, "Name the user data folder",
      qstr("The name shown in the list for " + to_utf8(path) + "."),
      qstr(path.filename()), "Add",
      [this](const std::string& label) {
        return m_ctx.paths.check_userdata_label(label);
      },
      [this, path](const std::string& label) {
        std::string problem = m_ctx.paths.add_userdata(path, label);
        if (!problem.empty()) {
          dialogs::tell(m_parent, "Cannot add the user data folder",
                        qstr(problem));
          return;
        }
        // Added to be used, so it becomes the active one.
        std::vector<UserdataFolder> folders = m_ctx.paths.custom_userdata();
        m_ctx.paths.set_active_userdata(folders.back().path);
        m_ctx.status.post("Using the user data folder \"" + label +
                          "\": " + to_utf8(folders.back().path));
        refresh_installed();
      });
}

void TabGame::on_userdata_rename_clicked() {
  std::filesystem::path path = m_ctx.paths.userdata();
  if (m_ctx.paths.is_default_userdata_active()) return;
  dialogs::ask_text(
      m_parent, "Rename the user data folder",
      qstr("Only the name in this list changes. The folder stays " +
           to_utf8(path) + "."),
      qstr(userdata_label(path)), "Rename",
      [this, path](const std::string& label) {
        return m_ctx.paths.check_userdata_label(label, path);
      },
      [this, path](const std::string& label) {
        std::string problem = m_ctx.paths.rename_userdata(path, label);
        if (!problem.empty())
          dialogs::tell(m_parent, "Cannot rename the user data folder",
                        qstr(problem));
        refresh_installed();
      });
}

void TabGame::on_userdata_remove_clicked() {
  std::filesystem::path path = m_ctx.paths.userdata();
  if (m_ctx.paths.is_default_userdata_active()) return;
  if (m_runner.busy()) {
    m_ctx.status.post(
        "Wait for the current task to finish before changing the user data "
        "folder.",
        MSG_WARN);
    return;
  }
  dialogs::confirm(
      m_parent, qstr("Remove \"" + userdata_label(path) + "\" from the list?"),
      qstr("The launcher forgets this folder and uses the default one. Nothing "
           "is deleted: the folder " +
           to_utf8(path) +
           " and its files stay where they are, and you can add it again "
           "later."),
      "Remove", [this, path]() {
        m_ctx.paths.remove_userdata(path);
        m_ctx.status.post(
            "Removed " + to_utf8(path) +
            " from the user data folders. Using the default one.");
        refresh_installed();
      });
}

// The game makes the folder again when it starts, so keeping it is fine.
void TabGame::check_missing_userdata() {
  if (m_asking_missing || m_ctx.paths.is_default_userdata_active()) return;
  std::filesystem::path path = m_ctx.paths.userdata();
  std::error_code ec;
  if (std::filesystem::is_directory(path, ec) || m_kept_missing.count(path))
    return;

  m_asking_missing = true;
  QStringList buttons;
  buttons << "Keep" << "Remove from List";
  dialogs::ask(
      m_parent,
      qstr("The user data folder \"" + userdata_label(path) + "\" is missing"),
      qstr(to_utf8(path) +
           " does not exist.\n\nKeep it, and the game creates and fills it in "
           "the next time it starts. "
           "Or remove it from the list and use the default user data folder."),
      buttons, [this, path](int choice) {
        m_asking_missing = false;
        if (choice == 1) {
          m_ctx.paths.remove_userdata(path);
          m_ctx.status.post(
              "Removed " + to_utf8(path) +
              " from the user data folders. Using the default one.");
        } else {
          m_kept_missing.insert(path);
        }
        refresh_installed();
      });
}

std::string TabGame::selected_install_name() {
  int row = selected_row(m_installs_list);
  if (row < 0 || (size_t)row >= m_install_names.size()) return "";
  return m_install_names[row];
}

void TabGame::on_game_selected() {
  if (m_updating) return;
  int index = m_game_dropdown->currentIndex();
  const std::vector<GameInfo>& all = all_games();
  if (index < 0 || (size_t)index >= all.size()) return;

  m_ctx.settings.set_string("game", all[index].key);
  m_ctx.settings.save();
  apply_game_choice();
  check_missing_userdata();
}

void TabGame::on_channel_toggled() {
  if (m_updating) return;
  m_ctx.settings.set_string(
      "channel", m_stable_button->isChecked() ? "stable" : "experimental");
  m_ctx.settings.save();
  apply_game_choice();
}

void TabGame::on_build_selected() {
  if (m_updating) return;
  refresh_installed();
}

void TabGame::on_installs_row_selected() {
  if (m_updating) return;
  std::string name = selected_install_name();
  m_delete_button->setEnabled(!name.empty());
  m_make_active_button->setEnabled(
      !name.empty() &&
      name != m_ctx.settings.get_string("active_install_" +
                                        m_ctx.settings.get_string("game")));
}

void TabGame::on_installs_row_activated(QTreeWidgetItem* row) {
  int index = m_installs_list->indexOfTopLevelItem(row);
  if (index < 0 || (size_t)index >= m_install_names.size()) return;
  Paths::InstallsSummary::const_iterator g =
      m_installs.find(m_ctx.settings.get_string("game"));
  if (g == m_installs.end()) return;
  std::map<std::string, std::filesystem::path>::const_iterator it =
      g->second.find(m_install_names[index]);
  if (it != g->second.end()) dialogs::open_folder(it->second);
}

void TabGame::on_make_active_clicked() {
  std::string name = selected_install_name();
  if (name.empty()) return;
  m_ctx.status.post("Setting active install to " + name + ".");
  m_ctx.settings.set_string(
      "active_install_" + m_ctx.settings.get_string("game"), name);
  refresh_installed();
}

void TabGame::on_install_new_clicked() { install_selected(false); }

void TabGame::on_update_clicked() { install_selected(true); }

void TabGame::install_selected(bool update_active) {
  int selected = m_builds_dropdown->currentIndex();
  if (selected < 0 || (size_t)selected >= m_releases.size()) return;
  Release release = m_releases[selected];

  std::filesystem::path update_path;
  std::string active_name;
  if (update_active) {
    std::string game = m_ctx.settings.get_string("game");
    active_name = m_ctx.settings.get_string("active_install_" + game);
    Paths::InstallsSummary::const_iterator g = m_installs.find(game);
    if (g != m_installs.end() && g->second.find(active_name) != g->second.end())
      update_path = g->second.find(active_name)->second;
  }

  DownloadQuestion question;
  question.url = release.url;
  question.filename = release.filename;
  if (update_path.empty()) {
    question.title = "Install " + release.name + "?";
    question.detail = "It is installed as a new, separate copy of the game.";
    question.button = "Install";
  } else {
    question.title = "Update \"" + active_name + "\"?";
    question.detail = removal_detail(active_name, true);
    question.button = "Update";
  }

  confirm_download(m_ctx, m_runner, m_parent, *m_content, question,
                   [this, release, update_path]() {
                     run_task(
                         [this, release, update_path]() {
                           m_ctx.installer.install_release(release,
                                                           update_path);
                         },
                         [this]() { refresh_installed(); });
                   });
}

void TabGame::on_delete_clicked() {
  std::string name = selected_install_name();
  if (name.empty()) return;

  dialogs::confirm(
      m_parent, qstr("Uninstall \"" + name + "\"?"),
      qstr(removal_detail(name, false)),
      qstr(dialogs::removal_button(m_ctx.settings.get_bool("use_trash"))),
      [this, name]() {
        run_task(
            [this, name]() { m_ctx.installer.remove_release_by_name(name); },
            [this]() { refresh_installed(); });
      });
}

// What happens to an install's files when it is updated or uninstalled, for
// the confirmation. Updating replaces them and uninstalling removes them, so
// it also says where user data inside the game directory goes.
std::string TabGame::removal_detail(const std::string& install_name,
                                    bool update) {
  bool use_trash = m_ctx.settings.get_bool("use_trash");
  std::string detail =
      update ? "The current files of \"" + install_name + "\" " +
                   dialogs::removal_text(use_trash) +
                   ", and the new version is installed in their place."
             : "The game files of \"" + install_name + "\" " +
                   dialogs::removal_text(use_trash) + ".";

  std::vector<std::filesystem::path> misplaced =
      m_ctx.installer.check_game_dir_for_userdata(install_name);
  if (!misplaced.empty()) {
    detail +=
        "\n\nThe game directory also contains user data that is not in the "
        "user data directory:\n";
    for (size_t i = 0; i < misplaced.size(); ++i)
      detail += "\n" + to_utf8(misplaced[i]);
    detail += use_trash ? "\n\nIt goes to the trash with the game files."
                        : "\n\nIt will be lost.";
  }
  return detail;
}

void TabGame::on_play_clicked() { start_game(""); }

void TabGame::on_resume_clicked() {
  std::string world = m_ctx.launcher.last_world();
  if (!world.empty()) start_game(world);
}

void TabGame::start_game(const std::string& world) {
  if (!m_ctx.launcher.start(world)) return;
  if (!m_ctx.settings.get_bool("keep_open_after_starting_game"))
    m_parent.close();
}
