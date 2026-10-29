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
    : m_ctx(ctx),
      m_page_refresh_pending(false),
      m_trash_size_seen(ctx.trash.total_size()),
      m_runner(ctx.status) {
  setWindowTitle("Evac \xe2\x80\x94 a launcher for Cataclysm: DDA and BN");
  WindowState saved = load_window_state(ctx.settings);
  resize(saved.valid ? saved.width : DEFAULT_WIDTH,
         saved.valid ? saved.height : DEFAULT_HEIGHT);
  if (saved.valid && saved.maximized)
    setWindowState(
        Qt::WindowMaximized);  // Takes effect when the window is shown.

  m_tab_game = new TabGame(ctx, m_runner, *this);
  m_tab_mods = new TabMods(ctx, m_runner, *this);
  m_tab_soundpacks = new TabSoundpacks(ctx, m_runner, *this);
  m_tab_fonts = new TabFonts(ctx, *this);
  m_tab_snapshots = new TabSnapshots(ctx, m_runner, *this);
  m_tab_backups = new TabBackups(ctx, m_runner, *this);
  m_tab_trash = new TabTrash(ctx, m_runner, *this);
  m_tab_settings = new TabSettings(ctx, *this);
  m_log_view = new LogView(ctx.status);

  m_tabs = new QTabWidget();
  m_tabs->addTab(m_tab_game, "Game");
  m_tabs->addTab(m_tab_mods, "Mods");
  m_tabs->addTab(m_tab_soundpacks, "Soundpacks");
  m_tabs->addTab(m_tab_fonts, "Fonts");
  m_tabs->addTab(m_tab_snapshots, "Snapshots");
  m_tabs->addTab(m_tab_backups, "Backups");
  m_tabs->addTab(m_tab_trash, "Trash");
  m_tabs->addTab(m_tab_settings, "Settings");

  QWidget* central = new QWidget();
  QVBoxLayout* layout = new QVBoxLayout(central);
  layout->addWidget(m_tabs, 1);
  layout->addWidget(m_log_view);
  setCentralWidget(central);

  connect(m_tabs, &QTabWidget::currentChanged, this,
          &MainWindow::on_switch_page);
  connect(&m_runner, &TaskRunner::finished, this,
          &MainWindow::on_task_finished);

  m_ctx.status.post("Welcome to Evac!");
  if (m_ctx.settings.get_bool("print_tips_of_the_day"))
    m_ctx.status.post("Tip of the day: " +
                      pick_tip((unsigned long)std::time(nullptr)));
  m_tab_game->start();
}

void MainWindow::closeEvent(QCloseEvent* event) {
  // Stop a running download and wait for the worker, so it never touches
  // things that are already gone.
  if (m_runner.busy()) {
    m_ctx.releases.cancel();
    m_ctx.installer.cancel();
    m_ctx.soundpacks.cancel();
    m_ctx.mods.cancel();
    m_runner.wait();
  }
  save_window_state();
  m_ctx.settings.save();
  event->accept();
}

void MainWindow::on_switch_page(int index) {
  QWidget* page = m_tabs->widget(index);
  m_log_view->setVisible(
      page != m_tab_settings);  // The log is not needed next to the settings.

  // A running job is changing the files and lists that the tabs show, so
  // wait until it is over.
  if (m_runner.busy()) {
    m_page_refresh_pending = true;
    return;
  }
  show_page(page);
}

void MainWindow::on_task_finished() {
  check_trash_limit();
  if (!m_page_refresh_pending) return;
  m_page_refresh_pending = false;
  show_page(m_tabs->currentWidget());
}

// After a job that put something in the trash, see if it is over its size
// limit. Never deletes without asking: with "trash_auto_prune" it asks right
// away, otherwise it only says so.
void MainWindow::check_trash_limit() {
  std::uint64_t total = m_ctx.trash.total_size();
  bool grew = (total > m_trash_size_seen);
  m_trash_size_seen = total;
  if (!grew || m_ctx.trash.over_limit().empty()) return;

  if (m_ctx.settings.get_bool("trash_auto_prune"))
    m_tab_trash->ask_free_up();
  else
    m_ctx.status.post(
        "The trash is over its size limit (" + str(format_size(total, true)) +
            "). Use Free Up Space on the Trash tab, or change the "
            "limit in Settings.",
        MSG_WARN);
}

// Tabs read the game's files when they are shown, as these can change at any
// time.
void MainWindow::show_page(QWidget* page) {
  if (page == m_tab_game)
    m_tab_game->apply_settings();
  else if (page == m_tab_mods)
    m_tab_mods->refresh();
  else if (page == m_tab_soundpacks)
    m_tab_soundpacks->refresh();
  else if (page == m_tab_fonts)
    m_tab_fonts->refresh();
  else if (page == m_tab_snapshots)
    m_tab_snapshots->refresh();
  else if (page == m_tab_backups)
    m_tab_backups->refresh();
  else if (page == m_tab_trash)
    m_tab_trash->refresh();
  else if (page == m_tab_settings)
    m_tab_settings->load_from_settings();
}

// A maximized window keeps the size it had before, so that un-maximizing it
// next time gives something sensible.
void MainWindow::save_window_state() {
  WindowState previous = load_window_state(m_ctx.settings);
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
  store_window_state(m_ctx.settings, state);
}
