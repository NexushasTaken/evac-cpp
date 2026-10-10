#include "ui/main_window.h"

MainWindow::MainWindow(AppContext& ctx)
    : ctx_(ctx),
      runner_(ctx.status),
      layout_(Gtk::Orientation::VERTICAL),
      tab_game_(ctx, runner_, *this),
      log_view_(ctx.status)
{
    set_title("Catapult \xe2\x80\x94 a launcher for Cataclysm: DDA and BN");
    set_default_size(760, 820);

    tabs_.set_vexpand(true);
    tabs_.append_page(tab_game_, "Game");

    // The other tabs are not ported yet.
    const char* const todo[] = {"Mods", "Soundpacks", "Fonts", "Backups", "Settings"};
    for (size_t i = 0; i < sizeof(todo) / sizeof(todo[0]); ++i)
    {
        Gtk::Label* label = Gtk::make_managed<Gtk::Label>("Not implemented yet.");
        label->set_sensitive(false);
        tabs_.append_page(*label, todo[i]);
    }

    layout_.append(tabs_);
    layout_.append(log_view_);
    set_child(layout_);

    signal_close_request().connect(sigc::mem_fun(*this, &MainWindow::on_close), false);

    ctx_.status.post("Welcome to Catapult!");
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
        runner_.wait();
    }
    ctx_.settings.save();
    return false;
}
