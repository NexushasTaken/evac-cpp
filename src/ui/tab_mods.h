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
    bool grayed;
    std::string badge;  // Why the row is grayed, shown next to the name.
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
  void extracted();
  void on_show_stock_toggled();
  void on_show_installed_toggled();
  void on_delete_clicked();
  void on_add_selected_clicked();
  void on_add_all_clicked();
  void on_kenan_clicked();
  void install_mods(const std::vector<std::string>& new_ids,
                    const std::vector<std::string>& reinstall_ids);

  AppContext& m_ctx;
  TaskRunner& m_runner;
  QWidget& m_parent;
  bool m_updating;
  std::vector<ModRow> m_installed_rows;
  std::vector<ModRow> m_available_rows;
  std::set<int> m_previous_installed_selection;
  std::set<int> m_previous_available_selection;

  QLabel* m_hint_label;
  QWidget* m_content;
  QLabel* m_installed_caption;
  QLabel* m_available_caption;
  QTreeWidget* m_installed_list;
  QTreeWidget* m_available_list;
  QCheckBox* m_show_stock_check;
  QCheckBox* m_show_installed_check;
  QPushButton* m_delete_button;
  QPushButton* m_add_selected_button;
  QPushButton* m_add_all_button;
  QPushButton* m_kenan_button;
  QTextBrowser* m_info_view;
};

#endif
