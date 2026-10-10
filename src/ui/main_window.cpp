#include "ui/main_window.h"

#include <ctime>

#include "core/tips.h"
#include "core/window_state.h"

static const int DEFAULT_WIDTH = 760;
static const int DEFAULT_HEIGHT = 820;

MainWindow::MainWindow(AppContext& ctx)
    : ctx_(ctx),
      page_refresh_pending_(false),
      runner_(ctx.status),
      layout_(Gtk::Orientation::VERTICAL),
      tab_game_(ctx, runner_, *this),
      tab_mods_(ctx, runner_, *this),
      tab_soundpacks_(ctx, runner_, *this),
      tab_fonts_(ctx),
      tab_backups_(ctx, runner_, *this),
      tab_settings_(ctx, *this),
      log_view_(ctx.status)
{
    set_title("Catapult \xe2\x80\x94 a launcher for Cataclysm: DDA and BN");
    WindowState saved = load_window_state(ctx.settings);
    set_default_size(saved.valid ? saved.width : DEFAULT_WIDTH, saved.valid ? saved.height : DEFAULT_HEIGHT);
    if (saved.valid && saved.maximized)
        maximize();

    tabs_.set_vexpand(true);
    tabs_.append_page(tab_game_, "Game");

    tabs_.append_page(tab_mods_, "Mods");
    tabs_.append_page(tab_soundpacks_, "Soundpacks");
    tabs_.append_page(tab_fonts_, "Fonts");
    tabs_.append_page(tab_backups_, "Backups");
    tabs_.append_page(tab_settings_, "Settings");

    layout_.append(tabs_);
    layout_.append(log_view_);
    set_child(layout_);

    tabs_.signal_switch_page().connect(sigc::mem_fun(*this, &MainWindow::on_switch_page));
    runner_.signal_finished().connect(sigc::mem_fun(*this, &MainWindow::on_task_finished));
    signal_close_request().connect(sigc::mem_fun(*this, &MainWindow::on_close), false);

    ctx_.status.post("Welcome to Catapult!");
    if (ctx_.settings.get_bool("print_tips_of_the_day"))
        ctx_.status.post("Tip of the day: " + pick_tip((unsigned long)std::time(nullptr)));
    tab_game_.start();
}

bool MainWindow::on_close()
{
    // Stop a running download and wait for the worker, so it never touches
    // things that are already gone.
    if (runner_.busy())
    {
        ctx_.releases.cancel();
        ctx_.installer.cancel();
        ctx_.soundpacks.cancel();
        ctx_.mods.cancel();
        runner_.wait();
    }
    save_window_state();
    ctx_.settings.save();
    return false;
}

void MainWindow::on_switch_page(Gtk::Widget* page, guint)
{
    log_view_.set_visible(page != &tab_settings_);  // The log is not needed next to the settings.

    // A running job is changing the files and lists that the tabs show, so
    // wait until it is over.
    if (runner_.busy())
    {
        page_refresh_pending_ = true;
        return;
    }
    show_page(page);
}

void MainWindow::on_task_finished()
{
    if (!page_refresh_pending_)
        return;
    page_refresh_pending_ = false;
    show_page(tabs_.get_nth_page(tabs_.get_current_page()));
}

// Tabs read the game's files when they are shown, as these can change at any time.
void MainWindow::show_page(Gtk::Widget* page)
{
    if (page == &tab_game_)
        tab_game_.apply_settings();
    else if (page == &tab_mods_)
        tab_mods_.refresh();
    else if (page == &tab_soundpacks_)
        tab_soundpacks_.refresh();
    else if (page == &tab_fonts_)
        tab_fonts_.refresh();
    else if (page == &tab_backups_)
        tab_backups_.refresh();
    else if (page == &tab_settings_)
        tab_settings_.load_from_settings();
}

// A maximized window keeps the size it had before, so that un-maximizing it
// next time gives something sensible.
void MainWindow::save_window_state()
{
    WindowState previous = load_window_state(ctx_.settings);
    WindowState state;
    state.valid = true;
    state.maximized = is_maximized();
    if (state.maximized)
    {
        state.width = previous.valid ? previous.width : DEFAULT_WIDTH;
        state.height = previous.valid ? previous.height : DEFAULT_HEIGHT;
    }
    else
    {
        state.width = get_width();
        state.height = get_height();
    }
    store_window_state(ctx_.settings, state);
}
