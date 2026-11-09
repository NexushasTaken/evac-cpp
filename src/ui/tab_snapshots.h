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

  AppContext& m_ctx;
  TaskRunner& m_runner;
  QWidget& m_parent;
  bool m_updating{false};
  std::vector<WorldInfo> m_worlds;
  std::vector<Snapshot> m_snapshots;
  std::string m_selected_name;    // Kept across reloads.
  std::set<std::string> m_asked;  // Deleted worlds already asked about.

  QLabel* m_hint_label{};
  QWidget* m_content{};
  QTreeWidget* m_worlds_list{};
  QLabel* m_snapshots_title{};
  QAction* m_export_action{};
  QAction* m_import_action{};
  QAction* m_start_fresh_action{};
  QAction* m_delete_action{};
  QTreeWidget* m_snapshots_list{};
  QPushButton* m_restore_button{};
  QPushButton* m_discard_button{};
  QTextBrowser* m_details_view{};
  QLineEdit* m_name_entry{};
  QLineEdit* m_description_entry{};
  QPushButton* m_take_button{};
  QPushButton* m_take_all_button{};
  QLabel* m_footer_label{};
  QLineEdit* m_command_entry{};
  QPushButton* m_copy_button{};
};

#endif
