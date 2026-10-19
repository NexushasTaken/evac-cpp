#include "ui/tab_game.h"

#include "core/games.h"
#include "ui/dialogs.h"
#include "ui/download_prompt.h"

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
      had_install_(-1),
      asking_missing_(false),
      content_(Gtk::Orientation::VERTICAL, 12),
      play_box_(Gtk::Orientation::VERTICAL, 8),
      installs_box_(Gtk::Orientation::VERTICAL, 4)
{
    build_ui();
}

void TabGame::build_ui()
{
    content_.set_margin(12);
    append(content_);

    // 1. Game choice and description.
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

    // Which userdata folder the game uses (saves, config, user mods, ...).
    userdata_dropdown_.set_model(Gtk::StringList::create(std::vector<Glib::ustring>()));
    userdata_dropdown_.set_hexpand(true);
    userdata_add_button_.set_icon_name("list-add-symbolic");
    userdata_add_button_.set_tooltip_text("Add a user data folder. It can be anywhere,\nand empty: the game fills it in.");
    userdata_rename_button_.set_icon_name("document-edit-symbolic");
    userdata_rename_button_.set_tooltip_text("Rename the selected user data folder in this list.\n(The folder itself is not renamed.)");
    userdata_remove_button_.set_icon_name("list-remove-symbolic");
    userdata_remove_button_.set_tooltip_text("Remove the selected user data folder from this list.\n(Its files are not deleted.)");
    Gtk::Box* userdata_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    userdata_row->append(*make_label("User data:"));
    userdata_row->append(userdata_dropdown_);
    userdata_row->append(userdata_add_button_);
    userdata_row->append(userdata_rename_button_);
    userdata_row->append(userdata_remove_button_);
    content_.append(*userdata_row);

    // 2. Play: the thing you do most often, so it comes first.
    active_title_.set_xalign(0.0f);
    active_title_.add_css_class("title-3");
    play_box_.append(active_title_);

    play_button_.set_label("Play");
    play_button_.set_tooltip_text("Launch the game.");
    play_button_.add_css_class("suggested-action");
    play_button_.set_hexpand(true);
    resume_button_.set_label("Resume Last World");
    resume_button_.set_tooltip_text("Skip main menu and quick-load last played world.");
    resume_button_.set_hexpand(true);

    // The folder buttons are rarely needed, so they hide in a menu.
    Gtk::Popover* popover = Gtk::make_managed<Gtk::Popover>();
    Gtk::Box* popover_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    popover_box->set_margin(6);
    Gtk::Button* game_dir_button = Gtk::make_managed<Gtk::Button>("Game folder");
    game_dir_button->set_tooltip_text("Open game installation directory (contains\ngame executable and built-in game content).");
    Gtk::Button* user_dir_button = Gtk::make_managed<Gtk::Button>("User data folder");
    user_dir_button->set_tooltip_text("Open user data directory (contains saves, \ngame configuration, user mods, sound, etc.)");
    popover_box->append(*game_dir_button);
    popover_box->append(*user_dir_button);
    popover->set_child(*popover_box);
    folder_menu_button_.set_label("Open folder");
    folder_menu_button_.set_popover(*popover);
    game_dir_button->signal_clicked().connect([this, popover]() {
        popover->popdown();
        open_folder(ctx_.paths.game_dir());
    });
    user_dir_button->signal_clicked().connect([this, popover]() {
        popover->popdown();
        open_folder(ctx_.paths.userdata());
    });

    Gtk::Box* play_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    play_row->append(play_button_);
    play_row->append(resume_button_);
    play_row->append(folder_menu_button_);
    play_box_.append(*play_row);
    content_.append(play_box_);

    // 3. The installs, only shown when there is more than one (or if asked for).
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
    delete_button_.add_css_class("destructive-action");
    delete_button_.set_tooltip_text("Delete the selected copy of the game.\n(Saves, settings and other user data\nare stored separately and will not be lost.)");
    Gtk::Box* installs_buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    make_active_button_.set_hexpand(true);
    delete_button_.set_hexpand(true);
    installs_buttons->append(make_active_button_);
    installs_buttons->append(delete_button_);
    installs_box_.append(*installs_buttons);
    content_.append(installs_box_);

    // 4. Install / update. A grid keeps the labels and widgets aligned.
    stable_button_.set_label("Stable");
    stable_button_.set_tooltip_text("Stable release channel (DDA and BN only).");
    experimental_button_.set_label("Experimental");
    experimental_button_.set_tooltip_text("Experimental release channel.");
    experimental_button_.set_group(stable_button_);
    refresh_button_.set_icon_name("view-refresh-symbolic");
    refresh_button_.set_tooltip_text("Re-retrieve the list of available game releases\nfrom GitHub (only for experimental releases).");

    Gtk::Box* channel_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    channel_box->append(stable_button_);
    channel_box->append(experimental_button_);
    refresh_button_.set_hexpand(true);
    refresh_button_.set_halign(Gtk::Align::END);
    channel_box->append(refresh_button_);

    builds_dropdown_.set_model(Gtk::StringList::create(std::vector<Glib::ustring>()));
    builds_dropdown_.set_hexpand(true);
    builds_dropdown_.set_tooltip_text("Choose which release of the game to install or update to.");

    install_new_button_.set_label("Install as New");
    install_new_button_.set_tooltip_text("Install the selected build as a new, separate copy of the game.");
    install_new_button_.set_hexpand(true);
    update_button_.set_label("Update Active Install");
    update_button_.set_tooltip_text("Replace the game install currently set as active\nwith the selected build.");
    update_button_.set_hexpand(true);
    Gtk::Box* install_buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    install_buttons->append(install_new_button_);
    install_buttons->append(update_button_);

    Gtk::Grid* grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(8);
    grid->set_column_spacing(8);
    grid->set_margin_top(8);
    grid->attach(*make_label("Channel:"), 0, 0);
    grid->attach(*channel_box, 1, 0);
    grid->attach(*make_label("Build:"), 0, 1);
    grid->attach(builds_dropdown_, 1, 1);
    grid->attach(*install_buttons, 0, 2, 2, 1);

    install_expander_.set_label("Install / update");
    install_expander_.set_child(*grid);
    content_.append(install_expander_);

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
    description_.set_visible(ctx_.settings.get_bool("show_game_desc"));

    game_dropdown_.property_selected().signal_changed().connect(sigc::mem_fun(*this, &TabGame::on_game_selected));
    stable_button_.signal_toggled().connect(sigc::mem_fun(*this, &TabGame::on_channel_toggled));
    builds_dropdown_.property_selected().signal_changed().connect(sigc::mem_fun(*this, &TabGame::on_build_selected));
    refresh_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::fetch_releases));
    install_new_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_install_new_clicked));
    update_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_update_clicked));
    play_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_play_clicked));
    resume_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_resume_clicked));
    make_active_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_make_active_clicked));
    delete_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_delete_clicked));
    installs_list_.signal_row_selected().connect(sigc::mem_fun(*this, &TabGame::on_installs_row_selected));
    installs_list_.signal_row_activated().connect(sigc::mem_fun(*this, &TabGame::on_installs_row_activated));
    userdata_dropdown_.property_selected().signal_changed().connect(sigc::mem_fun(*this, &TabGame::on_userdata_selected));
    userdata_add_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_userdata_add_clicked));
    userdata_rename_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_userdata_rename_clicked));
    userdata_remove_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabGame::on_userdata_remove_clicked));

    apply_game_choice();
    // The window is not shown yet, so ask once it is.
    Glib::signal_idle().connect_once([this]() { check_missing_userdata(); });
}

void TabGame::apply_settings()
{
    description_.set_visible(ctx_.settings.get_bool("show_game_desc"));
    refresh_installed();
    check_missing_userdata();
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
            Gtk::Box* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
            Gtk::Label* label = Gtk::make_managed<Gtk::Label>(it->first);
            label->set_xalign(0.0f);
            label->set_hexpand(true);
            row->append(*label);
            if (it->first == active)
            {
                Gtk::Label* tag = Gtk::make_managed<Gtk::Label>("active");
                tag->add_css_class("dim-label");
                row->append(*tag);
            }
            row->set_tooltip_text("Location:\n" + it->second + "\n(double-click to open folder)");
            installs_list_.append(*row);
            install_names_.push_back(it->first);
        }
    }
    make_active_button_.set_sensitive(false);
    delete_button_.set_sensitive(false);
    installs_box_.set_visible((have_install && g->second.size() > 1) || ctx_.settings.get_bool("always_show_installs"));

    // The play section.
    active_title_.set_text(have_install ? active : "No game installed yet");
    if (have_install)
        active_title_.remove_css_class("dim-label");
    else
        active_title_.add_css_class("dim-label");
    folder_menu_button_.set_visible(have_install);
    play_button_.set_sensitive(have_install);
    resume_button_.set_sensitive(have_install && ctx_.launcher.has_last_world());

    // The install buttons depend on the selected build.
    guint selected = builds_dropdown_.get_selected();
    bool build_selected = (selected != GTK_INVALID_LIST_POSITION && selected < releases_.size());
    bool same_build_installed = build_selected && have_install
                                && g->second.find(releases_[selected].name) != g->second.end()
                                && !ctx_.settings.get_bool("update_to_same_build_allowed");

    install_new_button_.set_sensitive(build_selected && !same_build_installed);
    update_button_.set_sensitive(build_selected && have_install && !same_build_installed);

    // Open the install section when there is nothing installed, close it once there is.
    if (had_install_ != (have_install ? 1 : 0))
    {
        had_install_ = have_install ? 1 : 0;
        install_expander_.set_expanded(!have_install);
    }

    updating_ = false;
    reload_userdata_list();
    ctx_.settings.save();
}

void TabGame::reload_userdata_list()
{
    updating_ = true;
    std::string active = ctx_.paths.userdata();
    std::vector<UserdataFolder> folders = ctx_.paths.custom_userdata();

    Glib::RefPtr<Gtk::StringList> model = Gtk::StringList::create(std::vector<Glib::ustring>());
    userdata_paths_.clear();
    model->append("Default");
    userdata_paths_.push_back(ctx_.paths.default_userdata());
    guint selected = 0;
    for (size_t i = 0; i < folders.size(); ++i)
    {
        model->append(folders[i].label);
        userdata_paths_.push_back(folders[i].path);
        if (folders[i].path == active)
            selected = (guint)(i + 1);
    }
    userdata_dropdown_.set_model(model);
    userdata_dropdown_.set_selected(selected);
    userdata_dropdown_.set_tooltip_text("The user data folder the game uses (saves, configuration,\nuser mods, sound, etc.):\n" + active);

    bool custom = (selected != 0);
    userdata_rename_button_.set_sensitive(custom);
    userdata_remove_button_.set_sensitive(custom);
    updating_ = false;
}

std::string TabGame::userdata_label(const std::string& path)
{
    std::vector<UserdataFolder> folders = ctx_.paths.custom_userdata();
    for (size_t i = 0; i < folders.size(); ++i)
    {
        if (folders[i].path == path)
            return folders[i].label;
    }
    return "Default";
}

void TabGame::on_userdata_selected()
{
    if (updating_)
        return;
    guint index = userdata_dropdown_.get_selected();
    if (index >= userdata_paths_.size())
        return;
    // A job on another tab may be working in the current userdata.
    if (runner_.busy())
    {
        ctx_.status.post("Wait for the current task to finish before changing the user data folder.", MSG_WARN);
        reload_userdata_list();
        return;
    }
    std::string path = userdata_paths_[index];
    ctx_.paths.set_active_userdata(path);
    ctx_.status.post("Using the user data folder \"" + userdata_label(path) + "\": " + path);
    refresh_installed();
    check_missing_userdata();
}

void TabGame::on_userdata_add_clicked()
{
    if (runner_.busy())
    {
        ctx_.status.post("Wait for the current task to finish before changing the user data folder.", MSG_WARN);
        return;
    }
    Glib::RefPtr<Gtk::FileDialog> dialog = Gtk::FileDialog::create();
    dialog->set_title("Choose a user data folder");
    dialog->select_folder(parent_, [this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
        Glib::RefPtr<Gio::File> file;
        try
        {
            file = dialog->select_folder_finish(result);
        }
        catch (const Glib::Error&)
        {
            return;  // Cancelled.
        }
        if (!file || file->get_path().empty())
            return;
        std::string path = file->get_path();
        dialogs::ask_text(
            parent_, "Name the user data folder", "The name shown in the list for " + path + ".",
            Glib::path_get_basename(path), "Add",
            [this](const std::string& label) { return ctx_.paths.check_userdata_label(label); },
            [this, path](const std::string& label) {
                std::string problem = ctx_.paths.add_userdata(path, label);
                if (!problem.empty())
                {
                    dialogs::tell(parent_, "Cannot add the user data folder", problem);
                    return;
                }
                // Added to be used, so it becomes the active one.
                std::vector<UserdataFolder> folders = ctx_.paths.custom_userdata();
                ctx_.paths.set_active_userdata(folders.back().path);
                ctx_.status.post("Using the user data folder \"" + label + "\": " + folders.back().path);
                refresh_installed();
            });
    });
}

void TabGame::on_userdata_rename_clicked()
{
    std::string path = ctx_.paths.userdata();
    if (ctx_.paths.is_default_userdata_active())
        return;
    dialogs::ask_text(
        parent_, "Rename the user data folder",
        "Only the name in this list changes. The folder stays " + path + ".", userdata_label(path), "Rename",
        [this, path](const std::string& label) { return ctx_.paths.check_userdata_label(label, path); },
        [this, path](const std::string& label) {
            std::string problem = ctx_.paths.rename_userdata(path, label);
            if (!problem.empty())
                dialogs::tell(parent_, "Cannot rename the user data folder", problem);
            refresh_installed();
        });
}

void TabGame::on_userdata_remove_clicked()
{
    std::string path = ctx_.paths.userdata();
    if (ctx_.paths.is_default_userdata_active())
        return;
    if (runner_.busy())
    {
        ctx_.status.post("Wait for the current task to finish before changing the user data folder.", MSG_WARN);
        return;
    }
    dialogs::confirm(parent_, "Remove \"" + userdata_label(path) + "\" from the list?",
                     "The launcher forgets this folder and uses the default one. Nothing is deleted: the folder "
                         + path + " and its files stay where they are, and you can add it again later.",
                     "Remove", [this, path]() {
                         ctx_.paths.remove_userdata(path);
                         ctx_.status.post("Removed " + path + " from the user data folders. Using the default one.");
                         refresh_installed();
                     });
}

// The game makes the folder again when it starts, so keeping it is fine.
void TabGame::check_missing_userdata()
{
    if (asking_missing_ || ctx_.paths.is_default_userdata_active())
        return;
    std::string path = ctx_.paths.userdata();
    if (Glib::file_test(path, Glib::FileTest::IS_DIR) || kept_missing_.count(path))
        return;

    asking_missing_ = true;
    std::vector<Glib::ustring> buttons;
    buttons.push_back("Keep");
    buttons.push_back("Remove from List");
    dialogs::ask(parent_, "The user data folder \"" + userdata_label(path) + "\" is missing",
                 path + " does not exist.\n\nKeep it, and the game creates and fills it in the next time it starts. "
                        "Or remove it from the list and use the default user data folder.",
                 buttons, [this, path](int choice) {
                     asking_missing_ = false;
                     if (choice == 1)
                     {
                         ctx_.paths.remove_userdata(path);
                         ctx_.status.post("Removed " + path + " from the user data folders. Using the default one.");
                     }
                     else
                     {
                         kept_missing_.insert(path);
                     }
                     refresh_installed();
                 });
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
    check_missing_userdata();
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

void TabGame::on_install_new_clicked()
{
    install_selected(false);
}

void TabGame::on_update_clicked()
{
    install_selected(true);
}

void TabGame::install_selected(bool update_active)
{
    guint selected = builds_dropdown_.get_selected();
    if (selected == GTK_INVALID_LIST_POSITION || selected >= releases_.size())
        return;
    Release release = releases_[selected];

    std::string update_path;
    std::string active_name;
    if (update_active)
    {
        std::string game = ctx_.settings.get_string("game");
        active_name = ctx_.settings.get_string("active_install_" + game);
        Paths::InstallsSummary::const_iterator g = installs_.find(game);
        if (g != installs_.end() && g->second.find(active_name) != g->second.end())
            update_path = g->second.find(active_name)->second;
    }

    DownloadQuestion question;
    question.url = release.url;
    question.filename = release.filename;
    if (update_path.empty())
    {
        question.title = "Install " + release.name + "?";
        question.detail = "It is installed as a new, separate copy of the game.";
        question.button = "Install";
    }
    else
    {
        question.title = "Update \"" + active_name + "\"?";
        question.detail = removal_detail(active_name, true);
        question.button = "Update";
    }

    confirm_download(ctx_, runner_, parent_, content_, question, [this, release, update_path]() {
        run_task([this, release, update_path]() { ctx_.installer.install_release(release, update_path); },
                 [this]() { refresh_installed(); });
    });
}

void TabGame::on_delete_clicked()
{
    std::string name = selected_install_name();
    if (name.empty())
        return;

    Glib::ustring title = "Uninstall \"" + name + "\"?";
    dialogs::confirm(parent_, title, removal_detail(name, false),
                     dialogs::removal_button(ctx_.settings.get_bool("use_trash")), [this, name]() {
                         run_task([this, name]() { ctx_.installer.remove_release_by_name(name); },
                                  [this]() { refresh_installed(); });
                     });
}

// What happens to an install's files when it is updated or uninstalled, for
// the confirmation. Updating replaces them and uninstalling removes them, so
// it also says where user data inside the game directory goes.
std::string TabGame::removal_detail(const std::string& install_name, bool update)
{
    bool use_trash = ctx_.settings.get_bool("use_trash");
    std::string detail = update ? "The current files of \"" + install_name + "\" " + dialogs::removal_text(use_trash)
                                      + ", and the new version is installed in their place."
                                : "The game files of \"" + install_name + "\" " + dialogs::removal_text(use_trash) + ".";

    std::vector<std::string> misplaced = ctx_.installer.check_game_dir_for_userdata(install_name);
    if (!misplaced.empty())
    {
        detail += "\n\nThe game directory also contains user data that is not in the user data directory:\n";
        for (size_t i = 0; i < misplaced.size(); ++i)
            detail += "\n" + misplaced[i];
        detail += use_trash ? "\n\nIt goes to the trash with the game files." : "\n\nIt will be lost.";
    }
    return detail;
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
