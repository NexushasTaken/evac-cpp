#ifndef EVAC_TAB_SETTINGS_H
#define EVAC_TAB_SETTINGS_H

#include <QScrollArea>
#include <string>
#include <utility>
#include <vector>

#include "ui/app_context.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QVBoxLayout;
class LicensesDialog;

// The Settings tab. Every change is applied and saved right away.
class TabSettings : public QScrollArea {
 public:
  TabSettings(AppContext& ctx, QWidget& parent);

  // Shows the current values of the settings.
  void load_from_settings();

 private:
  void build_ui();
  QCheckBox* add_check(QVBoxLayout& box, const char* caption,
                       const char* tooltip, const char* key);
  void add_row(QVBoxLayout& box, const char* caption, const char* tooltip,
               QWidget* widget, int stretch = 0);

  AppContext& m_ctx;
  QWidget& m_parent;
  LicensesDialog*
      m_licenses_dialog;  // Made when first needed, owned by m_parent.
  bool m_updating;
  std::vector<std::pair<std::string, QCheckBox*> > m_checks;

  QLineEdit* m_game_extra_args;
  QSpinBox* m_num_releases;
  QComboBox* m_proxy_option;
  QLineEdit* m_proxy_host;
  QSpinBox* m_proxy_port;
  QSpinBox* m_trash_max_size;
};

#endif
