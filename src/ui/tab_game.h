#ifndef EVAC_TAB_GAME_H
#define EVAC_TAB_GAME_H

#include <QWidget>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "core/paths.h"
#include "core/release_data.h"
#include "ui/app_context.h"
#include "ui/task_runner.h"

class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

// The Game tab: choose a game, install or update a release, launch it.
class TabGame : public QWidget {
 public:
  TabGame(AppContext& ctx, TaskRunner& runner, QWidget& parent);

  // Call once after the window exists; loads the release list.
  void start();

  // Call when settings that the tab depends on may have changed (on the
  // Settings tab).
  void apply_settings();

 private:
  void build_ui();

  // Settings -> widgets.
  void apply_game_choice();
  void reload_builds_list();
  void refresh_installed();
  void reload_userdata_list();

  // Slow jobs, run on the worker thread.
  void fetch_releases();
  void run_task(const std::function<void()>& work,
                const std::function<void()>& after);

  // Widget handlers.
  void on_game_selected();
  void on_channel_toggled();
  void on_build_selected();
  void on_install_new_clicked();
  void on_update_clicked();
  void on_play_clicked();
  void on_resume_clicked();
  void on_make_active_clicked();
  void on_delete_clicked();
  void on_installs_row_selected();
  void on_installs_row_activated(QTreeWidgetItem* row);
  void on_userdata_selected();
  void on_userdata_add_clicked();
  void on_userdata_rename_clicked();
  void on_userdata_remove_clicked();

  // Asks what to do when the active userdata folder is a custom one that is
  // gone.
  void check_missing_userdata();
  std::string userdata_label(const std::filesystem::path& path);

  // The install section folds open and closed, like an expander.
  void set_install_expanded(bool expanded);

  void install_selected(bool update_active);
  std::string removal_detail(const std::string& install_name, bool update);
  void start_game(const std::string& world);

  std::string release_key();
  std::string selected_install_name();

  AppContext& ctx_;
  TaskRunner& runner_;
  QWidget& parent_;

  bool updating_;    // True while we change widgets ourselves, so handlers do
                     // nothing.
  int had_install_;  // -1 before the first refresh, then 0 or 1. Used to
                     // open/close the install section once.
  std::vector<Release> releases_;
  Paths::InstallsSummary installs_;
  std::vector<std::string>
      install_names_;  // Same order as the rows in installs_list_.
  std::vector<std::filesystem::path>
      userdata_paths_;  // Same order as userdata_dropdown_, the default first.
  std::vector<std::string>
      userdata_labels_;  // What userdata_dropdown_ holds now.
  std::set<std::filesystem::path>
      kept_missing_;  // Missing folders the user chose to keep, not asked
                      // again.
  bool asking_missing_;

  QWidget* content_;
  QComboBox* game_dropdown_;
  QLabel* description_;

  // Userdata folder choice.
  QComboBox* userdata_dropdown_;
  QToolButton* userdata_add_button_;
  QToolButton* userdata_rename_button_;
  QToolButton* userdata_remove_button_;

  // Play section.
  QLabel* active_title_;
  QPushButton* play_button_;
  QPushButton* resume_button_;
  QPushButton* folder_menu_button_;

  // Installed versions.
  QWidget* installs_box_;
  QTreeWidget* installs_list_;
  QPushButton* make_active_button_;
  QPushButton* delete_button_;

  // Install / update section.
  QToolButton* install_expander_;
  QWidget* install_section_;
  QRadioButton* stable_button_;
  QRadioButton* experimental_button_;
  QToolButton* refresh_button_;
  QComboBox* builds_dropdown_;
  QPushButton* install_new_button_;
  QPushButton* update_button_;
};

#endif
