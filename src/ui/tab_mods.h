#ifndef EVAC_TAB_MODS_H
#define EVAC_TAB_MODS_H

#include <QWidget>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "ui/app_context.h"
#include "ui/task_runner.h"

class QCheckBox;
class QLabel;
class QPushButton;
class QTextBrowser;
class QTreeWidget;

// The Mods tab: the installed mods on the left, the local repository (where
// the Kenan modpack is unpacked to) on the right, and details of the selected
// mod at the bottom.
class TabMods : public QWidget {
 public:
  TabMods(AppContext& ctx, TaskRunner& runner, QWidget& parent);

  // Reads the mods from disk and updates both lists. Call when the tab is
  // shown.
  void refresh();

 private:
  // What a row of a list stands for.
  struct ModRow {
    std::string id;
    std::string name;
    std::string location;
    bool greyed;
    std::string badge;  // Why the row is greyed, shown next to the name.
  };

  void build_ui();
  void show_unavailable(const QString& message);
  void reload_installed();
  void reload_available();
  void show_mod_info(const std::string& id, bool from_available);
  void run_task(const std::function<void()>& work);

  std::set<int> selected_rows(QTreeWidget* list);
  void fill_list(QTreeWidget* list, const std::vector<ModRow>& rows,
                 const QString& placeholder);

  void on_installed_selection_changed();
  void on_available_selection_changed();
  void on_show_stock_toggled();
  void on_show_installed_toggled();
  void on_delete_clicked();
  void on_add_selected_clicked();
  void on_add_all_clicked();
  void on_kenan_clicked();
  void install_mods(const std::vector<std::string>& new_ids,
                    const std::vector<std::string>& reinstall_ids);

  AppContext& ctx_;
  TaskRunner& runner_;
  QWidget& parent_;
  bool updating_;
  std::vector<ModRow> installed_rows_;
  std::vector<ModRow> available_rows_;
  std::set<int> previous_installed_selection_;
  std::set<int> previous_available_selection_;

  QLabel* hint_label_;
  QWidget* content_;
  QLabel* installed_caption_;
  QLabel* available_caption_;
  QTreeWidget* installed_list_;
  QTreeWidget* available_list_;
  QCheckBox* show_stock_check_;
  QCheckBox* show_installed_check_;
  QPushButton* delete_button_;
  QPushButton* add_selected_button_;
  QPushButton* add_all_button_;
  QPushButton* kenan_button_;
  QTextBrowser* info_view_;
};

#endif
