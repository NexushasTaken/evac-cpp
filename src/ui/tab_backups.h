#ifndef EVAC_TAB_BACKUPS_H
#define EVAC_TAB_BACKUPS_H

#include <QWidget>
#include <functional>
#include <string>
#include <vector>

#include "core/backup_manager.h"
#include "ui/app_context.h"
#include "ui/task_runner.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextBrowser;

// The Backups tab: back up the saved games, and restore or delete backups.
class TabBackups : public QWidget {
 public:
  TabBackups(AppContext& ctx, TaskRunner& runner, QWidget& parent);

  // Reloads the list of backups. Call when the tab is shown.
  void refresh();

 private:
  void build_ui();
  void show_unavailable(const QString& message);
  void reload_list();
  void update_buttons();
  void run_task(const std::function<void()>& work);
  int selected_backup();

  void on_selection_changed();
  void on_name_changed();
  void on_create_clicked();
  void on_restore_clicked();
  void confirm_restore(const std::string& name,
                       const std::vector<std::string>& changed);
  void on_delete_clicked();

  AppContext& m_ctx;
  TaskRunner& m_runner;
  QWidget& m_parent;
  bool m_updating{false};
  std::vector<BackupSummary> m_backups;

  QLabel* m_hint_label{};
  QWidget* m_content{};
  QListWidget* m_list{};
  QPushButton* m_restore_button{};
  QPushButton* m_delete_button{};
  QPushButton* m_refresh_button{};
  QTextBrowser* m_info_view{};
  QLineEdit* m_name_entry{};
  QPushButton* m_create_button{};
};

#endif
