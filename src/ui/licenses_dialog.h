#ifndef EVAC_LICENSES_DIALOG_H
#define EVAC_LICENSES_DIALOG_H

#include <QDialog>

#include <filesystem>

// A window with the open source licenses. Closing it only hides it, so the
// same window can be shown again.
class LicensesDialog : public QDialog
{
public:
    LicensesDialog(QWidget& parent, const std::filesystem::path& data_dir);
};

#endif
