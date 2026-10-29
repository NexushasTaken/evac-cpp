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

  AppContext& ctx_;
  TaskRunner& runner_;
  QWidget& parent_;
  bool updating_;
  std::vector<BackupSummary> backups_;

  QLabel* hint_label_;
  QWidget* content_;
  QListWidget* list_;
  QPushButton* restore_button_;
  QPushButton* delete_button_;
  QPushButton* refresh_button_;
  QTextBrowser* info_view_;
  QLineEdit* name_entry_;
  QPushButton* create_button_;
};

#endif
