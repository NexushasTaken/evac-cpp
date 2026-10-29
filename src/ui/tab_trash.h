#ifndef EVAC_TAB_TRASH_H
#define EVAC_TAB_TRASH_H

#include <QWidget>
#include <functional>
#include <string>
#include <vector>

#include "core/trash.h"
#include "ui/app_context.h"
#include "ui/task_runner.h"

class QLabel;
class QPushButton;
class QTextBrowser;
class QTreeWidget;

// The Trash tab: what was deleted, to restore it or delete it for good.
class TabTrash : public QWidget {
 public:
  TabTrash(AppContext& ctx, TaskRunner& runner, QWidget& parent);

  // Reloads the list. Call when the tab is shown.
  void refresh();

  // Asks to delete the oldest items until the trash is under its size limit
  // (what Free Up Space does). Says so if it is not over the limit.
  void ask_free_up();

 private:
  void build_ui();
  void update_summary();
  void update_buttons();
  void run_task(const std::function<void()>& work);
  int selected_item();

  void on_selection_changed();
  void on_restore_clicked();
  void on_delete_clicked();
  void on_empty_clicked();
  void on_free_up_clicked();
  void on_open_folder_clicked();

  AppContext& ctx_;
  TaskRunner& runner_;
  QWidget& parent_;
  bool updating_;
  std::vector<TrashItem> items_;

  QWidget* content_;
  QLabel* summary_label_;
  QTreeWidget* list_;
  QTextBrowser* info_view_;
  QPushButton* restore_button_;
  QPushButton* delete_button_;
  QPushButton* empty_button_;
  QPushButton* free_up_button_;
  QPushButton* open_folder_button_;
};

#endif
