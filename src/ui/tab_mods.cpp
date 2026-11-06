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

// A list with the name, and the reason it is grayed (if it is) on the right.
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
    : m_ctx(ctx), m_runner(runner), m_parent(parent), m_updating(false) {
  build_ui();
}

void TabMods::build_ui() {
  QVBoxLayout* outer = new QVBoxLayout(this);
  outer->setContentsMargins(12, 12, 12, 12);
  m_hint_label = make_label("");
  m_hint_label->setWordWrap(true);
  m_hint_label->setAlignment(
      Qt::AlignLeft | Qt::AlignTop);  // Not in the middle of the empty tab.
  outer->addWidget(m_hint_label);
  m_content = new QWidget();
  outer->addWidget(m_content, 1);

  QVBoxLayout* content = new QVBoxLayout(m_content);
  content->setContentsMargins(0, 0, 0, 0);
  content->setSpacing(8);
  QHBoxLayout* columns = new QHBoxLayout();
  columns->setSpacing(12);
  content->addLayout(columns, 1);

  // Both columns have the same structure: header, list, footer.

  // Installed mods.
  QVBoxLayout* left = new QVBoxLayout();
  left->setSpacing(6);
  m_installed_caption = make_heading("");
  QHBoxLayout* left_header = new QHBoxLayout();
  left_header->addWidget(m_installed_caption, 1);
  left->addLayout(left_header);

  m_installed_list = make_mod_list();
  left->addWidget(m_installed_list, 1);

  m_show_stock_check = new QCheckBox("Show stock");
  m_show_stock_check->setToolTip("Show mods from data/mods on this list.");
  m_delete_button = new QPushButton("Delete Selected");
  m_delete_button->setToolTip(
      "Remove selected mods from your game installation.");
  QHBoxLayout* left_footer = new QHBoxLayout();
  left_footer->addWidget(m_show_stock_check, 1);
  left_footer->addWidget(m_delete_button);
  left->addLayout(left_footer);
  columns->addLayout(left, 1);

  // The local repository.
  QVBoxLayout* right = new QVBoxLayout();
  right->setSpacing(6);
  m_available_caption = make_heading("");
  m_kenan_button = new QPushButton("Get Kenan Modpack");
  m_kenan_button->setToolTip(
      "Download Kenan Modpack and add its mods\nto the local repository.");
  QHBoxLayout* right_header = new QHBoxLayout();
  right_header->addWidget(m_available_caption, 1);
  right_header->addWidget(m_kenan_button);
  right->addLayout(right_header);

  m_available_list = make_mod_list();
  right->addWidget(m_available_list, 1);

  m_show_installed_check = new QCheckBox("Show already installed");
  m_show_installed_check->setToolTip(
      "Show already installed mods on this list\nin case you want to "
      "re-install them.");
  m_add_all_button = new QPushButton("Add All");
  m_add_all_button->setToolTip(
      "Add all listed mods to your game installation.");
  m_add_selected_button = new QPushButton("Add Selected");
  m_add_selected_button->setToolTip(
      "Add selected mods to your game installation.");
  QHBoxLayout* right_footer = new QHBoxLayout();
  right_footer->addWidget(m_show_installed_check, 1);
  right_footer->addWidget(m_add_all_button);
  right_footer->addWidget(m_add_selected_button);
  right->addLayout(right_footer);
  columns->addLayout(right, 1);

  // Details of the selected mod.
  m_info_view = new QTextBrowser();
  m_info_view->setOpenExternalLinks(true);
  m_info_view->setFixedHeight(110);
  content->addWidget(m_info_view);

  connect(m_installed_list, &QTreeWidget::itemSelectionChanged, this,
          &TabMods::on_installed_selection_changed);
  connect(m_available_list, &QTreeWidget::itemSelectionChanged, this,
          &TabMods::on_available_selection_changed);
  connect(m_show_stock_check, &QCheckBox::toggled, this,
          &TabMods::on_show_stock_toggled);
  connect(m_show_installed_check, &QCheckBox::toggled, this,
          &TabMods::on_show_installed_toggled);
  connect(m_delete_button, &QPushButton::clicked, this,
          &TabMods::on_delete_clicked);
  connect(m_add_selected_button, &QPushButton::clicked, this,
          &TabMods::on_add_selected_clicked);
  connect(m_add_all_button, &QPushButton::clicked, this,
          &TabMods::on_add_all_clicked);
  connect(m_kenan_button, &QPushButton::clicked, this,
          &TabMods::on_kenan_clicked);
}

void TabMods::show_unavailable(const QString& message) {
  m_hint_label->setText(message);
  m_hint_label->setVisible(true);
  m_content->setVisible(false);
}

void TabMods::refresh() {
  std::string game = m_ctx.settings.get_string("game");
  Paths::InstallsSummary installs = m_ctx.paths.installs_summary();
  if (installs.find(game) == installs.end()) {
    show_unavailable("Install the game first (on the Game tab).");
    return;
  }
  m_hint_label->setVisible(false);
  m_content->setVisible(true);

  m_updating = true;
  m_show_stock_check->setChecked(m_ctx.settings.get_bool("show_stock_mods"));
  m_show_installed_check->setChecked(
      m_ctx.settings.get_bool("show_installed_mods_in_available"));
  m_updating = false;

  bool have_pack = (ModManager::modpack_for_game(game) != nullptr);
  m_kenan_button->setEnabled(have_pack);
  if (!have_pack)
    m_kenan_button->setToolTip("There is no modpack for this game.");

  m_info_view->setPlainText(NO_INFO_TEXT);
  m_ctx.mods.refresh_installed();
  m_ctx.mods.refresh_available();
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
  m_updating = true;
  list->clear();
  QBrush dim = dim_brush(list);
  for (size_t i = 0; i < rows.size(); ++i) {
    QTreeWidgetItem* item = new QTreeWidgetItem(list);
    item->setText(0, qstr(rows[i].name));
    item->setText(1, qstr(rows[i].badge));
    item->setForeground(1, dim);
    if (rows[i].grayed) item->setForeground(0, dim);
    item->setToolTip(0, qstr("Location: " + rows[i].location));
    item->setToolTip(1, item->toolTip(0));
  }
  if (rows.empty()) {
    QTreeWidgetItem* item = new QTreeWidgetItem(list);
    item->setText(0, placeholder);
    item->setForeground(0, dim);
    item->setFlags(Qt::NoItemFlags);
  }
  m_updating = false;
}

// Which mods are listed on the left depends on the settings. See ModStatus.
void TabMods::reload_installed() {
  bool show_stock = m_ctx.settings.get_bool("show_stock_mods");
  bool show_obsolete = m_ctx.settings.get_bool("show_obsolete_mods");
  int hidden = 0;

  m_installed_rows.clear();
  const std::map<std::string, ModEntry>& installed = m_ctx.mods.installed();
  for (std::map<std::string, ModEntry>::const_iterator it = installed.begin();
       it != installed.end(); ++it) {
    ModStatus status = m_ctx.mods.mod_status(it->first);
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
    row.grayed = it->second.is_stock;
    if (row.grayed) row.badge = "stock";
    m_installed_rows.push_back(row);
  }
  std::sort(m_installed_rows.begin(), m_installed_rows.end(),
            [](const ModRow& a, const ModRow& b) {
              return name_less(a.name, b.name);
            });

  std::string caption = "Installed";
  if (hidden > 0) caption += " (" + std::to_string(hidden) + " hidden)";
  m_installed_caption->setText(qstr(caption + ":"));

  fill_list(m_installed_list, m_installed_rows, "No mods installed.");
  m_previous_installed_selection.clear();
  m_delete_button->setEnabled(false);
}

void TabMods::reload_available() {
  bool include_installed =
      m_ctx.settings.get_bool("show_installed_mods_in_available");
  int hidden = 0;

  m_available_rows.clear();
  const std::map<std::string, ModEntry>& available = m_ctx.mods.available();
  for (std::map<std::string, ModEntry>::const_iterator it = available.begin();
       it != available.end(); ++it) {
    ModStatus status = m_ctx.mods.mod_status(it->first);
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
    row.grayed = !installable;
    if (row.grayed) row.badge = "installed";
    m_available_rows.push_back(row);
  }
  std::sort(m_available_rows.begin(), m_available_rows.end(),
            [](const ModRow& a, const ModRow& b) {
              return name_less(a.name, b.name);
            });

  std::string caption = "Local repository";
  if (hidden > 0) caption += " (" + std::to_string(hidden) + " hidden)";
  m_available_caption->setText(qstr(caption + ":"));

  fill_list(m_available_list, m_available_rows,
            "The local repository is empty. Use Get Kenan Modpack to fill it.");
  m_previous_available_selection.clear();
  m_add_selected_button->setEnabled(false);
  m_add_all_button->setEnabled(!m_available_rows.empty());
}

void TabMods::show_mod_info(const std::string& id, bool from_available) {
  const std::map<std::string, ModEntry>& mods =
      from_available ? m_ctx.mods.available() : m_ctx.mods.installed();
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
  m_info_view->setHtml(text);
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
  if (m_updating) return;
  std::set<int> now = selected_rows(m_installed_list);
  int active = pick_active(m_previous_installed_selection, now);
  m_previous_installed_selection = now;

  if (active >= 0 && (size_t)active < m_installed_rows.size())
    show_mod_info(m_installed_rows[active].id, false);

  // Stock mods can't be deleted, so there must be something else selected.
  bool can_delete = false;
  for (std::set<int>::const_iterator it = now.begin(); it != now.end(); ++it) {
    if (*it >= 0 && (size_t)*it < m_installed_rows.size() &&
        !m_installed_rows[*it].grayed)
      can_delete = true;
  }
  m_delete_button->setEnabled(can_delete);
}

void TabMods::on_available_selection_changed() {
  if (m_updating) return;
  std::set<int> now = selected_rows(m_available_list);
  int active = pick_active(m_previous_available_selection, now);
  m_previous_available_selection = now;

  if (active >= 0 && (size_t)active < m_available_rows.size())
    show_mod_info(m_available_rows[active].id, true);

  // As in the original launcher: adding is only possible if the selection has a
  // mod that is not installed (or is installed as an obsolete stock mod).
  bool can_add = false;
  const std::map<std::string, ModEntry>& installed = m_ctx.mods.installed();
  for (std::set<int>::const_iterator it = now.begin(); it != now.end(); ++it) {
    if (*it < 0 || (size_t)*it >= m_available_rows.size()) continue;
    std::map<std::string, ModEntry>::const_iterator inst =
        installed.find(m_available_rows[*it].id);
    if (inst == installed.end() || inst->second.is_obsolete) can_add = true;
  }
  m_add_selected_button->setEnabled(can_add);
}

void TabMods::on_show_stock_toggled() {
  if (m_updating) return;
  m_ctx.settings.set_bool("show_stock_mods", m_show_stock_check->isChecked());
  m_ctx.settings.save();
  reload_installed();
}

void TabMods::on_show_installed_toggled() {
  if (m_updating) return;
  m_ctx.settings.set_bool("show_installed_mods_in_available",
                          m_show_installed_check->isChecked());
  m_ctx.settings.save();
  reload_available();
}

// Runs a slow job on the worker thread, with the whole tab disabled until it is
// done.
void TabMods::run_task(const std::function<void()>& work) {
  m_content->setEnabled(false);
  bool started = m_runner.run(work, [this]() {
    m_content->setEnabled(true);
    reload_installed();
    reload_available();
  });
  if (!started) m_content->setEnabled(true);
}

void TabMods::on_delete_clicked() {
  std::vector<std::string> ids;
  int skipped = 0;
  std::set<int> selection = selected_rows(m_installed_list);
  for (std::set<int>::const_iterator it = selection.begin();
       it != selection.end(); ++it) {
    if (*it < 0 || (size_t)*it >= m_installed_rows.size()) continue;
    if (m_installed_rows[*it].grayed)
      ++skipped;
    else
      ids.push_back(m_installed_rows[*it].id);
  }

  if (skipped == 1)
    m_ctx.status.post(
        "One of the selected mods is stock and cannot be deleted.");
  else if (skipped > 1)
    m_ctx.status.post(std::to_string(skipped) +
                      " of the selected mods are stock and cannot be deleted.");
  if (ids.empty()) return;

  bool use_trash = m_ctx.settings.get_bool("use_trash");
  std::string detail;
  if (ids.size() == 1) {
    std::map<std::string, ModEntry>::const_iterator mod =
        m_ctx.mods.installed().find(ids[0]);
    std::string name =
        (mod != m_ctx.mods.installed().end()) ? mod->second.name : ids[0];
    detail = "\"" + name + "\" " + dialogs::removal_text(use_trash) + ".";
  } else {
    detail = std::to_string(ids.size()) + " mods " +
             dialogs::removal_text(use_trash) + ".";
  }
  dialogs::confirm(
      m_parent, ids.size() == 1 ? "Delete the mod?" : "Delete the mods?",
      qstr(detail), qstr(dialogs::removal_button(use_trash)), [this, ids]() {
        run_task([this, ids]() { m_ctx.mods.delete_mods(ids); });
      });
}

void TabMods::on_add_selected_clicked() {
  std::vector<std::string> new_ids;        // Install from scratch.
  std::vector<std::string> reinstall_ids;  // Delete the installed copy first.
  int stock = 0;

  std::set<int> selection = selected_rows(m_available_list);
  for (std::set<int>::const_iterator it = selection.begin();
       it != selection.end(); ++it) {
    if (*it < 0 || (size_t)*it >= m_available_rows.size()) continue;
    const std::string& id = m_available_rows[*it].id;
    ModStatus status = m_ctx.mods.mod_status(id);
    if (status == MOD_STOCK)
      ++stock;
    else if (status == MOD_INSTALLED || status == MOD_INSTALLED_MODIFIED_ID)
      reinstall_ids.push_back(id);
    else
      new_ids.push_back(id);
  }

  if (stock == 1)
    m_ctx.status.post(
        "One mod already comes with the game, so it will not be installed.");
  else if (stock > 1)
    m_ctx.status.post(
        std::to_string(stock) +
        " mods already come with the game, so they will not be installed.");

  if (reinstall_ids.empty()) {
    install_mods(new_ids, reinstall_ids);
    return;
  }

  std::string old_copies = m_ctx.settings.get_bool("use_trash")
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
  dialogs::ask(m_parent, "Reinstall mods?", qstr(detail), buttons,
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
    if (!reinstall_ids.empty()) m_ctx.mods.delete_mods(reinstall_ids);

    std::vector<std::string> all = new_ids;
    all.insert(all.end(), reinstall_ids.begin(), reinstall_ids.end());
    m_ctx.mods.install_mods(all);
  });
}

void TabMods::on_add_all_clicked() {
  m_available_list->selectAll();
  on_add_selected_clicked();
}

void TabMods::on_kenan_clicked() {
  const Modpack* pack =
      ModManager::modpack_for_game(m_ctx.settings.get_string("game"));
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
  if (!Paths::list_dir(m_ctx.paths.mod_repo()).empty()) {
    question.title = "Replace the local repository?";
    question.detail =
        "The mods in the local repository will be deleted for good (not moved "
        "to the trash) and "
        "replaced with a fresh download of " +
        copy.name + ".\nInstalled mods are not affected.";
    question.button = "Replace Repository";
  }
  confirm_download(
      m_ctx, m_runner, m_parent, *m_content, question, [this, copy]() {
        run_task([this, copy]() { m_ctx.mods.retrieve_modpack(copy); });
      });
}
