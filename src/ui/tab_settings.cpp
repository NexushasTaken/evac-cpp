#include "ui/tab_settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ui/licenses_dialog.h"
#include "ui/qt_helpers.h"

// The values of proxy_option, in the order of the drop-down.
static const char* const PROXY_OPTIONS[] = {"off", "on", "download"};
static const int NUM_PROXY_OPTIONS = 3;

static QLabel* make_section_heading(const QString& text) {
  QLabel* label = make_heading(text);
  label->setContentsMargins(0, 8, 0, 0);
  return label;
}

TabSettings::TabSettings(AppContext& ctx, QWidget& parent)
    : m_ctx(ctx),
      m_parent(parent),
      m_licenses_dialog(nullptr),
      m_updating(false) {
  build_ui();
  load_from_settings();
}

QCheckBox* TabSettings::add_check(QVBoxLayout& box, const char* caption,
                                  const char* tooltip, const char* key) {
  QCheckBox* check = new QCheckBox(caption);
  check->setToolTip(tooltip);
  std::string setting = key;
  connect(check, &QCheckBox::toggled, this, [this, setting](bool active) {
    if (m_updating) return;
    m_ctx.settings.set_bool(setting, active);
    m_ctx.settings.save();
  });
  box.addWidget(check);
  m_checks.push_back(std::make_pair(setting, check));
  return check;
}

// stretch > 0: the widget shares the width with the label.
void TabSettings::add_row(QVBoxLayout& box, const char* caption,
                          const char* tooltip, QWidget* widget, int stretch) {
  QHBoxLayout* row = new QHBoxLayout();
  row->setSpacing(8);
  QLabel* label = make_label(caption);
  label->setToolTip(tooltip);
  row->addWidget(label, 1);
  widget->setToolTip(tooltip);
  row->addWidget(widget, stretch);
  box.addLayout(row);
}

void TabSettings::build_ui() {
  setWidgetResizable(true);
  setFrameShape(QFrame::NoFrame);
  QWidget* page = new QWidget();
  QVBoxLayout* box = new QVBoxLayout(page);
  box->setContentsMargins(12, 12, 12, 12);
  box->setSpacing(6);
  setWidget(page);

  box->addWidget(make_section_heading("Game"));
  add_check(*box, "Show game description",
            "Show a description for the chosen game under the Game list.",
            "show_game_desc");
  add_check(
      *box, "Keep launcher open after starting the game",
      "If turned off, the launcher will close after clicking Play or Resume.\n"
      "Note that if the game fails to start for any reason, the launcher\nwill "
      "still close.",
      "keep_open_after_starting_game");

  m_game_extra_args = new QLineEdit();
  m_game_extra_args->setPlaceholderText("none");
  connect(m_game_extra_args, &QLineEdit::textChanged, this,
          [this](const QString& text) {
            if (m_updating) return;
            m_ctx.settings.set_string("game_extra_args", str(text));
            m_ctx.settings.save();
          });
  add_row(
      *box, "Extra command-line parameters:",
      "Added to the game's command line when starting it.\n"
      "Separate them with spaces; use quotes for a value with spaces in it.\n"
      "The launcher already passes --userdir, and --world for Resume.",
      m_game_extra_args, 1);
  add_check(*box, "Allow \"updating\" to the same build as installed",
            "Allow starting the update procedure even when the "
            "selected\nrelease is the same as currently installed.",
            "update_to_same_build_allowed");
  add_check(*box, "Shorten names of experimental releases",
            "Shorten release names to just the date and number\n(e.g. "
            "\"2021-09-08-1927\" instead of\n"
            "\"Cataclysm-DDA experimental build 2021-09-08-1927\").",
            "shorten_release_names");
  add_check(*box, "Print tips of the day on startup",
            "Print a random tip in the status pane when the launcher starts.",
            "print_tips_of_the_day");
  add_check(*box, "Always show installs list",
            "Show the installs list even when just one\ncopy of the game is "
            "installed, or none at all.",
            "always_show_installs");

  m_num_releases = new QSpinBox();
  m_num_releases->setRange(1, 100);
  connect(m_num_releases, &QSpinBox::valueChanged, this, [this](int value) {
    if (m_updating) return;
    m_ctx.settings.set_int("num_releases_to_request", value);
    m_ctx.settings.save();
  });
  add_row(*box, "Number of releases to request:",
          "How many latest releases will be requested\nfrom GitHub after "
          "starting the launcher,\nchoosing the game or clicking Refresh.",
          m_num_releases);

  box->addWidget(make_section_heading("Mods"));
  add_check(
      *box, "List obsolete mods in installed",
      "Show mods that come with the game but are marked as "
      "\"obsolete\".\nNormally, such mods are not considered as installed.",
      "show_obsolete_mods");
  add_check(
      *box, "Include archived mods when installing Kenan Modpack",
      "When installing Kenan Modpack, also include *archived* mods.\nThese "
      "mods are no longer maintained by Kenan and may be outdated.",
      "install_archived_mods");

  box->addWidget(make_section_heading("Downloads"));
  add_check(
      *box, "Keep download cache",
      "Keep downloaded contents (game releases, modpacks, soundpacks) in cache "
      "folder\nwhen install completes, instead of having it deleted.",
      "keep_cache");
  add_check(*box, "Ignore download cache",
            "Always download contents from the web,\neven if it's already "
            "inside cache folder.",
            "ignore_cache");

  m_proxy_option = new QComboBox();
  m_proxy_option->addItem("Disabled");
  m_proxy_option->addItem("Enabled");
  m_proxy_option->addItem("For Downloading Only");
  connect(m_proxy_option, &QComboBox::currentIndexChanged, this,
          [this](int index) {
            if (m_updating || index < 0 || index >= NUM_PROXY_OPTIONS) return;
            m_ctx.settings.set_string("proxy_option", PROXY_OPTIONS[index]);
            m_ctx.settings.save();
          });
  add_row(*box, "Proxy server:",
          "Disabled - Do not use proxy\nEnabled - Use proxy on all occasions "
          "(e.g. requesting releases)\n"
          "For Downloading Only - Use proxy only when downloading",
          m_proxy_option);

  m_proxy_host = new QLineEdit();
  m_proxy_host->setPlaceholderText("host");
  connect(m_proxy_host, &QLineEdit::textChanged, this,
          [this](const QString& text) {
            if (m_updating) return;
            m_ctx.settings.set_string("proxy_host", str(text));
            m_ctx.settings.save();
          });
  add_row(*box, "Proxy host:", "Configure a proxy server for downloading.",
          m_proxy_host);

  m_proxy_port = new QSpinBox();
  m_proxy_port->setRange(0, 65535);
  connect(m_proxy_port, &QSpinBox::valueChanged, this, [this](int value) {
    if (m_updating) return;
    m_ctx.settings.set_int("proxy_port", value);
    m_ctx.settings.save();
  });
  add_row(*box, "Proxy port:", "Configure a proxy server for downloading.",
          m_proxy_port);

  box->addWidget(make_section_heading("Trash"));
  add_check(*box, "Move deleted things to the trash",
            "Deleted game installs, mods, soundpacks, backups and replaced "
            "saves\ngo to the Trash tab, where they can be restored.\n"
            "When turned off, deleting is permanent.",
            "use_trash");
  m_trash_max_size = new QSpinBox();
  m_trash_max_size->setRange(0, 1048576);
  m_trash_max_size->setSingleStep(256);
  connect(m_trash_max_size, &QSpinBox::valueChanged, this, [this](int value) {
    if (m_updating) return;
    m_ctx.settings.set_int("trash_max_size_mb", value);
    m_ctx.settings.save();
  });
  add_row(*box, "Trash size limit (MiB):",
          "When the trash gets bigger than this, the launcher tells you,\nand "
          "Free Up Space on the Trash tab deletes the oldest items.\n"
          "Nothing is deleted without asking.",
          m_trash_max_size);
  add_check(*box, "Offer to delete the oldest items when over the limit",
            "When the trash gets over its size limit, ask right away whether "
            "to\ndelete the oldest items for good, "
            "instead of only showing a message.",
            "trash_auto_prune");

  box->addWidget(make_section_heading("Other"));
  add_check(*box, "Debug mode",
            "Enables debug messages in the status pane\n(useful for "
            "troubleshooting).",
            "debug_mode");

  QLabel* licenses_link =
      new QLabel("<a href=\"#\">View opensource licenses</a>");
  licenses_link->setTextFormat(Qt::RichText);
  connect(licenses_link, &QLabel::linkActivated, this, [this]() {
    if (!m_licenses_dialog)
      m_licenses_dialog = new LicensesDialog(m_parent, m_ctx.paths.data_dir());
    m_licenses_dialog->show();
    m_licenses_dialog->raise();
    m_licenses_dialog->activateWindow();
  });
  box->addWidget(licenses_link);

  QLabel* note =
      make_label("The launcher uses your system's theme and display scale.");
  note->setContentsMargins(0, 12, 0, 0);
  note->setEnabled(false);
  box->addWidget(note);
  box->addStretch(1);
}

void TabSettings::load_from_settings() {
  m_updating = true;

  for (size_t i = 0; i < m_checks.size(); ++i)
    m_checks[i].second->setChecked(m_ctx.settings.get_bool(m_checks[i].first));

  m_game_extra_args->setText(
      qstr(m_ctx.settings.get_string("game_extra_args")));
  m_num_releases->setValue(m_ctx.settings.get_int("num_releases_to_request"));

  std::string option = m_ctx.settings.get_string("proxy_option");
  int selected = 0;
  for (int i = 0; i < NUM_PROXY_OPTIONS; ++i) {
    if (option == PROXY_OPTIONS[i]) selected = i;
  }
  m_proxy_option->setCurrentIndex(selected);

  m_proxy_host->setText(qstr(m_ctx.settings.get_string("proxy_host")));
  m_proxy_port->setValue(m_ctx.settings.get_int("proxy_port"));
  m_trash_max_size->setValue(m_ctx.settings.get_int("trash_max_size_mb"));

  m_updating = false;
}
