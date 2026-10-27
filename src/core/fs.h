#ifndef EVAC_FS_H
#define EVAC_FS_H

#include <filesystem>
#include <string>
#include <vector>

#include "core/status.h"

// UTF-8 text <-> path. Paths are kept as std::filesystem::path; use these where
// one meets text (JSON, messages, the UI, names from the user, git arguments).
// path(std::string) and path.string() use the ANSI code page on Windows with
// MSVC, which garbles names like "José".
std::filesystem::path to_path(const std::string& utf8);
std::string to_utf8(const std::filesystem::path& path);

// Filesystem helpers. All of these block until done, so call them from a worker
// thread when the UI should stay responsive. Problems are reported to Status.
// The bool functions return true only if everything worked.
namespace fs_helper
{

// Lists the files and subdirectories in a directory (hidden ones too), as paths
// relative to it, in UTF-8 with "/" separators. Optionally recursive, and
// optionally filtered by a regular expression that is matched against the
// whole relative path.
std::vector<std::string> list_dir(const std::filesystem::path& path, bool recursive, const std::string& filter_regex,
                                  Status& status);

// Recursively copies a directory *into* dest_dir, i.e. to dest_dir/<name of the directory>.
bool copy_dir(const std::filesystem::path& path, const std::filesystem::path& dest_dir, Status& status);

// Recursively removes a directory.
bool rm_dir(const std::filesystem::path& path, Status& status);

// Moves a directory. The last part of dest is the new name of the directory.
// If dest already exists, the contents are merged into it.
bool move_dir(const std::filesystem::path& path, const std::filesystem::path& dest, Status& status);

}  // namespace fs_helper

#endif
