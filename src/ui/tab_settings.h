#ifndef CATAPULT_TAB_SETTINGS_H
#define CATAPULT_TAB_SETTINGS_H

#include <gtkmm.h>

#include <string>
#include <utility>
#include <vector>

#include "ui/app_context.h"

// The Settings tab. Every change is applied and saved right away.
class TabSettings : public Gtk::ScrolledWindow
{
public:
    explicit TabSettings(AppContext& ctx);

    // Shows the current values of the settings.
    void load_from_settings();

private:
    void build_ui();
    Gtk::CheckButton* add_check(Gtk::Box& box, const char* caption, const char* tooltip, const char* key);
    void add_row(Gtk::Box& box, const char* caption, const char* tooltip, Gtk::Widget& widget);

    AppContext& ctx_;
    bool updating_;
    std::vector<std::pair<std::string, Gtk::CheckButton*> > checks_;

    Gtk::SpinButton num_releases_;
    Gtk::DropDown proxy_option_;
    Gtk::Entry proxy_host_;
    Gtk::SpinButton proxy_port_;
};

#endif
