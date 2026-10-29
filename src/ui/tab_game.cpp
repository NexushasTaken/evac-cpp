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
    : ctx_(ctx),
      runner_(runner),
      parent_(parent),
      updating_(false),
      had_install_(-1),
      asking_missing_(false) {
  build_ui();
}

void TabGame::build_ui() {
  QVBoxLayout* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  content_ = new QWidget();
  outer->addWidget(content_);
  outer->addStretch(1);
  QVBoxLayout* content = new QVBoxLayout(content_);
  content->setContentsMargins(12, 12, 12, 12);
  content->setSpacing(12);

  // 1. Game choice and description.
  game_dropdown_ = new QComboBox();
  const std::vector<GameInfo>& all = all_games();
  for (size_t i = 0; i < all.size(); ++i)
    game_dropdown_->addItem(qstr(all[i].name));
  game_dropdown_->setToolTip("Choose which game to manage or play.");

  QHBoxLayout* game_row = new QHBoxLayout();
  game_row->setSpacing(8);
  game_row->addWidget(make_label("Game:"));
  game_row->addWidget(game_dropdown_, 1);
  content->addLayout(game_row);

  description_ = new QLabel();
  description_->setTextFormat(Qt::RichText);
  description_->setWordWrap(true);
  description_->setOpenExternalLinks(true);
  description_->setTextInteractionFlags(Qt::TextBrowserInteraction);
  content->addWidget(description_);

  // Which userdata folder the game uses (saves, config, user mods, ...).
  userdata_dropdown_ = new QComboBox();
  userdata_add_button_ = new QToolButton();
  set_icon(userdata_add_button_, "list-add", "Add");
  userdata_add_button_->setToolTip(
      "Add a user data folder. It can be anywhere,\nand empty: the game fills "
      "it in.");
  userdata_rename_button_ = new QToolButton();
  set_icon(userdata_rename_button_, "document-edit", "Rename");
  userdata_rename_button_->setToolTip(
      "Rename the selected user data folder in this list.\n(The folder itself "
      "is not renamed.)");
  userdata_remove_button_ = new QToolButton();
  set_icon(userdata_remove_button_, "list-remove", "Remove");
  userdata_remove_button_->setToolTip(
      "Remove the selected user data folder from this list.\n(Its files are "
      "not deleted.)");
  QHBoxLayout* userdata_row = new QHBoxLayout();
  userdata_row->setSpacing(8);
  userdata_row->addWidget(make_label("User data:"));
  userdata_row->addWidget(userdata_dropdown_, 1);
  userdata_row->addWidget(userdata_add_button_);
  userdata_row->addWidget(userdata_rename_button_);
  userdata_row->addWidget(userdata_remove_button_);
  content->addLayout(userdata_row);

  // 2. Play: the thing you do most often, so it comes first.
  QVBoxLayout* play_box = new QVBoxLayout();
  play_box->setSpacing(8);
  active_title_ = make_label("");
  QFont title_font = active_title_->font();
  title_font.setBold(true);
  title_font.setPointSizeF(title_font.pointSizeF() * 1.4);
  active_title_->setFont(title_font);
  play_box->addWidget(active_title_);

  play_button_ = new QPushButton("Play");
  play_button_->setToolTip("Launch the game.");
  resume_button_ = new QPushButton("Resume Last World");
  resume_button_->setToolTip(
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
  folder_menu_button_ = new QPushButton("Open folder");
  folder_menu_button_->setMenu(folder_menu);
  connect(game_dir_action, &QAction::triggered, this,
          [this]() { dialogs::open_folder(ctx_.paths.game_dir()); });
  connect(user_dir_action, &QAction::triggered, this,
          [this]() { dialogs::open_folder(ctx_.paths.userdata()); });

  QHBoxLayout* play_row = new QHBoxLayout();
  play_row->setSpacing(8);
  play_row->addWidget(play_button_, 1);
  play_row->addWidget(resume_button_, 1);
  play_row->addWidget(folder_menu_button_);
  play_box->addLayout(play_row);
  content->addLayout(play_box);

  // 3. The installs, only shown when there is more than one (or if asked for).
  installs_box_ = new QWidget();
  QVBoxLayout* installs = new QVBoxLayout(installs_box_);
  installs->setContentsMargins(0, 0, 0, 0);
  installs->setSpacing(4);
  installs->addWidget(make_label("Installed versions:"));
  installs_list_ = new QTreeWidget();
  installs_list_->setColumnCount(2);
  installs_list_->setHeaderHidden(true);
  installs_list_->setRootIsDecorated(false);
  installs_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  installs_list_->header()->setStretchLastSection(false);
  installs_list_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  installs_list_->header()->setSectionResizeMode(1,
                                                 QHeaderView::ResizeToContents);
  installs_list_->setMinimumHeight(90);
  installs->addWidget(installs_list_);
  make_active_button_ = new QPushButton("Make active");
  make_active_button_->setToolTip(
      "Set the selected game install as active (i.e. in use).");
  delete_button_ = new QPushButton("Delete");
  delete_button_->setToolTip(
      "Delete the selected copy of the game.\n(Saves, settings and other user "
      "data\nare stored separately and will not be lost.)");
  QHBoxLayout* installs_buttons = new QHBoxLayout();
  installs_buttons->setSpacing(8);
  installs_buttons->addWidget(make_active_button_, 1);
  installs_buttons->addWidget(delete_button_, 1);
  installs->addLayout(installs_buttons);
  content->addWidget(installs_box_);

  // 4. Install / update. A grid keeps the labels and widgets aligned.
  stable_button_ = new QRadioButton("Stable");
  stable_button_->setToolTip("Stable release channel (DDA and BN only).");
  experimental_button_ = new QRadioButton("Experimental");
  experimental_button_->setToolTip("Experimental release channel.");
  refresh_button_ = new QToolButton();
  set_icon(refresh_button_, "view-refresh", "Refresh");
  refresh_button_->setToolTip(
      "Re-retrieve the list of available game releases\nfrom GitHub (only for "
      "experimental releases).");

  QHBoxLayout* channel_box = new QHBoxLayout();
  channel_box->setSpacing(8);
  channel_box->addWidget(stable_button_);
  channel_box->addWidget(experimental_button_);
  channel_box->addStretch(1);
  channel_box->addWidget(refresh_button_);

  builds_dropdown_ = new QComboBox();
  builds_dropdown_->setToolTip(
      "Choose which release of the game to install or update to.");

  install_new_button_ = new QPushButton("Install as New");
  install_new_button_->setToolTip(
      "Install the selected build as a new, separate copy of the game.");
  update_button_ = new QPushButton("Update Active Install");
  update_button_->setToolTip(
      "Replace the game install currently set as active\nwith the selected "
      "build.");
  QHBoxLayout* install_buttons = new QHBoxLayout();
  install_buttons->setSpacing(8);
  install_buttons->addWidget(install_new_button_, 1);
  install_buttons->addWidget(update_button_, 1);

  install_section_ = new QWidget();
  QGridLayout* grid = new QGridLayout(install_section_);
  grid->setContentsMargins(0, 8, 0, 0);
  grid->setVerticalSpacing(8);
  grid->setHorizontalSpacing(8);
  grid->addWidget(make_label("Channel:"), 0, 0);
  grid->addLayout(channel_box, 0, 1);
  grid->addWidget(make_label("Build:"), 1, 0);
  grid->addWidget(builds_dropdown_, 1, 1);
  grid->addLayout(install_buttons, 2, 0, 1, 2);
  grid->setColumnStretch(1, 1);

  install_expander_ = new QToolButton();
  install_expander_->setText("Install / update");
  install_expander_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  install_expander_->setAutoRaise(true);
  install_expander_->setCheckable(true);
  connect(install_expander_, &QToolButton::toggled, this,
          &TabGame::set_install_expanded);
  content->addWidget(install_expander_, 0, Qt::AlignLeft);
  content->addWidget(install_section_);
  set_install_expanded(false);

  // Signals. These are connected after the initial state is set, see start().
}

void TabGame::set_install_expanded(bool expanded) {
  if (install_expander_->isChecked() != expanded)
    install_expander_->setChecked(
        expanded);  // Calls this again through the signal.
  install_expander_->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
  install_section_->setVisible(expanded);
}

void TabGame::start() {
  const std::vector<GameInfo>& all = all_games();
  std::string game = ctx_.settings.get_string("game");
  for (size_t i = 0; i < all.size(); ++i) {
    if (all[i].key == game) game_dropdown_->setCurrentIndex((int)i);
  }
  description_->setVisible(ctx_.settings.get_bool("show_game_desc"));

  connect(game_dropdown_, &QComboBox::currentIndexChanged, this,
          &TabGame::on_game_selected);
  connect(stable_button_, &QRadioButton::toggled, this,
          &TabGame::on_channel_toggled);
  connect(builds_dropdown_, &QComboBox::currentIndexChanged, this,
          &TabGame::on_build_selected);
  connect(refresh_button_, &QToolButton::clicked, this,
          &TabGame::fetch_releases);
  connect(install_new_button_, &QPushButton::clicked, this,
          &TabGame::on_install_new_clicked);
  connect(update_button_, &QPushButton::clicked, this,
          &TabGame::on_update_clicked);
  connect(play_button_, &QPushButton::clicked, this, &TabGame::on_play_clicked);
  connect(resume_button_, &QPushButton::clicked, this,
          &TabGame::on_resume_clicked);
  connect(make_active_button_, &QPushButton::clicked, this,
          &TabGame::on_make_active_clicked);
  connect(delete_button_, &QPushButton::clicked, this,
          &TabGame::on_delete_clicked);
  connect(installs_list_, &QTreeWidget::itemSelectionChanged, this,
          &TabGame::on_installs_row_selected);
  connect(installs_list_, &QTreeWidget::itemActivated, this,
          &TabGame::on_installs_row_activated);
  connect(userdata_dropdown_, &QComboBox::currentIndexChanged, this,
          &TabGame::on_userdata_selected);
  connect(userdata_add_button_, &QToolButton::clicked, this,
          &TabGame::on_userdata_add_clicked);
  connect(userdata_rename_button_, &QToolButton::clicked, this,
          &TabGame::on_userdata_rename_clicked);
  connect(userdata_remove_button_, &QToolButton::clicked, this,
          &TabGame::on_userdata_remove_clicked);

  apply_game_choice();
  // The window is not shown yet, so ask once it is.
  QTimer::singleShot(0, this, [this]() { check_missing_userdata(); });
}

void TabGame::apply_settings() {
  description_->setVisible(ctx_.settings.get_bool("show_game_desc"));
  refresh_installed();
  check_missing_userdata();
}

std::string TabGame::release_key() {
  return ReleaseManager::make_key(ctx_.settings.get_string("game"),
                                  ctx_.settings.get_string("channel"));
}

// Runs a slow job on the worker thread, with the whole tab disabled until it
// is done. `after` runs on the main thread when the job has finished.
void TabGame::run_task(const std::function<void()>& work,
                       const std::function<void()>& after) {
  content_->setEnabled(false);
  bool started = runner_.run(work, [this, after]() {
    content_->setEnabled(true);
    after();
  });
  if (!started) content_->setEnabled(true);
}

void TabGame::fetch_releases() {
  std::string key = release_key();
  run_task([this, key]() { ctx_.releases.fetch(key); },
           [this]() { reload_builds_list(); });
}

// Makes the widgets match the settings (game and channel) and loads the builds.
void TabGame::apply_game_choice() {
  updating_ = true;

  const GameInfo& info = find_game(ctx_.settings.get_string("game"));
  if (!info.has_stable_channel &&
      ctx_.settings.get_string("channel") != "experimental")
    ctx_.settings.set_string("channel", "experimental");
  std::string channel = ctx_.settings.get_string("channel");

  stable_button_->setEnabled(info.has_stable_channel);
  experimental_button_->setEnabled(info.has_stable_channel);
  if (channel == "stable")
    stable_button_->setChecked(true);
  else
    experimental_button_->setChecked(true);
  refresh_button_->setEnabled(channel == "experimental");

  description_->setText(qstr(info.description));

  const std::vector<GameInfo>& all = all_games();
  for (size_t i = 0; i < all.size(); ++i) {
    if (all[i].key == info.key) game_dropdown_->setCurrentIndex((int)i);
  }

  updating_ = false;

  if (ctx_.releases.get(release_key()).empty())
    fetch_releases();
  else
    reload_builds_list();
}

void TabGame::reload_builds_list() {
  updating_ = true;
  releases_ = ctx_.releases.get(release_key());

  builds_dropdown_->clear();
  for (size_t i = 0; i < releases_.size(); ++i)
    builds_dropdown_->addItem(qstr(releases_[i].name));
  updating_ = false;

  refresh_installed();
}

// Updates everything that depends on the installed games and the selected
// build.
void TabGame::refresh_installed() {
  updating_ = true;

  std::string game = ctx_.settings.get_string("game");
  installs_ = ctx_.paths.installs_summary();
  Paths::InstallsSummary::const_iterator g = installs_.find(game);
  bool have_install = (g != installs_.end());
  std::string active = ctx_.settings.get_string("active_install_" + game);

  // The list of installs.
  installs_list_->clear();
  install_names_.clear();
  if (have_install) {
    QBrush dim = dim_brush(installs_list_);
    std::map<std::string, std::filesystem::path>::const_iterator it;
    for (it = g->second.begin(); it != g->second.end(); ++it) {
      QTreeWidgetItem* row = new QTreeWidgetItem(installs_list_);
      row->setText(0, qstr(it->first));
      if (it->first == active) {
        row->setText(1, "active");
        row->setForeground(1, dim);
      }
      QString tooltip = qstr("Location:\n" + to_utf8(it->second) +
                             "\n(double-click to open folder)");
      row->setToolTip(0, tooltip);
      row->setToolTip(1, tooltip);
      install_names_.push_back(it->first);
    }
  }
  make_active_button_->setEnabled(false);
  delete_button_->setEnabled(false);
  installs_box_->setVisible((have_install && g->second.size() > 1) ||
                            ctx_.settings.get_bool("always_show_installs"));

  // The play section.
  active_title_->setText(have_install ? qstr(active)
                                      : QString("No game installed yet"));
  active_title_->setEnabled(have_install);  // Dimmed when there is nothing.
  folder_menu_button_->setVisible(have_install);
  play_button_->setEnabled(have_install);
  resume_button_->setEnabled(have_install && ctx_.launcher.has_last_world());

  // The install buttons depend on the selected build.
  int selected = builds_dropdown_->currentIndex();
  bool build_selected = (selected >= 0 && (size_t)selected < releases_.size());
  bool same_build_installed =
      build_selected && have_install &&
      g->second.find(releases_[selected].name) != g->second.end() &&
      !ctx_.settings.get_bool("update_to_same_build_allowed");

  install_new_button_->setEnabled(build_selected && !same_build_installed);
  update_button_->setEnabled(build_selected && have_install &&
                             !same_build_installed);

  // Open the install section when there is nothing installed, close it once
  // there is.
  if (had_install_ != (have_install ? 1 : 0)) {
    had_install_ = have_install ? 1 : 0;
    set_install_expanded(!have_install);
  }

  updating_ = false;
  reload_userdata_list();
  ctx_.settings.save();
}

void TabGame::reload_userdata_list() {
  updating_ = true;
  std::filesystem::path active = ctx_.paths.userdata();
  std::vector<UserdataFolder> folders = ctx_.paths.custom_userdata();

  std::vector<std::string> labels;
  userdata_paths_.clear();
  labels.push_back("Default");
  userdata_paths_.push_back(ctx_.paths.default_userdata());
  int selected = 0;
  for (size_t i = 0; i < folders.size(); ++i) {
    labels.push_back(folders[i].label);
    userdata_paths_.push_back(folders[i].path);
    if (folders[i].path == active) selected = (int)(i + 1);
  }
  // Refilled only when the list changed. This also runs while the dropdown
  // handles a choice.
  if (labels != userdata_labels_) {
    userdata_dropdown_->clear();
    for (size_t i = 0; i < labels.size(); ++i)
      userdata_dropdown_->addItem(qstr(labels[i]));
    userdata_labels_ = labels;
  }
  if (userdata_dropdown_->currentIndex() != selected)
    userdata_dropdown_->setCurrentIndex(selected);
  userdata_dropdown_->setToolTip(
      qstr("The user data folder the game uses (saves, configuration,\nuser "
           "mods, sound, etc.):\n" +
           to_utf8(active)));

  bool custom = (selected != 0);
  userdata_rename_button_->setEnabled(custom);
  userdata_remove_button_->setEnabled(custom);
  updating_ = false;
}

std::string TabGame::userdata_label(const std::filesystem::path& path) {
  std::vector<UserdataFolder> folders = ctx_.paths.custom_userdata();
  for (size_t i = 0; i < folders.size(); ++i) {
    if (folders[i].path == path) return folders[i].label;
  }
  return "Default";
}

void TabGame::on_userdata_selected() {
  if (updating_) return;
  int index = userdata_dropdown_->currentIndex();
  if (index < 0 || (size_t)index >= userdata_paths_.size()) return;
  // A job on another tab may be working in the current userdata.
  if (runner_.busy()) {
    ctx_.status.post(
        "Wait for the current task to finish before changing the user data "
        "folder.",
        MSG_WARN);
    reload_userdata_list();
    return;
  }
  std::filesystem::path path = userdata_paths_[index];
  ctx_.paths.set_active_userdata(path);
  ctx_.status.post("Using the user data folder \"" + userdata_label(path) +
                   "\": " + to_utf8(path));
  refresh_installed();
  check_missing_userdata();
}

void TabGame::on_userdata_add_clicked() {
  if (runner_.busy()) {
    ctx_.status.post(
        "Wait for the current task to finish before changing the user data "
        "folder.",
        MSG_WARN);
    return;
  }
  QString dir =
      QFileDialog::getExistingDirectory(&parent_, "Choose a user data folder");
  if (dir.isEmpty()) return;
  std::filesystem::path path = to_path(str(dir));
  dialogs::ask_text(
      parent_, "Name the user data folder",
      qstr("The name shown in the list for " + to_utf8(path) + "."),
      qstr(path.filename()), "Add",
      [this](const std::string& label) {
        return ctx_.paths.check_userdata_label(label);
      },
      [this, path](const std::string& label) {
        std::string problem = ctx_.paths.add_userdata(path, label);
        if (!problem.empty()) {
          dialogs::tell(parent_, "Cannot add the user data folder",
                        qstr(problem));
          return;
        }
        // Added to be used, so it becomes the active one.
        std::vector<UserdataFolder> folders = ctx_.paths.custom_userdata();
        ctx_.paths.set_active_userdata(folders.back().path);
        ctx_.status.post("Using the user data folder \"" + label +
                         "\": " + to_utf8(folders.back().path));
        refresh_installed();
      });
}

void TabGame::on_userdata_rename_clicked() {
  std::filesystem::path path = ctx_.paths.userdata();
  if (ctx_.paths.is_default_userdata_active()) return;
  dialogs::ask_text(
      parent_, "Rename the user data folder",
      qstr("Only the name in this list changes. The folder stays " +
           to_utf8(path) + "."),
      qstr(userdata_label(path)), "Rename",
      [this, path](const std::string& label) {
        return ctx_.paths.check_userdata_label(label, path);
      },
      [this, path](const std::string& label) {
        std::string problem = ctx_.paths.rename_userdata(path, label);
        if (!problem.empty())
          dialogs::tell(parent_, "Cannot rename the user data folder",
                        qstr(problem));
        refresh_installed();
      });
}

void TabGame::on_userdata_remove_clicked() {
  std::filesystem::path path = ctx_.paths.userdata();
  if (ctx_.paths.is_default_userdata_active()) return;
  if (runner_.busy()) {
    ctx_.status.post(
        "Wait for the current task to finish before changing the user data "
        "folder.",
        MSG_WARN);
    return;
  }
  dialogs::confirm(
      parent_, qstr("Remove \"" + userdata_label(path) + "\" from the list?"),
      qstr("The launcher forgets this folder and uses the default one. Nothing "
           "is deleted: the folder " +
           to_utf8(path) +
           " and its files stay where they are, and you can add it again "
           "later."),
      "Remove", [this, path]() {
        ctx_.paths.remove_userdata(path);
        ctx_.status.post("Removed " + to_utf8(path) +
                         " from the user data folders. Using the default one.");
        refresh_installed();
      });
}

// The game makes the folder again when it starts, so keeping it is fine.
void TabGame::check_missing_userdata() {
  if (asking_missing_ || ctx_.paths.is_default_userdata_active()) return;
  std::filesystem::path path = ctx_.paths.userdata();
  std::error_code ec;
  if (std::filesystem::is_directory(path, ec) || kept_missing_.count(path))
    return;

  asking_missing_ = true;
  QStringList buttons;
  buttons << "Keep" << "Remove from List";
  dialogs::ask(
      parent_,
      qstr("The user data folder \"" + userdata_label(path) + "\" is missing"),
      qstr(to_utf8(path) +
           " does not exist.\n\nKeep it, and the game creates and fills it in "
           "the next time it starts. "
           "Or remove it from the list and use the default user data folder."),
      buttons, [this, path](int choice) {
        asking_missing_ = false;
        if (choice == 1) {
          ctx_.paths.remove_userdata(path);
          ctx_.status.post(
              "Removed " + to_utf8(path) +
              " from the user data folders. Using the default one.");
        } else {
          kept_missing_.insert(path);
        }
        refresh_installed();
      });
}

std::string TabGame::selected_install_name() {
  int row = selected_row(installs_list_);
  if (row < 0 || (size_t)row >= install_names_.size()) return "";
  return install_names_[row];
}

void TabGame::on_game_selected() {
  if (updating_) return;
  int index = game_dropdown_->currentIndex();
  const std::vector<GameInfo>& all = all_games();
  if (index < 0 || (size_t)index >= all.size()) return;

  ctx_.settings.set_string("game", all[index].key);
  ctx_.settings.save();
  apply_game_choice();
  check_missing_userdata();
}

void TabGame::on_channel_toggled() {
  if (updating_) return;
  ctx_.settings.set_string(
      "channel", stable_button_->isChecked() ? "stable" : "experimental");
  ctx_.settings.save();
  apply_game_choice();
}

void TabGame::on_build_selected() {
  if (updating_) return;
  refresh_installed();
}

void TabGame::on_installs_row_selected() {
  if (updating_) return;
  std::string name = selected_install_name();
  delete_button_->setEnabled(!name.empty());
  make_active_button_->setEnabled(
      !name.empty() &&
      name != ctx_.settings.get_string("active_install_" +
                                       ctx_.settings.get_string("game")));
}

void TabGame::on_installs_row_activated(QTreeWidgetItem* row) {
  int index = installs_list_->indexOfTopLevelItem(row);
  if (index < 0 || (size_t)index >= install_names_.size()) return;
  Paths::InstallsSummary::const_iterator g =
      installs_.find(ctx_.settings.get_string("game"));
  if (g == installs_.end()) return;
  std::map<std::string, std::filesystem::path>::const_iterator it =
      g->second.find(install_names_[index]);
  if (it != g->second.end()) dialogs::open_folder(it->second);
}

void TabGame::on_make_active_clicked() {
  std::string name = selected_install_name();
  if (name.empty()) return;
  ctx_.status.post("Setting active install to " + name + ".");
  ctx_.settings.set_string("active_install_" + ctx_.settings.get_string("game"),
                           name);
  refresh_installed();
}

void TabGame::on_install_new_clicked() { install_selected(false); }

void TabGame::on_update_clicked() { install_selected(true); }

void TabGame::install_selected(bool update_active) {
  int selected = builds_dropdown_->currentIndex();
  if (selected < 0 || (size_t)selected >= releases_.size()) return;
  Release release = releases_[selected];

  std::filesystem::path update_path;
  std::string active_name;
  if (update_active) {
    std::string game = ctx_.settings.get_string("game");
    active_name = ctx_.settings.get_string("active_install_" + game);
    Paths::InstallsSummary::const_iterator g = installs_.find(game);
    if (g != installs_.end() && g->second.find(active_name) != g->second.end())
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

  confirm_download(ctx_, runner_, parent_, *content_, question,
                   [this, release, update_path]() {
                     run_task(
                         [this, release, update_path]() {
                           ctx_.installer.install_release(release, update_path);
                         },
                         [this]() { refresh_installed(); });
                   });
}

void TabGame::on_delete_clicked() {
  std::string name = selected_install_name();
  if (name.empty()) return;

  dialogs::confirm(
      parent_, qstr("Uninstall \"" + name + "\"?"),
      qstr(removal_detail(name, false)),
      qstr(dialogs::removal_button(ctx_.settings.get_bool("use_trash"))),
      [this, name]() {
        run_task(
            [this, name]() { ctx_.installer.remove_release_by_name(name); },
            [this]() { refresh_installed(); });
      });
}

// What happens to an install's files when it is updated or uninstalled, for
// the confirmation. Updating replaces them and uninstalling removes them, so
// it also says where user data inside the game directory goes.
std::string TabGame::removal_detail(const std::string& install_name,
                                    bool update) {
  bool use_trash = ctx_.settings.get_bool("use_trash");
  std::string detail =
      update ? "The current files of \"" + install_name + "\" " +
                   dialogs::removal_text(use_trash) +
                   ", and the new version is installed in their place."
             : "The game files of \"" + install_name + "\" " +
                   dialogs::removal_text(use_trash) + ".";

  std::vector<std::filesystem::path> misplaced =
      ctx_.installer.check_game_dir_for_userdata(install_name);
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
  std::string world = ctx_.launcher.last_world();
  if (!world.empty()) start_game(world);
}

void TabGame::start_game(const std::string& world) {
  if (!ctx_.launcher.start(world)) return;
  if (!ctx_.settings.get_bool("keep_open_after_starting_game")) parent_.close();
}
