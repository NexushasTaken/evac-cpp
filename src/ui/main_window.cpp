#include "ui/main_window.h"

#include <QCloseEvent>
#include <QTabWidget>
#include <QVBoxLayout>
#include <ctime>

#include "core/tips.h"
#include "core/window_state.h"
#include "ui/log_view.h"
#include "ui/qt_helpers.h"
#include "ui/tab_backups.h"
#include "ui/tab_fonts.h"
#include "ui/tab_game.h"
#include "ui/tab_mods.h"
#include "ui/tab_settings.h"
#include "ui/tab_snapshots.h"
#include "ui/tab_soundpacks.h"
#include "ui/tab_trash.h"

static const int DEFAULT_WIDTH = 760;
static const int DEFAULT_HEIGHT = 820;

MainWindow::MainWindow(AppContext& ctx)
    : ctx_(ctx),
      page_refresh_pending_(false),
      trash_size_seen_(ctx.trash.total_size()),
      runner_(ctx.status) {
  setWindowTitle("Evac \xe2\x80\x94 a launcher for Cataclysm: DDA and BN");
  WindowState saved = load_window_state(ctx.settings);
  resize(saved.valid ? saved.width : DEFAULT_WIDTH,
         saved.valid ? saved.height : DEFAULT_HEIGHT);
  if (saved.valid && saved.maximized)
    setWindowState(
        Qt::WindowMaximized);  // Takes effect when the window is shown.

  tab_game_ = new TabGame(ctx, runner_, *this);
  tab_mods_ = new TabMods(ctx, runner_, *this);
  tab_soundpacks_ = new TabSoundpacks(ctx, runner_, *this);
  tab_fonts_ = new TabFonts(ctx, *this);
  tab_snapshots_ = new TabSnapshots(ctx, runner_, *this);
  tab_backups_ = new TabBackups(ctx, runner_, *this);
  tab_trash_ = new TabTrash(ctx, runner_, *this);
  tab_settings_ = new TabSettings(ctx, *this);
  log_view_ = new LogView(ctx.status);

  tabs_ = new QTabWidget();
  tabs_->addTab(tab_game_, "Game");
  tabs_->addTab(tab_mods_, "Mods");
  tabs_->addTab(tab_soundpacks_, "Soundpacks");
  tabs_->addTab(tab_fonts_, "Fonts");
  tabs_->addTab(tab_snapshots_, "Snapshots");
  tabs_->addTab(tab_backups_, "Backups");
  tabs_->addTab(tab_trash_, "Trash");
  tabs_->addTab(tab_settings_, "Settings");

  QWidget* central = new QWidget();
  QVBoxLayout* layout = new QVBoxLayout(central);
  layout->addWidget(tabs_, 1);
  layout->addWidget(log_view_);
  setCentralWidget(central);

  connect(tabs_, &QTabWidget::currentChanged, this,
          &MainWindow::on_switch_page);
  connect(&runner_, &TaskRunner::finished, this, &MainWindow::on_task_finished);

  ctx_.status.post("Welcome to Evac!");
  if (ctx_.settings.get_bool("print_tips_of_the_day"))
    ctx_.status.post("Tip of the day: " +
                     pick_tip((unsigned long)std::time(nullptr)));
  tab_game_->start();
}

void MainWindow::closeEvent(QCloseEvent* event) {
  // Stop a running download and wait for the worker, so it never touches
  // things that are already gone.
  if (runner_.busy()) {
    ctx_.releases.cancel();
    ctx_.installer.cancel();
    ctx_.soundpacks.cancel();
    ctx_.mods.cancel();
    runner_.wait();
  }
  save_window_state();
  ctx_.settings.save();
  event->accept();
}

void MainWindow::on_switch_page(int index) {
  QWidget* page = tabs_->widget(index);
  log_view_->setVisible(
      page != tab_settings_);  // The log is not needed next to the settings.

  // A running job is changing the files and lists that the tabs show, so
  // wait until it is over.
  if (runner_.busy()) {
    page_refresh_pending_ = true;
    return;
  }
  show_page(page);
}

void MainWindow::on_task_finished() {
  check_trash_limit();
  if (!page_refresh_pending_) return;
  page_refresh_pending_ = false;
  show_page(tabs_->currentWidget());
}

// After a job that put something in the trash, see if it is over its size
// limit. Never deletes without asking: with "trash_auto_prune" it asks right
// away, otherwise it only says so.
void MainWindow::check_trash_limit() {
  std::uint64_t total = ctx_.trash.total_size();
  bool grew = (total > trash_size_seen_);
  trash_size_seen_ = total;
  if (!grew || ctx_.trash.over_limit().empty()) return;

  if (ctx_.settings.get_bool("trash_auto_prune"))
    tab_trash_->ask_free_up();
  else
    ctx_.status.post("The trash is over its size limit (" +
                         str(format_size(total, true)) +
                         "). Use Free Up Space on the Trash tab, or change the "
                         "limit in Settings.",
                     MSG_WARN);
}

// Tabs read the game's files when they are shown, as these can change at any
// time.
void MainWindow::show_page(QWidget* page) {
  if (page == tab_game_)
    tab_game_->apply_settings();
  else if (page == tab_mods_)
    tab_mods_->refresh();
  else if (page == tab_soundpacks_)
    tab_soundpacks_->refresh();
  else if (page == tab_fonts_)
    tab_fonts_->refresh();
  else if (page == tab_snapshots_)
    tab_snapshots_->refresh();
  else if (page == tab_backups_)
    tab_backups_->refresh();
  else if (page == tab_trash_)
    tab_trash_->refresh();
  else if (page == tab_settings_)
    tab_settings_->load_from_settings();
}

// A maximized window keeps the size it had before, so that un-maximizing it
// next time gives something sensible.
void MainWindow::save_window_state() {
  WindowState previous = load_window_state(ctx_.settings);
  WindowState state;
  state.valid = true;
  state.maximized = isMaximized();
  if (state.maximized) {
    state.width = previous.valid ? previous.width : DEFAULT_WIDTH;
    state.height = previous.valid ? previous.height : DEFAULT_HEIGHT;
  } else {
    state.width = width();
    state.height = height();
  }
  store_window_state(ctx_.settings, state);
}
