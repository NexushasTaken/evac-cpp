#ifndef EVAC_TAB_SOUNDPACKS_H
#define EVAC_TAB_SOUNDPACKS_H

#include <QWidget>

#include <functional>
#include <string>
#include <vector>

#include "core/soundpack_manager.h"
#include "ui/app_context.h"
#include "ui/task_runner.h"

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;

// The Soundpacks tab: installed packs on the left, downloadable ones on the right.
class TabSoundpacks : public QWidget
{
public:
    TabSoundpacks(AppContext& ctx, TaskRunner& runner, QWidget& parent);

    // Reloads the lists from disk. Call when the tab is shown.
    void refresh();

private:
    void build_ui();
    void show_unavailable(const QString& message);
    void refresh_installed();
    void update_buttons();
    void run_task(const std::function<void()>& work);

    int selected_installed();
    int selected_available();

    void on_show_stock_toggled();
    void on_installed_selected();
    void on_available_selected();
    void on_toggle_clicked();
    void on_delete_clicked();
    void on_install_clicked();
    void on_install_from_file_clicked();
    void install_selected(const std::string& from_file);

    AppContext& ctx_;
    TaskRunner& runner_;
    QWidget& parent_;
    bool updating_;
    std::vector<InstalledSoundpack> installed_;

    QLabel* hint_label_;
    QWidget* content_;
    QListWidget* installed_list_;
    QListWidget* available_list_;
    QCheckBox* show_stock_check_;
    QPushButton* toggle_button_;
    QPushButton* delete_button_;
    QPushButton* install_button_;
    QPushButton* install_from_file_button_;
};

#endif
