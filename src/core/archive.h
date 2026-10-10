#ifndef CATAPULT_ARCHIVE_H
#define CATAPULT_ARCHIVE_H

#include <string>

#include "core/status.h"

// Archive helpers on top of libarchive, so no external zip/unzip/tar is needed.
// Both block until done; call them from a worker thread.
namespace archive_helper
{

// Extracts a .zip or .tar.gz (the format is detected from the content) into
// dest_dir, which is created if needed. Existing files are overwritten.
bool extract(const std::string& archive_path, const std::string& dest_dir, Status& status);

// Creates a .zip with the directory dir_to_zip (relative to parent) in it.
// Entries are stored as "<dir_to_zip>/...". Symlinks are skipped.
bool zip(const std::string& parent, const std::string& dir_to_zip, const std::string& dest_zip, Status& status);

}  // namespace archive_helper

#endif
