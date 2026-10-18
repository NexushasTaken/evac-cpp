#ifndef CATAPULT_TAB_SETTINGS_H
#define CATAPULT_TAB_SETTINGS_H

#include <gtkmm.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ui/app_context.h"
#include "ui/licenses_dialog.h"

// The Settings tab. Every change is applied and saved right away.
class TabSettings : public Gtk::ScrolledWindow
{
public:
    TabSettings(AppContext& ctx, Gtk::Window& parent);

    // Shows the current values of the settings.
    void load_from_settings();

private:
    void build_ui();
    Gtk::CheckButton* add_check(Gtk::Box& box, const char* caption, const char* tooltip, const char* key);
    void add_row(Gtk::Box& box, const char* caption, const char* tooltip, Gtk::Widget& widget);

    AppContext& ctx_;
    Gtk::Window& parent_;
    std::unique_ptr<LicensesDialog> licenses_dialog_;  // Made when first needed.
    bool updating_;
    std::vector<std::pair<std::string, Gtk::CheckButton*> > checks_;

    Gtk::SpinButton num_releases_;
    Gtk::DropDown proxy_option_;
    Gtk::Entry proxy_host_;
    Gtk::SpinButton proxy_port_;
    Gtk::SpinButton trash_max_size_;
};

#endif
