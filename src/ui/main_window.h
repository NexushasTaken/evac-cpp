#ifndef EVAC_MAIN_WINDOW_H
#define EVAC_MAIN_WINDOW_H

#include <QMainWindow>
#include <cstdint>

#include "ui/app_context.h"
#include "ui/task_runner.h"

class QTabWidget;
class LogView;
class TabBackups;
class TabFonts;
class TabGame;
class TabMods;
class TabSettings;
class TabSnapshots;
class TabSoundpacks;
class TabTrash;

class MainWindow : public QMainWindow {
 public:
  explicit MainWindow(AppContext& ctx);

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void save_window_state();
  void on_switch_page(int index);
  void on_task_finished();
  void show_page(QWidget* page);
  void check_trash_limit();

  AppContext& m_ctx;
  bool m_page_refresh_pending{
      false};                       // A tab was opened while a job was running.
  std::uint64_t m_trash_size_seen;  // To notice when the trash grows.
  TaskRunner m_runner;

  // Owned by the window (Qt deletes child widgets with their parent).
  QTabWidget* m_tabs;
  TabGame* m_tab_game;
  TabMods* m_tab_mods;
  TabSoundpacks* m_tab_soundpacks;
  TabFonts* m_tab_fonts;
  TabSnapshots* m_tab_snapshots;
  TabBackups* m_tab_backups;
  TabTrash* m_tab_trash;
  TabSettings* m_tab_settings;
  LogView* m_log_view;
};

#endif
