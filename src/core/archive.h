#ifndef EVAC_ARCHIVE_H
#define EVAC_ARCHIVE_H

#include <filesystem>
#include <string>

#include "core/status.h"

// Archive helpers on top of libarchive, so no external zip/unzip/tar is needed.
// Both block until done; call them from a worker thread.
namespace archive_helper
{

// Extracts a .zip or .tar.gz (the format is detected from the content) into
// dest_dir, which is created if needed. Existing files are overwritten.
bool extract(const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir, Status& status);

// Creates a .zip with the directory dir_to_zip (a name in parent, UTF-8) in it.
// Entries are stored as "<dir_to_zip>/...". Symlinks are skipped.
bool zip(const std::filesystem::path& parent, const std::string& dir_to_zip, const std::filesystem::path& dest_zip,
         Status& status);

}  // namespace archive_helper

#endif
