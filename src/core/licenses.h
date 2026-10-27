#ifndef EVAC_LICENSES_H
#define EVAC_LICENSES_H

#include <string>
#include <vector>

struct LicenseSection
{
    std::string heading;
    std::string text;
};

// The license texts shown in the launcher: Catapult itself, the libraries it
// uses and the bundled fonts. data_dir is where the launcher's data files are.
std::vector<LicenseSection> load_license_sections(const std::string& data_dir);

#endif
