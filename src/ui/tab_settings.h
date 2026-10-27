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
class TabSettings : public QScrollArea
{
public:
    TabSettings(AppContext& ctx, QWidget& parent);

    // Shows the current values of the settings.
    void load_from_settings();

private:
    void build_ui();
    QCheckBox* add_check(QVBoxLayout& box, const char* caption, const char* tooltip, const char* key);
    void add_row(QVBoxLayout& box, const char* caption, const char* tooltip, QWidget* widget, int stretch = 0);

    AppContext& ctx_;
    QWidget& parent_;
    LicensesDialog* licenses_dialog_;  // Made when first needed, owned by parent_.
    bool updating_;
    std::vector<std::pair<std::string, QCheckBox*> > checks_;

    QLineEdit* game_extra_args_;
    QSpinBox* num_releases_;
    QComboBox* proxy_option_;
    QLineEdit* proxy_host_;
    QSpinBox* proxy_port_;
    QSpinBox* trash_max_size_;
};

#endif
