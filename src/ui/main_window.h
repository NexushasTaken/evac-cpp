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

  AppContext& ctx_;
  bool page_refresh_pending_;      // A tab was opened while a job was running.
  std::uint64_t trash_size_seen_;  // To notice when the trash grows.
  TaskRunner runner_;

  // Owned by the window (Qt deletes child widgets with their parent).
  QTabWidget* tabs_;
  TabGame* tab_game_;
  TabMods* tab_mods_;
  TabSoundpacks* tab_soundpacks_;
  TabFonts* tab_fonts_;
  TabSnapshots* tab_snapshots_;
  TabBackups* tab_backups_;
  TabTrash* tab_trash_;
  TabSettings* tab_settings_;
  LogView* log_view_;
};

#endif
