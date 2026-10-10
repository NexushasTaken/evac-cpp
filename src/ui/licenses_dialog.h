#ifndef CATAPULT_LICENSES_DIALOG_H
#define CATAPULT_LICENSES_DIALOG_H

#include <gtkmm.h>

#include <string>

// A window with the open source licenses. Closing it only hides it, so the
// same window can be shown again.
class LicensesDialog : public Gtk::Window
{
public:
    LicensesDialog(Gtk::Window& parent, const std::string& data_dir);

private:
    Gtk::ScrolledWindow scroller_;
    Gtk::TextView text_view_;
};

#endif
