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
    : ctx_(ctx), parent_(parent), licenses_dialog_(nullptr), updating_(false) {
  build_ui();
  load_from_settings();
}

QCheckBox* TabSettings::add_check(QVBoxLayout& box, const char* caption,
                                  const char* tooltip, const char* key) {
  QCheckBox* check = new QCheckBox(caption);
  check->setToolTip(tooltip);
  std::string setting = key;
  connect(check, &QCheckBox::toggled, this, [this, setting](bool active) {
    if (updating_) return;
    ctx_.settings.set_bool(setting, active);
    ctx_.settings.save();
  });
  box.addWidget(check);
  checks_.push_back(std::make_pair(setting, check));
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

  game_extra_args_ = new QLineEdit();
  game_extra_args_->setPlaceholderText("none");
  connect(game_extra_args_, &QLineEdit::textChanged, this,
          [this](const QString& text) {
            if (updating_) return;
            ctx_.settings.set_string("game_extra_args", str(text));
            ctx_.settings.save();
          });
  add_row(
      *box, "Extra command-line parameters:",
      "Added to the game's command line when starting it.\n"
      "Separate them with spaces; use quotes for a value with spaces in it.\n"
      "The launcher already passes --userdir, and --world for Resume.",
      game_extra_args_, 1);
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

  num_releases_ = new QSpinBox();
  num_releases_->setRange(1, 100);
  connect(num_releases_, &QSpinBox::valueChanged, this, [this](int value) {
    if (updating_) return;
    ctx_.settings.set_int("num_releases_to_request", value);
    ctx_.settings.save();
  });
  add_row(*box, "Number of releases to request:",
          "How many latest releases will be requested\nfrom GitHub after "
          "starting the launcher,\nchoosing the game or clicking Refresh.",
          num_releases_);

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

  proxy_option_ = new QComboBox();
  proxy_option_->addItem("Disabled");
  proxy_option_->addItem("Enabled");
  proxy_option_->addItem("For Downloading Only");
  connect(proxy_option_, &QComboBox::currentIndexChanged, this,
          [this](int index) {
            if (updating_ || index < 0 || index >= NUM_PROXY_OPTIONS) return;
            ctx_.settings.set_string("proxy_option", PROXY_OPTIONS[index]);
            ctx_.settings.save();
          });
  add_row(*box, "Proxy server:",
          "Disabled - Do not use proxy\nEnabled - Use proxy on all occasions "
          "(e.g. requesting releases)\n"
          "For Downloading Only - Use proxy only when downloading",
          proxy_option_);

  proxy_host_ = new QLineEdit();
  proxy_host_->setPlaceholderText("host");
  connect(proxy_host_, &QLineEdit::textChanged, this,
          [this](const QString& text) {
            if (updating_) return;
            ctx_.settings.set_string("proxy_host", str(text));
            ctx_.settings.save();
          });
  add_row(*box, "Proxy host:", "Configure a proxy server for downloading.",
          proxy_host_);

  proxy_port_ = new QSpinBox();
  proxy_port_->setRange(0, 65535);
  connect(proxy_port_, &QSpinBox::valueChanged, this, [this](int value) {
    if (updating_) return;
    ctx_.settings.set_int("proxy_port", value);
    ctx_.settings.save();
  });
  add_row(*box, "Proxy port:", "Configure a proxy server for downloading.",
          proxy_port_);

  box->addWidget(make_section_heading("Trash"));
  add_check(*box, "Move deleted things to the trash",
            "Deleted game installs, mods, soundpacks, backups and replaced "
            "saves\ngo to the Trash tab, where they can be restored.\n"
            "When turned off, deleting is permanent.",
            "use_trash");
  trash_max_size_ = new QSpinBox();
  trash_max_size_->setRange(0, 1048576);
  trash_max_size_->setSingleStep(256);
  connect(trash_max_size_, &QSpinBox::valueChanged, this, [this](int value) {
    if (updating_) return;
    ctx_.settings.set_int("trash_max_size_mb", value);
    ctx_.settings.save();
  });
  add_row(*box, "Trash size limit (MiB):",
          "When the trash gets bigger than this, the launcher tells you,\nand "
          "Free Up Space on the Trash tab deletes the oldest items.\n"
          "Nothing is deleted without asking.",
          trash_max_size_);
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
    if (!licenses_dialog_)
      licenses_dialog_ = new LicensesDialog(parent_, ctx_.paths.data_dir());
    licenses_dialog_->show();
    licenses_dialog_->raise();
    licenses_dialog_->activateWindow();
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
  updating_ = true;

  for (size_t i = 0; i < checks_.size(); ++i)
    checks_[i].second->setChecked(ctx_.settings.get_bool(checks_[i].first));

  game_extra_args_->setText(qstr(ctx_.settings.get_string("game_extra_args")));
  num_releases_->setValue(ctx_.settings.get_int("num_releases_to_request"));

  std::string option = ctx_.settings.get_string("proxy_option");
  int selected = 0;
  for (int i = 0; i < NUM_PROXY_OPTIONS; ++i) {
    if (option == PROXY_OPTIONS[i]) selected = i;
  }
  proxy_option_->setCurrentIndex(selected);

  proxy_host_->setText(qstr(ctx_.settings.get_string("proxy_host")));
  proxy_port_->setValue(ctx_.settings.get_int("proxy_port"));
  trash_max_size_->setValue(ctx_.settings.get_int("trash_max_size_mb"));

  updating_ = false;
}
