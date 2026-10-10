#include "ui/tab_game.h"

#include "core/games.h"

static Gtk::Label* make_label(const Glib::ustring& text)
{
    Gtk::Label* label = Gtk::make_managed<Gtk::Label>(text);
    label->set_xalign(0.0f);
    return label;
}

TabGame::TabGame(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent)
    : Gtk::Box(Gtk::Orientation::VERTICAL),
      ctx_(ctx),
      runner_(runner),
      parent_(parent),
      updating_(false),
      content_(Gtk::Orientation::VERTICAL, 8),
      installs_box_(Gtk::Orientation::VERTICAL, 4)
{
    build_ui();
}

void TabGame::build_ui()
{
    content_.set_margin(12);
    append(content_);

    // Game choice and description.
    Glib::RefPtr<Gtk::StringList> games = Gtk::StringList::create(std::vector<Glib::ustring>());
    const std::vector<GameInfo>& all = all_games();
    for (size_t i = 0; i < all.size(); ++i)
        games->append(all[i].name);
    game_dropdown_.set_model(games);
    game_dropdown_.set_tooltip_text("Choose which game to manage or play.");

    Gtk::Box* game_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    game_row->append(*make_label("Game:"));
    game_dropdown_.set_hexpand(true);
    game_row->append(game_dropdown_);
    content_.append(*game_row);

    description_.set_wrap(true);
    description_.set_xalign(0.0f);
    description_.set_use_markup(true);
    description_.set_selectable(true);
    content_.append(description_);

    // Channel and build choice.
    stable_button_.set_label("Stable");
    stable_button_.set_tooltip_text("Stable release channel (DDA and BN only).");
    experimental_button_.set_label("Experimental");
    experimental_button_.set_tooltip_text("Experimental release channel.");
    experimental_button_.set_group(stable_button_);
    refresh_button_.set_label("Refresh");
    refresh_button_.set_tooltip_text("Re-retrieve the list of available game releases\nfrom GitHub (only for experimental releases).");

    Gtk::Box* channel_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    channel_row->append(*make_label("Channel:"));
    channel_row->append(stable_button_);
    channel_row->append(experimental_button_);
    refresh_button_.set_hexpand(true);
    refresh_button_.set_halign(Gtk::Align::END);
    channel_row->append(refresh_button_);
    content_.append(*channel_row);

    builds_dropdown_.set_model(Gtk::StringList::create(std::vector<Glib::ustring>()));
    builds_dropdown_.set_hexpand(true);
    builds_dropdown_.set_tooltip_text("Choose which release of the game to install or update to.");
    Gtk::Box* builds_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    builds_row->append(*make_label("Available builds:"));
    builds_row->append(builds_dropdown_);
    content_.append(*builds_row);

    update_current_check_.set_label("Update current active install");
    update_current_check_.set_tooltip_text(
        "If checked, the game install currently set as active\nwill be replaced with the version selected above.\n"
        "Otherwise, the selected build will be a new,\nseparate install of the game.");
    content_.append(update_current_check_);

    install_button_.set_label("Install Selected");
    install_button_.set_tooltip_text("Install selected game release.");
    content_.append(install_button_);

    content_.append(*Gtk::make_managed<Gtk::Separator>());

    // The active install.
    game_dir_button_.set_label("Game folder");
    game_dir_button_.set_tooltip_text("Open game installation directory (contains\ngame executable and built-in game content).");
    user_dir_button_.set_label("User data folder");
    user_dir_button_.set_tooltip_text("Open user data directory (contains saves, \ngame configuration, user mods, sound, etc.)");
    Gtk::Box* active_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    active_row->append(*make_label("Active install:"));
    active_install_label_.set_xalign(0.0f);
    active_install_label_.set_hexpand(true);
    active_row->append(active_install_label_);
    active_row->append(game_dir_button_);
    active_row->append(user_dir_button_);
    content_.append(*active_row);

    play_button_.set_label("Play");
    play_button_.set_tooltip_text("Launch the game.");
    resume_button_.set_label("Resume Last World");
    resume_button_.set_tooltip_text("Skip main menu and quick-load last played world.");
    Gtk::Box* play_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    play_button_.set_hexpand(true);
    resume_button_.set_hexpand(true);
    play_row->append(play_button_);
    play_row->append(resume_button_);
    content_.append(*play_row);

    // The installs, only shown when there is more than one (or if asked for).
    installs_box_.append(*make_label("Installed versions:"));
    Gtk::ScrolledWindow* scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroller->set_min_content_height(90);
    scroller->set_child(installs_list_);
    scroller->set_has_frame(true);
    installs_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
    installs_box_.append(*scroller);
    make_active_button_.set_label("Make active");
    make_active_button_.set_tooltip_text("Set the selected game install as active (i.e. in use).");
    delete_button_.set_label("Delete");
    delete_button_.set_tooltip_text("Delete the selected copy of the game.\n(Saves, settings and other user data\nare stored separately and will not be lost.)");
    Gtk::Box* installs_buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    make_active_button_.set_hexpand(true);
    delete_button_.set_hexpand(true);
    installs_buttons->append(make_active_button_);
    installs_buttons->append(delete_button_);
    installs_box_.append(*installs_buttons);
    content_.append(installs_box_);

    // Signals. These are connected after the initial state is set, see start().
}

void TabGame::start()
{
    const std::vector<GameInfo>& all = all_games();
    std::string game = ctx_.settings.get_string("game");
    for (size_t i = 0; i < all.size(); ++i)
    {
        if (all[i].key == game)
            game_dropdown_.set_selected((guint)i);
    }
    update_current_check_.set_active(ctx_.settings.get_bool("update_current_when_installing"));
    description_.set_visible(ctx_.settings.get_bool("show_game_desc"));

    game_dropdown_.property_selected().signal_changed().connect(sigc::mem_fun(*this, &TabGame::on_game_selected));
    stable_button_.signal_toggled().connect(sigc::mem_fun(*this, &TabGame::on_channel_toggled));
    builds_dropdown_.property_selected().signal_changed().connect(sigc::mem_fun(*this, &TabGame::on_build_selected));
    update_current_check_.signal_toggled().connect(sigc::mem_fun(*this, &TabGame::on_update_current_toggled));
    refresh_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::fetch_releases));
    install_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_install_clicked));
    play_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_play_clicked));
    resume_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_resume_clicked));
    make_active_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_make_active_clicked));
    delete_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_delete_clicked));
    installs_list_.signal_row_selected().connect(sigc::mem_fun(*this, &TabGame::on_installs_row_selected));
    installs_list_.signal_row_activated().connect(sigc::mem_fun(*this, &TabGame::on_installs_row_activated));
    game_dir_button_.signal_clicked().connect([this]() { open_folder(ctx_.paths.game_dir()); });
    user_dir_button_.signal_clicked().connect([this]() { open_folder(ctx_.paths.userdata()); });

    apply_game_choice();
}

void TabGame::apply_settings()
{
    description_.set_visible(ctx_.settings.get_bool("show_game_desc"));
    refresh_installed();
}

std::string TabGame::release_key()
{
    return ReleaseManager::make_key(ctx_.settings.get_string("game"), ctx_.settings.get_string("channel"));
}

// Runs a slow job on the worker thread, with the whole tab disabled until it
// is done. `after` runs on the main thread when the job has finished.
void TabGame::run_task(const std::function<void()>& work, const std::function<void()>& after)
{
    content_.set_sensitive(false);
    bool started = runner_.run(work, [this, after]() {
        content_.set_sensitive(true);
        after();
    });
    if (!started)
        content_.set_sensitive(true);
}

void TabGame::fetch_releases()
{
    std::string key = release_key();
    run_task([this, key]() { ctx_.releases.fetch(key); },
             [this]() {
                 reload_builds_list();
             });
}

// Makes the widgets match the settings (game and channel) and loads the builds.
void TabGame::apply_game_choice()
{
    updating_ = true;

    const GameInfo& info = find_game(ctx_.settings.get_string("game"));
    if (!info.has_stable_channel && ctx_.settings.get_string("channel") != "experimental")
        ctx_.settings.set_string("channel", "experimental");
    std::string channel = ctx_.settings.get_string("channel");

    stable_button_.set_sensitive(info.has_stable_channel);
    experimental_button_.set_sensitive(info.has_stable_channel);
    if (channel == "stable")
        stable_button_.set_active(true);
    else
        experimental_button_.set_active(true);
    refresh_button_.set_sensitive(channel == "experimental");

    description_.set_markup(info.description);

    const std::vector<GameInfo>& all = all_games();
    for (size_t i = 0; i < all.size(); ++i)
    {
        if (all[i].key == info.key)
            game_dropdown_.set_selected((guint)i);
    }

    updating_ = false;

    if (ctx_.releases.get(release_key()).empty())
        fetch_releases();
    else
        reload_builds_list();
}

void TabGame::reload_builds_list()
{
    updating_ = true;
    releases_ = ctx_.releases.get(release_key());

    Glib::RefPtr<Gtk::StringList> model = Gtk::StringList::create(std::vector<Glib::ustring>());
    for (size_t i = 0; i < releases_.size(); ++i)
        model->append(releases_[i].name);
    builds_dropdown_.set_model(model);
    updating_ = false;

    refresh_installed();
}

// Updates everything that depends on the installed games and the selected build.
void TabGame::refresh_installed()
{
    updating_ = true;

    std::string game = ctx_.settings.get_string("game");
    installs_ = ctx_.paths.installs_summary();
    Paths::InstallsSummary::const_iterator g = installs_.find(game);
    bool have_install = (g != installs_.end());
    std::string active = ctx_.settings.get_string("active_install_" + game);

    // The list of installs.
    while (Gtk::ListBoxRow* row = installs_list_.get_row_at_index(0))
        installs_list_.remove(*row);
    install_names_.clear();
    if (have_install)
    {
        std::map<std::string, std::string>::const_iterator it;
        for (it = g->second.begin(); it != g->second.end(); ++it)
        {
            Gtk::Label* label = Gtk::make_managed<Gtk::Label>(it->first);
            label->set_xalign(0.0f);
            label->set_tooltip_text("Location:\n" + it->second + "\n(double-click to open folder)");
            installs_list_.append(*label);
            install_names_.push_back(it->first);
        }
    }
    make_active_button_.set_sensitive(false);
    delete_button_.set_sensitive(false);
    installs_box_.set_visible((have_install && g->second.size() > 1) || ctx_.settings.get_bool("always_show_installs"));

    // The active install.
    active_install_label_.set_text(have_install ? active : "None");
    game_dir_button_.set_visible(have_install);
    user_dir_button_.set_visible(have_install);
    play_button_.set_sensitive(have_install);
    resume_button_.set_sensitive(have_install && ctx_.launcher.has_last_world());

    // The install button depends on the selected build.
    guint selected = builds_dropdown_.get_selected();
    bool build_selected = (selected != GTK_INVALID_LIST_POSITION && selected < releases_.size());
    bool same_build_installed = build_selected && have_install
                                && g->second.find(releases_[selected].name) != g->second.end()
                                && !ctx_.settings.get_bool("update_to_same_build_allowed");

    update_current_check_.set_sensitive(have_install && !same_build_installed);
    bool updating_active = have_install && update_current_check_.get_active();
    install_button_.set_label(updating_active ? "Update to Selected" : "Install Selected");
    install_button_.set_sensitive(build_selected && !same_build_installed);

    updating_ = false;
    ctx_.settings.save();
}

std::string TabGame::selected_install_name()
{
    Gtk::ListBoxRow* row = installs_list_.get_selected_row();
    if (!row || row->get_index() < 0 || (size_t)row->get_index() >= install_names_.size())
        return "";
    return install_names_[row->get_index()];
}

void TabGame::on_game_selected()
{
    if (updating_)
        return;
    guint index = game_dropdown_.get_selected();
    const std::vector<GameInfo>& all = all_games();
    if (index >= all.size())
        return;

    ctx_.settings.set_string("game", all[index].key);
    ctx_.settings.save();
    apply_game_choice();
}

void TabGame::on_channel_toggled()
{
    if (updating_)
        return;
    ctx_.settings.set_string("channel", stable_button_.get_active() ? "stable" : "experimental");
    ctx_.settings.save();
    apply_game_choice();
}

void TabGame::on_build_selected()
{
    if (updating_)
        return;
    refresh_installed();
}

void TabGame::on_update_current_toggled()
{
    if (updating_)
        return;
    ctx_.settings.set_bool("update_current_when_installing", update_current_check_.get_active());
    refresh_installed();
}

void TabGame::on_installs_row_selected(Gtk::ListBoxRow*)
{
    if (updating_)
        return;
    std::string name = selected_install_name();
    delete_button_.set_sensitive(!name.empty());
    make_active_button_.set_sensitive(!name.empty() && name != ctx_.settings.get_string("active_install_" + ctx_.settings.get_string("game")));
}

void TabGame::on_installs_row_activated(Gtk::ListBoxRow* row)
{
    if (!row || row->get_index() < 0 || (size_t)row->get_index() >= install_names_.size())
        return;
    Paths::InstallsSummary::const_iterator g = installs_.find(ctx_.settings.get_string("game"));
    if (g == installs_.end())
        return;
    std::map<std::string, std::string>::const_iterator it = g->second.find(install_names_[row->get_index()]);
    if (it != g->second.end())
        open_folder(it->second);
}

void TabGame::on_make_active_clicked()
{
    std::string name = selected_install_name();
    if (name.empty())
        return;
    ctx_.status.post("Setting active install to " + name + ".");
    ctx_.settings.set_string("active_install_" + ctx_.settings.get_string("game"), name);
    refresh_installed();
}

void TabGame::on_install_clicked()
{
    guint selected = builds_dropdown_.get_selected();
    if (selected == GTK_INVALID_LIST_POSITION || selected >= releases_.size())
        return;
    Release release = releases_[selected];

    std::string update_path;
    std::string active_name;
    if (ctx_.settings.get_bool("update_current_when_installing"))
    {
        std::string game = ctx_.settings.get_string("game");
        active_name = ctx_.settings.get_string("active_install_" + game);
        Paths::InstallsSummary::const_iterator g = installs_.find(game);
        if (g != installs_.end() && g->second.find(active_name) != g->second.end())
            update_path = g->second.find(active_name)->second;
    }

    if (update_path.empty())
    {
        run_task([this, release]() { ctx_.installer.install_release(release, ""); }, [this]() { refresh_installed(); });
        return;
    }

    confirm_game_safe_to_delete(active_name, [this, release, update_path](bool ok) {
        if (ok)
            run_task([this, release, update_path]() { ctx_.installer.install_release(release, update_path); },
                     [this]() { refresh_installed(); });
    });
}

void TabGame::on_delete_clicked()
{
    std::string name = selected_install_name();
    if (name.empty())
        return;

    confirm_game_safe_to_delete(name, [this, name](bool ok) {
        if (ok)
            run_task([this, name]() { ctx_.installer.remove_release_by_name(name); }, [this]() { refresh_installed(); });
    });
}

// Asks the user first if there is user data inside the install that would be lost.
void TabGame::confirm_game_safe_to_delete(const std::string& install_name, const std::function<void(bool)>& answer)
{
    std::vector<std::string> misplaced = ctx_.installer.check_game_dir_for_userdata(install_name);
    if (misplaced.empty())
    {
        answer(true);
        return;
    }

    std::string detail = "The game directory of \"" + install_name + "\" contains user data that is not in the "
                         "user data directory, and it will be lost:\n";
    for (size_t i = 0; i < misplaced.size(); ++i)
        detail += "\n" + misplaced[i];

    Glib::RefPtr<Gtk::AlertDialog> dialog = Gtk::AlertDialog::create("Delete the game files?");
    dialog->set_detail(detail);
    std::vector<Glib::ustring> buttons;
    buttons.push_back("Cancel");
    buttons.push_back("Delete anyway");
    dialog->set_buttons(buttons);
    dialog->set_cancel_button(0);
    dialog->set_default_button(0);
    dialog->choose(parent_, [dialog, answer](const Glib::RefPtr<Gio::AsyncResult>& result) {
        int choice = 0;
        try
        {
            choice = dialog->choose_finish(result);
        }
        catch (const Glib::Error&)
        {
            choice = 0;  // Dismissed.
        }
        answer(choice == 1);
    });
}

void TabGame::on_play_clicked()
{
    start_game("");
}

void TabGame::on_resume_clicked()
{
    std::string world = ctx_.launcher.last_world();
    if (!world.empty())
        start_game(world);
}

void TabGame::start_game(const std::string& world)
{
    if (!ctx_.launcher.start(world))
        return;
    if (!ctx_.settings.get_bool("keep_open_after_starting_game"))
        parent_.close();
}

void TabGame::open_folder(const std::string& dir)
{
    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(dir);
    if (!file->query_exists())
        return;
    Glib::RefPtr<Gtk::FileLauncher> launcher = Gtk::FileLauncher::create(file);
    launcher->launch(parent_, [launcher](const Glib::RefPtr<Gio::AsyncResult>& result) {
        try
        {
            launcher->launch_finish(result);
        }
        catch (const Glib::Error&)
        {
        }
    });
}
