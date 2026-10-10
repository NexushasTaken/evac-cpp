#include "ui/tab_mods.h"

#include <algorithm>
#include <strings.h>

static const char* const NO_INFO_TEXT = "Select a mod in either list to see its details here.";

static Gtk::Label* make_label(const Glib::ustring& text)
{
    Gtk::Label* label = Gtk::make_managed<Gtk::Label>(text);
    label->set_xalign(0.0f);
    return label;
}

static bool name_less(const std::string& a, const std::string& b)
{
    return strcasecmp(a.c_str(), b.c_str()) < 0;
}

// Authors and maintainers are a list, but sometimes just a string.
static std::string to_text_list(const nlohmann::json& value)
{
    if (value.is_string())
        return value.get<std::string>();

    std::string result;
    if (value.is_array())
    {
        for (size_t i = 0; i < value.size(); ++i)
        {
            if (!value[i].is_string())
                continue;
            if (!result.empty())
                result += ", ";
            result += value[i].get<std::string>();
        }
    }
    return result;
}

TabMods::TabMods(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8),
      ctx_(ctx),
      runner_(runner),
      parent_(parent),
      updating_(false),
      content_(Gtk::Orientation::VERTICAL, 8)
{
    build_ui();
}

void TabMods::build_ui()
{
    set_margin(12);
    hint_label_.set_wrap(true);
    hint_label_.set_xalign(0.0f);
    append(hint_label_);
    content_.set_vexpand(true);
    append(content_);

    Gtk::Box* columns = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    columns->set_vexpand(true);
    columns->set_homogeneous(true);
    content_.append(*columns);

    // Both columns have the same structure: header, list, footer.

    // Installed mods.
    Gtk::Box* left = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    installed_caption_.set_xalign(0.0f);
    installed_caption_.set_hexpand(true);
    installed_caption_.add_css_class("heading");
    Gtk::Box* left_header = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    left_header->append(installed_caption_);
    left->append(*left_header);

    Gtk::ScrolledWindow* installed_scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    installed_scroller->set_vexpand(true);
    installed_scroller->set_has_frame(true);
    installed_scroller->set_child(installed_list_);
    left->append(*installed_scroller);

    show_stock_check_.set_label("Show stock");
    show_stock_check_.set_tooltip_text("Show mods from data/mods on this list.");
    show_stock_check_.set_hexpand(true);
    show_stock_check_.set_valign(Gtk::Align::CENTER);
    delete_button_.set_label("Delete Selected");
    delete_button_.set_tooltip_text("Remove selected mods from your game installation.");
    delete_button_.add_css_class("destructive-action");
    Gtk::Box* left_footer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    left_footer->append(show_stock_check_);
    left_footer->append(delete_button_);
    left->append(*left_footer);
    columns->append(*left);

    // The local repository.
    Gtk::Box* right = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    available_caption_.set_xalign(0.0f);
    available_caption_.set_hexpand(true);
    available_caption_.add_css_class("heading");
    kenan_button_.set_label("Get Kenan Modpack");
    kenan_button_.set_tooltip_text("Download Kenan Modpack and add its mods\nto the local repository.");
    Gtk::Box* right_header = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    right_header->append(available_caption_);
    right_header->append(kenan_button_);
    right->append(*right_header);

    Gtk::ScrolledWindow* available_scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    available_scroller->set_vexpand(true);
    available_scroller->set_has_frame(true);
    available_scroller->set_child(available_list_);
    right->append(*available_scroller);

    show_installed_check_.set_label("Show already installed");
    show_installed_check_.set_tooltip_text("Show already installed mods on this list\nin case you want to re-install them.");
    show_installed_check_.set_hexpand(true);
    show_installed_check_.set_valign(Gtk::Align::CENTER);
    add_all_button_.set_label("Add All");
    add_all_button_.set_tooltip_text("Add all listed mods to your game installation.");
    add_selected_button_.set_label("Add Selected");
    add_selected_button_.set_tooltip_text("Add selected mods to your game installation.");
    add_selected_button_.add_css_class("suggested-action");
    Gtk::Box* right_footer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    right_footer->append(show_installed_check_);
    right_footer->append(add_all_button_);
    right_footer->append(add_selected_button_);
    right->append(*right_footer);
    columns->append(*right);

    // Empty lists say what to do.
    Gtk::Label* installed_placeholder = Gtk::make_managed<Gtk::Label>("No mods installed.");
    installed_placeholder->add_css_class("dim-label");
    installed_placeholder->set_margin(12);
    installed_list_.set_placeholder(*installed_placeholder);
    Gtk::Label* available_placeholder =
        Gtk::make_managed<Gtk::Label>("The local repository is empty.\nUse Get Kenan Modpack to fill it.");
    available_placeholder->add_css_class("dim-label");
    available_placeholder->set_justify(Gtk::Justification::CENTER);
    available_placeholder->set_margin(12);
    available_list_.set_placeholder(*available_placeholder);

    // Details of the selected mod.
    info_label_.set_wrap(true);
    info_label_.set_xalign(0.0f);
    info_label_.set_yalign(0.0f);
    info_label_.set_use_markup(true);
    info_label_.set_selectable(true);
    Gtk::ScrolledWindow* info_scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
    info_scroller->set_min_content_height(110);
    info_scroller->set_has_frame(true);
    info_scroller->set_child(info_label_);
    content_.append(*info_scroller);

    installed_list_.set_selection_mode(Gtk::SelectionMode::MULTIPLE);
    available_list_.set_selection_mode(Gtk::SelectionMode::MULTIPLE);

    installed_list_.signal_selected_rows_changed().connect(sigc::mem_fun(*this, &TabMods::on_installed_selection_changed));
    available_list_.signal_selected_rows_changed().connect(sigc::mem_fun(*this, &TabMods::on_available_selection_changed));
    show_stock_check_.signal_toggled().connect(sigc::mem_fun(*this, &TabMods::on_show_stock_toggled));
    show_installed_check_.signal_toggled().connect(sigc::mem_fun(*this, &TabMods::on_show_installed_toggled));
    delete_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabMods::on_delete_clicked));
    add_selected_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabMods::on_add_selected_clicked));
    add_all_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabMods::on_add_all_clicked));
    kenan_button_.signal_clicked().connect(sigc::mem_fun(*this, &TabMods::on_kenan_clicked));
}

void TabMods::show_unavailable(const Glib::ustring& message)
{
    hint_label_.set_text(message);
    hint_label_.set_visible(true);
    content_.set_visible(false);
}

void TabMods::refresh()
{
    std::string game = ctx_.settings.get_string("game");
    Paths::InstallsSummary installs = ctx_.paths.installs_summary();
    if (installs.find(game) == installs.end())
    {
        show_unavailable("Install the game first (on the Game tab).");
        return;
    }
    hint_label_.set_visible(false);
    content_.set_visible(true);

    updating_ = true;
    show_stock_check_.set_active(ctx_.settings.get_bool("show_stock_mods"));
    show_installed_check_.set_active(ctx_.settings.get_bool("show_installed_mods_in_available"));
    updating_ = false;

    bool have_pack = (ModManager::modpack_for_game(game) != nullptr);
    kenan_button_.set_sensitive(have_pack);
    if (!have_pack)
        kenan_button_.set_tooltip_text("There is no modpack for this game.");

    info_label_.set_markup(NO_INFO_TEXT);
    ctx_.mods.refresh_installed();
    ctx_.mods.refresh_available();
    reload_installed();
    reload_available();
}

std::set<int> TabMods::selected_rows(Gtk::ListBox& list)
{
    std::set<int> result;
    std::vector<Gtk::ListBoxRow*> rows = list.get_selected_rows();
    for (size_t i = 0; i < rows.size(); ++i)
        result.insert(rows[i]->get_index());
    return result;
}

void TabMods::fill_list(Gtk::ListBox& list, const std::vector<ModRow>& rows)
{
    updating_ = true;
    while (Gtk::ListBoxRow* row = list.get_row_at_index(0))
        list.remove(*row);
    for (size_t i = 0; i < rows.size(); ++i)
    {
        Gtk::Box* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
        Gtk::Label* label = make_label(rows[i].name);
        label->set_hexpand(true);
        box->append(*label);
        if (!rows[i].badge.empty())
        {
            Gtk::Label* badge = Gtk::make_managed<Gtk::Label>(rows[i].badge);
            badge->add_css_class("dim-label");
            box->append(*badge);
        }
        box->set_tooltip_text("Location: " + rows[i].location);
        if (rows[i].greyed)
            box->set_sensitive(false);
        list.append(*box);
    }
    updating_ = false;
}

// Which mods are listed on the left depends on the settings. See ModStatus.
void TabMods::reload_installed()
{
    bool show_stock = ctx_.settings.get_bool("show_stock_mods");
    bool show_obsolete = ctx_.settings.get_bool("show_obsolete_mods");
    int hidden = 0;

    installed_rows_.clear();
    const std::map<std::string, ModEntry>& installed = ctx_.mods.installed();
    for (std::map<std::string, ModEntry>::const_iterator it = installed.begin(); it != installed.end(); ++it)
    {
        ModStatus status = ctx_.mods.mod_status(it->first);
        bool show = false;
        if (status == MOD_NOT_INSTALLED || status == MOD_INSTALLED)
        {
            show = true;
        }
        else if (status == MOD_STOCK_OBSOLETE || status == MOD_INSTALLED_MODIFIED_ID)
        {
            if (show_obsolete)
            {
                if (show_stock)
                    show = true;
                else
                    ++hidden;
            }
        }
        else if (status == MOD_STOCK)
        {
            show = show_stock;
            if (!show)
                ++hidden;
        }

        if (!show)
            continue;
        ModRow row;
        row.id = it->first;
        row.name = it->second.name;
        if (show_obsolete && status == MOD_STOCK_OBSOLETE)
            row.name += " [obsolete]";
        row.location = it->second.location;
        row.greyed = it->second.is_stock;
        if (row.greyed)
            row.badge = "stock";
        installed_rows_.push_back(row);
    }
    std::sort(installed_rows_.begin(), installed_rows_.end(),
              [](const ModRow& a, const ModRow& b) { return name_less(a.name, b.name); });

    std::string caption = "Installed";
    if (hidden > 0)
        caption += " (" + std::to_string(hidden) + " hidden)";
    installed_caption_.set_text(caption + ":");

    fill_list(installed_list_, installed_rows_);
    previous_installed_selection_.clear();
    delete_button_.set_sensitive(false);
}

void TabMods::reload_available()
{
    bool include_installed = ctx_.settings.get_bool("show_installed_mods_in_available");
    int hidden = 0;

    available_rows_.clear();
    const std::map<std::string, ModEntry>& available = ctx_.mods.available();
    for (std::map<std::string, ModEntry>::const_iterator it = available.begin(); it != available.end(); ++it)
    {
        ModStatus status = ctx_.mods.mod_status(it->first);
        bool installable = (status == MOD_NOT_INSTALLED || status == MOD_STOCK_OBSOLETE);
        if (!installable && !include_installed)
        {
            ++hidden;
            continue;
        }

        ModRow row;
        row.id = it->first;
        row.name = it->second.name;
        row.location = it->second.location;
        row.greyed = !installable;
        if (row.greyed)
            row.badge = "installed";
        available_rows_.push_back(row);
    }
    std::sort(available_rows_.begin(), available_rows_.end(),
              [](const ModRow& a, const ModRow& b) { return name_less(a.name, b.name); });

    std::string caption = "Local repository";
    if (hidden > 0)
        caption += " (" + std::to_string(hidden) + " hidden)";
    available_caption_.set_text(caption + ":");

    fill_list(available_list_, available_rows_);
    previous_available_selection_.clear();
    add_selected_button_.set_sensitive(false);
    add_all_button_.set_sensitive(!available_rows_.empty());
}

void TabMods::show_mod_info(const std::string& id, bool from_available)
{
    const std::map<std::string, ModEntry>& mods = from_available ? ctx_.mods.available() : ctx_.mods.installed();
    std::map<std::string, ModEntry>::const_iterator it = mods.find(id);
    if (it == mods.end())
        return;
    const nlohmann::json& info = it->second.modinfo;

    Glib::ustring text = "<u>Name:</u> " + Glib::Markup::escape_text(it->second.name);
    text += " (<u>ID:</u> " + Glib::Markup::escape_text(id) + ")";
    if (info.contains("authors"))
        text += "\n<u>Authors:</u> " + Glib::Markup::escape_text(to_text_list(info["authors"]));
    if (info.contains("maintainers"))
        text += "\n<u>Maintainers:</u> " + Glib::Markup::escape_text(to_text_list(info["maintainers"]));
    if (info.contains("category") && info["category"].is_string())
        text += "\n<u>Category:</u> " + Glib::Markup::escape_text(info["category"].get<std::string>());
    if (info.contains("description") && info["description"].is_string())
        text += "\n<u>Description:</u> " + Glib::Markup::escape_text(info["description"].get<std::string>());
    info_label_.set_markup(text);
}

// The mod to show details of: the one that was just selected, or if the last
// change was a deselection, the last one that is still selected.
static int pick_active(const std::set<int>& before, const std::set<int>& now)
{
    int active = -1;
    for (std::set<int>::const_iterator it = now.begin(); it != now.end(); ++it)
    {
        if (before.find(*it) == before.end())
            active = *it;
    }
    if (active < 0 && !now.empty())
        active = *now.rbegin();
    return active;
}

void TabMods::on_installed_selection_changed()
{
    if (updating_)
        return;
    std::set<int> now = selected_rows(installed_list_);
    int active = pick_active(previous_installed_selection_, now);
    previous_installed_selection_ = now;

    if (active >= 0 && (size_t)active < installed_rows_.size())
        show_mod_info(installed_rows_[active].id, false);

    // Stock mods can't be deleted, so there must be something else selected.
    bool can_delete = false;
    for (std::set<int>::const_iterator it = now.begin(); it != now.end(); ++it)
    {
        if ((size_t)*it < installed_rows_.size() && !installed_rows_[*it].greyed)
            can_delete = true;
    }
    delete_button_.set_sensitive(can_delete);
}

void TabMods::on_available_selection_changed()
{
    if (updating_)
        return;
    std::set<int> now = selected_rows(available_list_);
    int active = pick_active(previous_available_selection_, now);
    previous_available_selection_ = now;

    if (active >= 0 && (size_t)active < available_rows_.size())
        show_mod_info(available_rows_[active].id, true);

    // As in the original launcher: adding is only possible if the selection has a
    // mod that is not installed (or is installed as an obsolete stock mod).
    bool can_add = false;
    const std::map<std::string, ModEntry>& installed = ctx_.mods.installed();
    for (std::set<int>::const_iterator it = now.begin(); it != now.end(); ++it)
    {
        if ((size_t)*it >= available_rows_.size())
            continue;
        std::map<std::string, ModEntry>::const_iterator inst = installed.find(available_rows_[*it].id);
        if (inst == installed.end() || inst->second.is_obsolete)
            can_add = true;
    }
    add_selected_button_.set_sensitive(can_add);
}

void TabMods::on_show_stock_toggled()
{
    if (updating_)
        return;
    ctx_.settings.set_bool("show_stock_mods", show_stock_check_.get_active());
    ctx_.settings.save();
    reload_installed();
}

void TabMods::on_show_installed_toggled()
{
    if (updating_)
        return;
    ctx_.settings.set_bool("show_installed_mods_in_available", show_installed_check_.get_active());
    ctx_.settings.save();
    reload_available();
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is done.
void TabMods::run_task(const std::function<void()>& work)
{
    content_.set_sensitive(false);
    bool started = runner_.run(work, [this]() {
        content_.set_sensitive(true);
        reload_installed();
        reload_available();
    });
    if (!started)
        content_.set_sensitive(true);
}

// Shows a message with buttons; `answer` gets the number of the button pressed,
// 0 being the first one (always cancel, also when the dialog is dismissed).
void TabMods::ask(const Glib::ustring& title, const Glib::ustring& detail, const std::vector<Glib::ustring>& buttons,
                  const std::function<void(int)>& answer)
{
    Glib::RefPtr<Gtk::AlertDialog> dialog = Gtk::AlertDialog::create(title);
    dialog->set_detail(detail);
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
            choice = 0;
        }
        answer(choice);
    });
}

void TabMods::on_delete_clicked()
{
    std::vector<std::string> ids;
    int skipped = 0;
    std::set<int> selection = selected_rows(installed_list_);
    for (std::set<int>::const_iterator it = selection.begin(); it != selection.end(); ++it)
    {
        if ((size_t)*it >= installed_rows_.size())
            continue;
        if (installed_rows_[*it].greyed)
            ++skipped;
        else
            ids.push_back(installed_rows_[*it].id);
    }

    if (skipped == 1)
        ctx_.status.post("One of the selected mods is stock and cannot be deleted.");
    else if (skipped > 1)
        ctx_.status.post(std::to_string(skipped) + " of the selected mods are stock and cannot be deleted.");
    if (ids.empty())
        return;

    if (ids.size() == 1)
    {
        run_task([this, ids]() { ctx_.mods.delete_mods(ids); });
        return;
    }

    std::vector<Glib::ustring> buttons;
    buttons.push_back("Cancel");
    buttons.push_back("Delete");
    ask("Delete the mods?", "Deleting " + std::to_string(ids.size()) + " mods.", buttons, [this, ids](int choice) {
        if (choice == 1)
            run_task([this, ids]() { ctx_.mods.delete_mods(ids); });
    });
}

void TabMods::on_add_selected_clicked()
{
    std::vector<std::string> new_ids;        // Install from scratch.
    std::vector<std::string> reinstall_ids;  // Delete the installed copy first.
    int stock = 0;

    std::set<int> selection = selected_rows(available_list_);
    for (std::set<int>::const_iterator it = selection.begin(); it != selection.end(); ++it)
    {
        if ((size_t)*it >= available_rows_.size())
            continue;
        const std::string& id = available_rows_[*it].id;
        ModStatus status = ctx_.mods.mod_status(id);
        if (status == MOD_STOCK)
            ++stock;
        else if (status == MOD_INSTALLED || status == MOD_INSTALLED_MODIFIED_ID)
            reinstall_ids.push_back(id);
        else
            new_ids.push_back(id);
    }

    if (stock == 1)
        ctx_.status.post("One mod already comes with the game, so it will not be installed.");
    else if (stock > 1)
        ctx_.status.post(std::to_string(stock) + " mods already come with the game, so they will not be installed.");

    if (reinstall_ids.empty())
    {
        install_mods(new_ids, reinstall_ids);
        return;
    }

    std::string detail = (reinstall_ids.size() == 1)
                             ? "One of the selected mods is already installed.\nDelete and reinstall it?"
                             : std::to_string(reinstall_ids.size()) + " of the selected mods are already installed.\nDelete and reinstall them?";
    std::vector<Glib::ustring> buttons;
    buttons.push_back("Cancel");
    buttons.push_back("No, skip them");
    buttons.push_back("Yes, reinstall");
    ask("Reinstall mods?", detail, buttons, [this, new_ids, reinstall_ids](int choice) {
        if (choice == 2)
            install_mods(new_ids, reinstall_ids);
        else if (choice == 1)
            install_mods(new_ids, std::vector<std::string>());
    });
}

void TabMods::install_mods(const std::vector<std::string>& new_ids, const std::vector<std::string>& reinstall_ids)
{
    if (new_ids.empty() && reinstall_ids.empty())
        return;

    run_task([this, new_ids, reinstall_ids]() {
        if (!reinstall_ids.empty())
            ctx_.mods.delete_mods(reinstall_ids);

        std::vector<std::string> all = new_ids;
        all.insert(all.end(), reinstall_ids.begin(), reinstall_ids.end());
        ctx_.mods.install_mods(all);
    });
}

void TabMods::on_add_all_clicked()
{
    available_list_.select_all();
    on_add_selected_clicked();
}

void TabMods::on_kenan_clicked()
{
    const Modpack* pack = ModManager::modpack_for_game(ctx_.settings.get_string("game"));
    if (!pack)
        return;
    Modpack copy = *pack;
    run_task([this, copy]() { ctx_.mods.retrieve_modpack(copy); });
}
