#ifndef EVAC_TAB_SNAPSHOTS_H
#define EVAC_TAB_SNAPSHOTS_H

#include <QWidget>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "core/snapshot_manager.h"
#include "ui/app_context.h"
#include "ui/task_runner.h"

class QAction;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextBrowser;
class QTreeWidget;

// The Snapshots tab: git snapshots of each world, to take, restore or discard.
// See docs/git-backups.md.
class TabSnapshots : public QWidget {
 public:
  TabSnapshots(AppContext& ctx, TaskRunner& runner, QWidget& parent);

  // Reloads the worlds and their status. Call when the tab is shown.
  void refresh();

 private:
  void build_ui();
  void show_unavailable(const QString& message);
  void reload();
  void fill_worlds(const std::vector<WorldInfo>& worlds);
  void fill_snapshots();
  void update_buttons();
  void update_details();
  void ask_about_deleted_worlds();
  void run_task(const std::function<void()>& work);
  bool check_writable();
  const WorldInfo* selected_world();
  const Snapshot* selected_snapshot();

  void on_world_selected();
  void on_snapshot_selected();
  void on_take_clicked();
  void on_take_all_clicked();
  void on_restore_clicked();
  void on_discard_clicked();
  void on_copy_clicked();
  void on_export_clicked();
  void on_import_clicked();
  void on_start_fresh_clicked();
  void on_delete_clicked();
  void import_bundle(const std::filesystem::path& path,
                     const std::string& world);

  AppContext& ctx_;
  TaskRunner& runner_;
  QWidget& parent_;
  bool updating_;
  std::vector<WorldInfo> worlds_;
  std::vector<Snapshot> snapshots_;
  std::string selected_name_;    // Kept across reloads.
  std::set<std::string> asked_;  // Deleted worlds already asked about.

  QLabel* hint_label_;
  QWidget* content_;
  QTreeWidget* worlds_list_;
  QLabel* snapshots_title_;
  QAction* export_action_;
  QAction* import_action_;
  QAction* start_fresh_action_;
  QAction* delete_action_;
  QTreeWidget* snapshots_list_;
  QPushButton* restore_button_;
  QPushButton* discard_button_;
  QTextBrowser* details_view_;
  QLineEdit* name_entry_;
  QLineEdit* description_entry_;
  QPushButton* take_button_;
  QPushButton* take_all_button_;
  QLabel* footer_label_;
  QLineEdit* command_entry_;
  QPushButton* copy_button_;
};

#endif
