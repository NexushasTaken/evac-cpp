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
  void extracted();
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

  AppContext& m_ctx;
  TaskRunner& m_runner;
  QWidget& m_parent;

  bool m_updating;    // True while we change widgets ourselves, so handlers do
                      // nothing.
  int m_had_install;  // -1 before the first refresh, then 0 or 1. Used to
                      // open/close the install section once.
  std::vector<Release> m_releases;
  Paths::InstallsSummary m_installs;
  std::vector<std::string>
      m_install_names;  // Same order as the rows in m_installs_list.
  std::vector<std::filesystem::path>
      m_userdata_paths;  // Same order as m_userdata_dropdown, the default
                         // first.
  std::vector<std::string>
      m_userdata_labels;  // What m_userdata_dropdown holds now.
  std::set<std::filesystem::path>
      m_kept_missing;  // Missing folders the user chose to keep, not asked
                       // again.
  bool m_asking_missing;

  QWidget* m_content;
  QComboBox* m_game_dropdown;
  QLabel* m_description;

  // Userdata folder choice.
  QComboBox* m_userdata_dropdown;
  QToolButton* m_userdata_add_button;
  QToolButton* m_userdata_rename_button;
  QToolButton* m_userdata_remove_button;

  // Play section.
  QLabel* m_active_title;
  QPushButton* m_play_button;
  QPushButton* m_resume_button;
  QPushButton* m_folder_menu_button;

  // Installed versions.
  QWidget* m_installs_box;
  QTreeWidget* m_installs_list;
  QPushButton* m_make_active_button;
  QPushButton* m_delete_button;

  // Install / update section.
  QToolButton* m_install_expander;
  QWidget* m_install_section;
  QRadioButton* m_stable_button;
  QRadioButton* m_experimental_button;
  QToolButton* m_refresh_button;
  QComboBox* m_builds_dropdown;
  QPushButton* m_install_new_button;
  QPushButton* m_update_button;
};

#endif
