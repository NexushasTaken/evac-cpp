#include "ui/tab_mods.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

#include "core/fs.h"
#include "ui/dialogs.h"
#include "ui/download_prompt.h"
#include "ui/qt_helpers.h"

static const char* const NO_INFO_TEXT =
    "Select a mod in either list to see its details here.";

// Case-insensitive, like the original (strcasecmp is not on Windows).
static bool name_less(const std::string& a, const std::string& b) {
  return QString::compare(qstr(a), qstr(b), Qt::CaseInsensitive) < 0;
}

// Authors and maintainers are a list, but sometimes just a string.
static std::string to_text_list(const nlohmann::json& value) {
  if (value.is_string()) return value.get<std::string>();

  std::string result;
  if (value.is_array()) {
    for (size_t i = 0; i < value.size(); ++i) {
      if (!value[i].is_string()) continue;
      if (!result.empty()) result += ", ";
      result += value[i].get<std::string>();
    }
  }
  return result;
}

// A list with the name, and the reason it is greyed (if it is) on the right.
static QTreeWidget* make_mod_list() {
  QTreeWidget* list = new QTreeWidget();
  list->setColumnCount(2);
  list->setHeaderHidden(true);
  list->setRootIsDecorated(false);
  list->setSelectionMode(QAbstractItemView::ExtendedSelection);
  list->header()->setStretchLastSection(false);
  list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  return list;
}

TabMods::TabMods(AppContext& ctx, TaskRunner& runner, QWidget& parent)
    : ctx_(ctx), runner_(runner), parent_(parent), updating_(false) {
  build_ui();
}

void TabMods::build_ui() {
  QVBoxLayout* outer = new QVBoxLayout(this);
  outer->setContentsMargins(12, 12, 12, 12);
  hint_label_ = make_label("");
  hint_label_->setWordWrap(true);
  hint_label_->setAlignment(
      Qt::AlignLeft | Qt::AlignTop);  // Not in the middle of the empty tab.
  outer->addWidget(hint_label_);
  content_ = new QWidget();
  outer->addWidget(content_, 1);

  QVBoxLayout* content = new QVBoxLayout(content_);
  content->setContentsMargins(0, 0, 0, 0);
  content->setSpacing(8);
  QHBoxLayout* columns = new QHBoxLayout();
  columns->setSpacing(12);
  content->addLayout(columns, 1);

  // Both columns have the same structure: header, list, footer.

  // Installed mods.
  QVBoxLayout* left = new QVBoxLayout();
  left->setSpacing(6);
  installed_caption_ = make_heading("");
  QHBoxLayout* left_header = new QHBoxLayout();
  left_header->addWidget(installed_caption_, 1);
  left->addLayout(left_header);

  installed_list_ = make_mod_list();
  left->addWidget(installed_list_, 1);

  show_stock_check_ = new QCheckBox("Show stock");
  show_stock_check_->setToolTip("Show mods from data/mods on this list.");
  delete_button_ = new QPushButton("Delete Selected");
  delete_button_->setToolTip(
      "Remove selected mods from your game installation.");
  QHBoxLayout* left_footer = new QHBoxLayout();
  left_footer->addWidget(show_stock_check_, 1);
  left_footer->addWidget(delete_button_);
  left->addLayout(left_footer);
  columns->addLayout(left, 1);

  // The local repository.
  QVBoxLayout* right = new QVBoxLayout();
  right->setSpacing(6);
  available_caption_ = make_heading("");
  kenan_button_ = new QPushButton("Get Kenan Modpack");
  kenan_button_->setToolTip(
      "Download Kenan Modpack and add its mods\nto the local repository.");
  QHBoxLayout* right_header = new QHBoxLayout();
  right_header->addWidget(available_caption_, 1);
  right_header->addWidget(kenan_button_);
  right->addLayout(right_header);

  available_list_ = make_mod_list();
  right->addWidget(available_list_, 1);

  show_installed_check_ = new QCheckBox("Show already installed");
  show_installed_check_->setToolTip(
      "Show already installed mods on this list\nin case you want to "
      "re-install them.");
  add_all_button_ = new QPushButton("Add All");
  add_all_button_->setToolTip("Add all listed mods to your game installation.");
  add_selected_button_ = new QPushButton("Add Selected");
  add_selected_button_->setToolTip(
      "Add selected mods to your game installation.");
  QHBoxLayout* right_footer = new QHBoxLayout();
  right_footer->addWidget(show_installed_check_, 1);
  right_footer->addWidget(add_all_button_);
  right_footer->addWidget(add_selected_button_);
  right->addLayout(right_footer);
  columns->addLayout(right, 1);

  // Details of the selected mod.
  info_view_ = new QTextBrowser();
  info_view_->setOpenExternalLinks(true);
  info_view_->setFixedHeight(110);
  content->addWidget(info_view_);

  connect(installed_list_, &QTreeWidget::itemSelectionChanged, this,
          &TabMods::on_installed_selection_changed);
  connect(available_list_, &QTreeWidget::itemSelectionChanged, this,
          &TabMods::on_available_selection_changed);
  connect(show_stock_check_, &QCheckBox::toggled, this,
          &TabMods::on_show_stock_toggled);
  connect(show_installed_check_, &QCheckBox::toggled, this,
          &TabMods::on_show_installed_toggled);
  connect(delete_button_, &QPushButton::clicked, this,
          &TabMods::on_delete_clicked);
  connect(add_selected_button_, &QPushButton::clicked, this,
          &TabMods::on_add_selected_clicked);
  connect(add_all_button_, &QPushButton::clicked, this,
          &TabMods::on_add_all_clicked);
  connect(kenan_button_, &QPushButton::clicked, this,
          &TabMods::on_kenan_clicked);
}

void TabMods::show_unavailable(const QString& message) {
  hint_label_->setText(message);
  hint_label_->setVisible(true);
  content_->setVisible(false);
}

void TabMods::refresh() {
  std::string game = ctx_.settings.get_string("game");
  Paths::InstallsSummary installs = ctx_.paths.installs_summary();
  if (installs.find(game) == installs.end()) {
    show_unavailable("Install the game first (on the Game tab).");
    return;
  }
  hint_label_->setVisible(false);
  content_->setVisible(true);

  updating_ = true;
  show_stock_check_->setChecked(ctx_.settings.get_bool("show_stock_mods"));
  show_installed_check_->setChecked(
      ctx_.settings.get_bool("show_installed_mods_in_available"));
  updating_ = false;

  bool have_pack = (ModManager::modpack_for_game(game) != nullptr);
  kenan_button_->setEnabled(have_pack);
  if (!have_pack)
    kenan_button_->setToolTip("There is no modpack for this game.");

  info_view_->setPlainText(NO_INFO_TEXT);
  ctx_.mods.refresh_installed();
  ctx_.mods.refresh_available();
  reload_installed();
  reload_available();
}

std::set<int> TabMods::selected_rows(QTreeWidget* list) {
  std::set<int> result;
  QList<QTreeWidgetItem*> items = list->selectedItems();
  for (int i = 0; i < items.size(); ++i)
    result.insert(list->indexOfTopLevelItem(items[i]));
  return result;
}

// Empty lists say what to do, in a row that can't be selected.
void TabMods::fill_list(QTreeWidget* list, const std::vector<ModRow>& rows,
                        const QString& placeholder) {
  updating_ = true;
  list->clear();
  QBrush dim = dim_brush(list);
  for (size_t i = 0; i < rows.size(); ++i) {
    QTreeWidgetItem* item = new QTreeWidgetItem(list);
    item->setText(0, qstr(rows[i].name));
    item->setText(1, qstr(rows[i].badge));
    item->setForeground(1, dim);
    if (rows[i].greyed) item->setForeground(0, dim);
    item->setToolTip(0, qstr("Location: " + rows[i].location));
    item->setToolTip(1, item->toolTip(0));
  }
  if (rows.empty()) {
    QTreeWidgetItem* item = new QTreeWidgetItem(list);
    item->setText(0, placeholder);
    item->setForeground(0, dim);
    item->setFlags(Qt::NoItemFlags);
  }
  updating_ = false;
}

// Which mods are listed on the left depends on the settings. See ModStatus.
void TabMods::reload_installed() {
  bool show_stock = ctx_.settings.get_bool("show_stock_mods");
  bool show_obsolete = ctx_.settings.get_bool("show_obsolete_mods");
  int hidden = 0;

  installed_rows_.clear();
  const std::map<std::string, ModEntry>& installed = ctx_.mods.installed();
  for (std::map<std::string, ModEntry>::const_iterator it = installed.begin();
       it != installed.end(); ++it) {
    ModStatus status = ctx_.mods.mod_status(it->first);
    bool show = false;
    if (status == MOD_NOT_INSTALLED || status == MOD_INSTALLED) {
      show = true;
    } else if (status == MOD_STOCK_OBSOLETE ||
               status == MOD_INSTALLED_MODIFIED_ID) {
      if (show_obsolete) {
        if (show_stock)
          show = true;
        else
          ++hidden;
      }
    } else if (status == MOD_STOCK) {
      show = show_stock;
      if (!show) ++hidden;
    }

    if (!show) continue;
    ModRow row;
    row.id = it->first;
    row.name = it->second.name;
    if (show_obsolete && status == MOD_STOCK_OBSOLETE)
      row.name += " [obsolete]";
    row.location = to_utf8(it->second.location);
    row.greyed = it->second.is_stock;
    if (row.greyed) row.badge = "stock";
    installed_rows_.push_back(row);
  }
  std::sort(installed_rows_.begin(), installed_rows_.end(),
            [](const ModRow& a, const ModRow& b) {
              return name_less(a.name, b.name);
            });

  std::string caption = "Installed";
  if (hidden > 0) caption += " (" + std::to_string(hidden) + " hidden)";
  installed_caption_->setText(qstr(caption + ":"));

  fill_list(installed_list_, installed_rows_, "No mods installed.");
  previous_installed_selection_.clear();
  delete_button_->setEnabled(false);
}

void TabMods::reload_available() {
  bool include_installed =
      ctx_.settings.get_bool("show_installed_mods_in_available");
  int hidden = 0;

  available_rows_.clear();
  const std::map<std::string, ModEntry>& available = ctx_.mods.available();
  for (std::map<std::string, ModEntry>::const_iterator it = available.begin();
       it != available.end(); ++it) {
    ModStatus status = ctx_.mods.mod_status(it->first);
    bool installable =
        (status == MOD_NOT_INSTALLED || status == MOD_STOCK_OBSOLETE);
    if (!installable && !include_installed) {
      ++hidden;
      continue;
    }

    ModRow row;
    row.id = it->first;
    row.name = it->second.name;
    row.location = to_utf8(it->second.location);
    row.greyed = !installable;
    if (row.greyed) row.badge = "installed";
    available_rows_.push_back(row);
  }
  std::sort(available_rows_.begin(), available_rows_.end(),
            [](const ModRow& a, const ModRow& b) {
              return name_less(a.name, b.name);
            });

  std::string caption = "Local repository";
  if (hidden > 0) caption += " (" + std::to_string(hidden) + " hidden)";
  available_caption_->setText(qstr(caption + ":"));

  fill_list(available_list_, available_rows_,
            "The local repository is empty. Use Get Kenan Modpack to fill it.");
  previous_available_selection_.clear();
  add_selected_button_->setEnabled(false);
  add_all_button_->setEnabled(!available_rows_.empty());
}

void TabMods::show_mod_info(const std::string& id, bool from_available) {
  const std::map<std::string, ModEntry>& mods =
      from_available ? ctx_.mods.available() : ctx_.mods.installed();
  std::map<std::string, ModEntry>::const_iterator it = mods.find(id);
  if (it == mods.end()) return;
  const nlohmann::json& info = it->second.modinfo;

  QString text = "<u>Name:</u> " + escape(it->second.name);
  text += " (<u>ID:</u> " + escape(id) + ")";
  if (info.contains("authors"))
    text += "<br><u>Authors:</u> " + escape(to_text_list(info["authors"]));
  if (info.contains("maintainers"))
    text +=
        "<br><u>Maintainers:</u> " + escape(to_text_list(info["maintainers"]));
  if (info.contains("category") && info["category"].is_string())
    text +=
        "<br><u>Category:</u> " + escape(info["category"].get<std::string>());
  if (info.contains("description") && info["description"].is_string())
    text += "<br><u>Description:</u> " +
            escape(info["description"].get<std::string>());
  info_view_->setHtml(text);
}

// The mod to show details of: the one that was just selected, or if the last
// change was a deselection, the last one that is still selected.
static int pick_active(const std::set<int>& before, const std::set<int>& now) {
  int active = -1;
  for (std::set<int>::const_iterator it = now.begin(); it != now.end(); ++it) {
    if (before.find(*it) == before.end()) active = *it;
  }
  if (active < 0 && !now.empty()) active = *now.rbegin();
  return active;
}

void TabMods::on_installed_selection_changed() {
  if (updating_) return;
  std::set<int> now = selected_rows(installed_list_);
  int active = pick_active(previous_installed_selection_, now);
  previous_installed_selection_ = now;

  if (active >= 0 && (size_t)active < installed_rows_.size())
    show_mod_info(installed_rows_[active].id, false);

  // Stock mods can't be deleted, so there must be something else selected.
  bool can_delete = false;
  for (std::set<int>::const_iterator it = now.begin(); it != now.end(); ++it) {
    if (*it >= 0 && (size_t)*it < installed_rows_.size() &&
        !installed_rows_[*it].greyed)
      can_delete = true;
  }
  delete_button_->setEnabled(can_delete);
}

void TabMods::on_available_selection_changed() {
  if (updating_) return;
  std::set<int> now = selected_rows(available_list_);
  int active = pick_active(previous_available_selection_, now);
  previous_available_selection_ = now;

  if (active >= 0 && (size_t)active < available_rows_.size())
    show_mod_info(available_rows_[active].id, true);

  // As in the original launcher: adding is only possible if the selection has a
  // mod that is not installed (or is installed as an obsolete stock mod).
  bool can_add = false;
  const std::map<std::string, ModEntry>& installed = ctx_.mods.installed();
  for (std::set<int>::const_iterator it = now.begin(); it != now.end(); ++it) {
    if (*it < 0 || (size_t)*it >= available_rows_.size()) continue;
    std::map<std::string, ModEntry>::const_iterator inst =
        installed.find(available_rows_[*it].id);
    if (inst == installed.end() || inst->second.is_obsolete) can_add = true;
  }
  add_selected_button_->setEnabled(can_add);
}

void TabMods::on_show_stock_toggled() {
  if (updating_) return;
  ctx_.settings.set_bool("show_stock_mods", show_stock_check_->isChecked());
  ctx_.settings.save();
  reload_installed();
}

void TabMods::on_show_installed_toggled() {
  if (updating_) return;
  ctx_.settings.set_bool("show_installed_mods_in_available",
                         show_installed_check_->isChecked());
  ctx_.settings.save();
  reload_available();
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is
// done.
void TabMods::run_task(const std::function<void()>& work) {
  content_->setEnabled(false);
  bool started = runner_.run(work, [this]() {
    content_->setEnabled(true);
    reload_installed();
    reload_available();
  });
  if (!started) content_->setEnabled(true);
}

void TabMods::on_delete_clicked() {
  std::vector<std::string> ids;
  int skipped = 0;
  std::set<int> selection = selected_rows(installed_list_);
  for (std::set<int>::const_iterator it = selection.begin();
       it != selection.end(); ++it) {
    if (*it < 0 || (size_t)*it >= installed_rows_.size()) continue;
    if (installed_rows_[*it].greyed)
      ++skipped;
    else
      ids.push_back(installed_rows_[*it].id);
  }

  if (skipped == 1)
    ctx_.status.post(
        "One of the selected mods is stock and cannot be deleted.");
  else if (skipped > 1)
    ctx_.status.post(std::to_string(skipped) +
                     " of the selected mods are stock and cannot be deleted.");
  if (ids.empty()) return;

  bool use_trash = ctx_.settings.get_bool("use_trash");
  std::string detail;
  if (ids.size() == 1) {
    std::map<std::string, ModEntry>::const_iterator mod =
        ctx_.mods.installed().find(ids[0]);
    std::string name =
        (mod != ctx_.mods.installed().end()) ? mod->second.name : ids[0];
    detail = "\"" + name + "\" " + dialogs::removal_text(use_trash) + ".";
  } else {
    detail = std::to_string(ids.size()) + " mods " +
             dialogs::removal_text(use_trash) + ".";
  }
  dialogs::confirm(
      parent_, ids.size() == 1 ? "Delete the mod?" : "Delete the mods?",
      qstr(detail), qstr(dialogs::removal_button(use_trash)), [this, ids]() {
        run_task([this, ids]() { ctx_.mods.delete_mods(ids); });
      });
}

void TabMods::on_add_selected_clicked() {
  std::vector<std::string> new_ids;        // Install from scratch.
  std::vector<std::string> reinstall_ids;  // Delete the installed copy first.
  int stock = 0;

  std::set<int> selection = selected_rows(available_list_);
  for (std::set<int>::const_iterator it = selection.begin();
       it != selection.end(); ++it) {
    if (*it < 0 || (size_t)*it >= available_rows_.size()) continue;
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
    ctx_.status.post(
        "One mod already comes with the game, so it will not be installed.");
  else if (stock > 1)
    ctx_.status.post(
        std::to_string(stock) +
        " mods already come with the game, so they will not be installed.");

  if (reinstall_ids.empty()) {
    install_mods(new_ids, reinstall_ids);
    return;
  }

  std::string old_copies = ctx_.settings.get_bool("use_trash")
                               ? "The installed copies go to the trash."
                               : "The installed copies are deleted for good.";
  std::string detail = (reinstall_ids.size() == 1)
                           ? "One of the selected mods is already "
                             "installed.\nDelete and reinstall it?"
                           : std::to_string(reinstall_ids.size()) +
                                 " of the selected mods are already "
                                 "installed.\nDelete and reinstall them?";
  detail += "\n" + old_copies;
  QStringList buttons;
  buttons << "Cancel" << "No, skip them" << "Yes, reinstall";
  dialogs::ask(parent_, "Reinstall mods?", qstr(detail), buttons,
               [this, new_ids, reinstall_ids](int choice) {
                 if (choice == 2)
                   install_mods(new_ids, reinstall_ids);
                 else if (choice == 1)
                   install_mods(new_ids, std::vector<std::string>());
               });
}

void TabMods::install_mods(const std::vector<std::string>& new_ids,
                           const std::vector<std::string>& reinstall_ids) {
  if (new_ids.empty() && reinstall_ids.empty()) return;

  run_task([this, new_ids, reinstall_ids]() {
    if (!reinstall_ids.empty()) ctx_.mods.delete_mods(reinstall_ids);

    std::vector<std::string> all = new_ids;
    all.insert(all.end(), reinstall_ids.begin(), reinstall_ids.end());
    ctx_.mods.install_mods(all);
  });
}

void TabMods::on_add_all_clicked() {
  available_list_->selectAll();
  on_add_selected_clicked();
}

void TabMods::on_kenan_clicked() {
  const Modpack* pack =
      ModManager::modpack_for_game(ctx_.settings.get_string("game"));
  if (!pack) return;
  Modpack copy = *pack;

  DownloadQuestion question;
  question.url = copy.url;
  question.filename = copy.filename;
  question.title = "Get " + copy.name + "?";
  question.detail =
      "Its mods are put in the local repository, from where you can install "
      "them.";
  question.button = "Get Modpack";

  // Getting the pack replaces the local repository, which is not trashed:
  // it only holds the last download of the pack.
  if (!Paths::list_dir(ctx_.paths.mod_repo()).empty()) {
    question.title = "Replace the local repository?";
    question.detail =
        "The mods in the local repository will be deleted for good (not moved "
        "to the trash) and "
        "replaced with a fresh download of " +
        copy.name + ".\nInstalled mods are not affected.";
    question.button = "Replace Repository";
  }
  confirm_download(ctx_, runner_, parent_, *content_, question, [this, copy]() {
    run_task([this, copy]() { ctx_.mods.retrieve_modpack(copy); });
  });
}
