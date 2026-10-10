#include "ui/tab_settings.h"

// The values of proxy_option, in the order of the drop-down.
static const char* const PROXY_OPTIONS[] = {"off", "on", "download"};
static const int NUM_PROXY_OPTIONS = 3;

static Gtk::Label* make_heading(const Glib::ustring& text)
{
    Gtk::Label* label = Gtk::make_managed<Gtk::Label>();
    label->set_markup("<b>" + Glib::Markup::escape_text(text) + "</b>");
    label->set_xalign(0.0f);
    label->set_margin_top(8);
    return label;
}

TabSettings::TabSettings(AppContext& ctx) : ctx_(ctx), updating_(false)
{
    build_ui();
    load_from_settings();
}

Gtk::CheckButton* TabSettings::add_check(Gtk::Box& box, const char* caption, const char* tooltip, const char* key)
{
    Gtk::CheckButton* check = Gtk::make_managed<Gtk::CheckButton>(caption);
    check->set_tooltip_text(tooltip);
    std::string setting = key;
    check->signal_toggled().connect([this, check, setting]() {
        if (updating_)
            return;
        ctx_.settings.set_bool(setting, check->get_active());
        ctx_.settings.save();
    });
    box.append(*check);
    checks_.push_back(std::make_pair(setting, check));
    return check;
}

void TabSettings::add_row(Gtk::Box& box, const char* caption, const char* tooltip, Gtk::Widget& widget)
{
    Gtk::Box* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    Gtk::Label* label = Gtk::make_managed<Gtk::Label>(caption);
    label->set_xalign(0.0f);
    label->set_hexpand(true);
    label->set_tooltip_text(tooltip);
    row->append(*label);
    widget.set_tooltip_text(tooltip);
    row->append(widget);
    box.append(*row);
}

void TabSettings::build_ui()
{
    Gtk::Box* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    box->set_margin(12);
    set_child(*box);

    box->append(*make_heading("Game"));
    add_check(*box, "Show game description", "Show a description for the chosen game under the Game list.", "show_game_desc");
    add_check(*box, "Keep launcher open after starting the game",
              "If turned off, the launcher will close after clicking Play or Resume.\n"
              "Note that if the game fails to start for any reason, the launcher\nwill still close.",
              "keep_open_after_starting_game");
    add_check(*box, "Allow \"updating\" to the same build as installed",
              "Allow starting the update procedure even when the selected\nrelease is the same as currently installed.",
              "update_to_same_build_allowed");
    add_check(*box, "Shorten names of experimental releases",
              "Shorten release names to just the date and number\n(e.g. \"2021-09-08-1927\" instead of\n"
              "\"Cataclysm-DDA experimental build 2021-09-08-1927\").",
              "shorten_release_names");
    add_check(*box, "Always show installs list",
              "Show the installs list even when just one\ncopy of the game is installed, or none at all.", "always_show_installs");

    num_releases_.set_adjustment(Gtk::Adjustment::create(10, 1, 100, 1));
    num_releases_.signal_value_changed().connect([this]() {
        if (updating_)
            return;
        ctx_.settings.set_int("num_releases_to_request", num_releases_.get_value_as_int());
        ctx_.settings.save();
    });
    add_row(*box, "Number of releases to request:",
            "How many latest releases will be requested\nfrom GitHub after starting the launcher,\nchoosing the game or clicking Refresh.",
            num_releases_);

    box->append(*make_heading("Mods"));
    add_check(*box, "List obsolete mods in installed",
              "Show mods that come with the game but are marked as \"obsolete\".\nNormally, such mods are not considered as installed.",
              "show_obsolete_mods");
    add_check(*box, "Include archived mods when installing Kenan Modpack",
              "When installing Kenan Modpack, also include *archived* mods.\nThese mods are no longer maintained by Kenan and may be outdated.",
              "install_archived_mods");

    box->append(*make_heading("Downloads"));
    add_check(*box, "Keep download cache",
              "Keep downloaded contents (game releases, modpacks, soundpacks) in cache folder\nwhen install completes, instead of having it deleted.",
              "keep_cache");
    add_check(*box, "Ignore download cache", "Always download contents from the web,\neven if it's already inside cache folder.",
              "ignore_cache");

    Glib::RefPtr<Gtk::StringList> proxy_model = Gtk::StringList::create(std::vector<Glib::ustring>());
    proxy_model->append("Disabled");
    proxy_model->append("Enabled");
    proxy_model->append("For Downloading Only");
    proxy_option_.set_model(proxy_model);
    proxy_option_.property_selected().signal_changed().connect([this]() {
        guint index = proxy_option_.get_selected();
        if (updating_ || index >= (guint)NUM_PROXY_OPTIONS)
            return;
        ctx_.settings.set_string("proxy_option", PROXY_OPTIONS[index]);
        ctx_.settings.save();
    });
    add_row(*box, "Proxy server:",
            "Disabled - Do not use proxy\nEnabled - Use proxy on all occasions (e.g. requesting releases)\n"
            "For Downloading Only - Use proxy only when downloading",
            proxy_option_);

    proxy_host_.set_placeholder_text("host");
    proxy_host_.signal_changed().connect([this]() {
        if (updating_)
            return;
        ctx_.settings.set_string("proxy_host", proxy_host_.get_text());
        ctx_.settings.save();
    });
    add_row(*box, "Proxy host:", "Configure a proxy server for downloading.", proxy_host_);

    proxy_port_.set_adjustment(Gtk::Adjustment::create(0, 0, 65535, 1));
    proxy_port_.signal_value_changed().connect([this]() {
        if (updating_)
            return;
        ctx_.settings.set_int("proxy_port", proxy_port_.get_value_as_int());
        ctx_.settings.save();
    });
    add_row(*box, "Proxy port:", "Configure a proxy server for downloading.", proxy_port_);

    box->append(*make_heading("Other"));
    add_check(*box, "Debug mode", "Enables debug messages in the status pane\n(useful for troubleshooting).", "debug_mode");

    Gtk::Label* note = Gtk::make_managed<Gtk::Label>("The launcher uses your system's GTK theme and display scale.");
    note->set_xalign(0.0f);
    note->set_margin_top(12);
    note->set_sensitive(false);
    box->append(*note);
}

void TabSettings::load_from_settings()
{
    updating_ = true;

    for (size_t i = 0; i < checks_.size(); ++i)
        checks_[i].second->set_active(ctx_.settings.get_bool(checks_[i].first));

    num_releases_.set_value(ctx_.settings.get_int("num_releases_to_request"));

    std::string option = ctx_.settings.get_string("proxy_option");
    guint selected = 0;
    for (int i = 0; i < NUM_PROXY_OPTIONS; ++i)
    {
        if (option == PROXY_OPTIONS[i])
            selected = (guint)i;
    }
    proxy_option_.set_selected(selected);

    proxy_host_.set_text(ctx_.settings.get_string("proxy_host"));
    proxy_port_.set_value(ctx_.settings.get_int("proxy_port"));

    updating_ = false;
}
