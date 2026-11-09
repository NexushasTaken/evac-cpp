#include "core/archive.h"

#include <archive.h>
#include <archive_entry.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <array>
#include <climits>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <vector>

#include "core/fs.h"
#include "core/status.h"

namespace fs = std::filesystem;

namespace archive_helper {

static std::string file_name_of(const fs::path& path) {
  return to_utf8(path.filename());
}

// libarchive's char* names are in the ANSI code page on Windows, so use its
// wchar_t versions there. Elsewhere names are bytes, passed on as they are.
#ifdef _WIN32
static int open_for_reading(struct archive* a, const fs::path& file) {
  return archive_read_open_filename_w(a, file.c_str(), 65536);
}
static int open_for_writing(struct archive* a, const fs::path& file) {
  return archive_write_open_filename_w(a, file.c_str());
}
static fs::path entry_path(struct archive_entry* e) {
  return archive_entry_pathname_w(e);
}
static const wchar_t* entry_hardlink(struct archive_entry* e) {
  return archive_entry_hardlink_w(e);
}
static void set_entry_path(struct archive_entry* e, const fs::path& path) {
  archive_entry_copy_pathname_w(e, path.c_str());
}
static void set_entry_hardlink(struct archive_entry* e, const fs::path& path) {
  archive_entry_copy_hardlink_w(e, path.c_str());
}
#else
static int open_for_reading(struct archive* a, const fs::path& file) {
  return archive_read_open_filename(a, file.c_str(), USHRT_MAX);
}
static int open_for_writing(struct archive* a, const fs::path& file) {
  return archive_write_open_filename(a, file.c_str());
}
static fs::path entry_path(struct archive_entry* e) {
  return archive_entry_pathname(e);
}
static const char* entry_hardlink(struct archive_entry* e) {
  return archive_entry_hardlink(e);
}
static void set_entry_path(struct archive_entry* e, const fs::path& path) {
  archive_entry_copy_pathname(e, path.c_str());
}
static void set_entry_hardlink(struct archive_entry* e, const fs::path& path) {
  archive_entry_copy_hardlink(e, path.c_str());
}
#endif

static bool copy_data_blocks(struct archive* in, struct archive* out,
                             std::string& error) {
  const void* buff = nullptr;
  size_t size = 0;
  la_int64_t offset = 0;

  while (true) {
    const int r = archive_read_data_block(in, &buff, &size, &offset);
    if (r == ARCHIVE_EOF) {
      return true;
    }
    if (r < ARCHIVE_WARN) {
      error = (archive_error_string(in) != nullptr) ? archive_error_string(in)
                                                    : "read error";
      return false;
    }
    if (archive_write_data_block(out, buff, size, offset) < ARCHIVE_WARN) {
      error = (archive_error_string(out) != nullptr) ? archive_error_string(out)
                                                     : "write error";
      return false;
    }
  }
}

bool extract(const fs::path& archive_path, const fs::path& dest_dir,
             Status& status) {
  std::error_code ec;
  fs::create_directories(dest_dir, ec);
  if (ec) {
    status.post(
        "Cannot create directory " + to_utf8(dest_dir) + ": " + ec.message(),
        MSG_ERROR);
    return false;
  }

  status.post("Extracting " + file_name_of(archive_path));

  struct archive* in = archive_read_new();
  archive_read_support_format_zip(in);
  archive_read_support_format_tar(in);
  archive_read_support_filter_gzip(in);

  struct archive* out = archive_write_disk_new();
  const int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
                    ARCHIVE_EXTRACT_SECURE_NODOTDOT |
                    ARCHIVE_EXTRACT_SECURE_SYMLINKS;
  archive_write_disk_set_options(out, flags);

  bool ok = true;
  std::string error;

  if (open_for_reading(in, archive_path) != ARCHIVE_OK) {
    ok = false;
    error = (archive_error_string(in) != nullptr) ? archive_error_string(in)
                                                  : "cannot open archive";
  }

  struct archive_entry* entry = nullptr;
  while (ok) {
    const int r = archive_read_next_header(in, &entry);
    if (r == ARCHIVE_EOF) {
      break;
    }
    if (r < ARCHIVE_WARN) {
      ok = false;
      error = (archive_error_string(in) != nullptr) ? archive_error_string(in)
                                                    : "read error";
      break;
    }

    set_entry_path(entry, dest_dir / entry_path(entry));

    // Hard links point inside the archive, so they need the same prefix too.
    if (entry_hardlink(entry) != nullptr) {
      set_entry_hardlink(entry, dest_dir / entry_hardlink(entry));
    }

    if (archive_write_header(out, entry) < ARCHIVE_WARN) {
      ok = false;
      error = (archive_error_string(out) != nullptr) ? archive_error_string(out)
                                                     : "write error";
      break;
    }

    ok = copy_data_blocks(in, out, error);
    if (ok) {
      archive_write_finish_entry(out);
    }
  }

  archive_read_free(in);
  archive_write_free(out);

  if (!ok) {
    status.post(
        "Extraction of " + file_name_of(archive_path) + " failed: " + error,
        MSG_ERROR);
  }
  return ok;
}

static bool add_file_data(struct archive* a, const fs::path& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    return false;
  }
  std::array<char, USHRT_MAX> buf{};
  while (f) {
    f.read(buf.data(), sizeof(buf));
    const std::streamsize n = f.gcount();
    if (n > 0 &&
        0 > archive_write_data(a, buf.data(), static_cast<size_t>(n))) {
      return false;
    }
  }
  return true;
}

bool zip(const fs::path& parent, const std::string& dir_to_zip,
         const fs::path& dest_zip, Status& status) {
  status.post("Creating " + file_name_of(dest_zip));

  const fs::path& base = parent;
  const fs::path root = base / to_path(dir_to_zip);
  std::error_code ec;
  if (!fs::is_directory(root, ec)) {
    status.post("Cannot zip, no such directory: " + to_utf8(root), MSG_ERROR);
    return false;
  }

  struct archive* a = archive_write_new();
  archive_write_set_format_zip(a);
#ifdef _WIN32
  // Store names as UTF-8 (with the zip flag for it), not in the ANSI code page.
  archive_write_set_format_option(a, "zip", "hdrcharset", "UTF-8");
#endif
  if (open_for_writing(a, dest_zip) != ARCHIVE_OK) {
    status.post(
        "Cannot create " + to_utf8(dest_zip) + ": " + archive_error_string(a),
        MSG_ERROR);
    archive_write_free(a);
    return false;
  }

  bool ok = true;

  // The directory itself first, then everything in it.
  std::vector<fs::path> items;
  items.push_back(root);
  fs::recursive_directory_iterator it(root, ec);
  for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
    items.push_back(it->path());
  }

  for (size_t i = 0; i < items.size() && ok; ++i) {
    const fs::path& p = items.at(i);
    const fs::file_status st = fs::symlink_status(p, ec);
    if (fs::is_symlink(st)) {
      continue;
    }
    const bool is_dir = fs::is_directory(st);

    struct archive_entry* e = archive_entry_new();
    // Zip wants "/" separators, and a trailing one for a directory.
    fs::path rel = to_path(fs::relative(p, base));
    if (is_dir) {
      rel += "/";
    }
    set_entry_path(e, rel);
    const mode_t mode =
        S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO;
    archive_entry_set_perm(e, static_cast<unsigned>(st.permissions()) & mode);
    archive_entry_set_mtime(e, std::time(nullptr), 0);
    if (is_dir) {
      archive_entry_set_filetype(e, AE_IFDIR);
      archive_entry_set_size(e, 0);
    } else {
      archive_entry_set_filetype(e, AE_IFREG);
      archive_entry_set_size(e, static_cast<la_int64_t>(fs::file_size(p, ec)));
    }

    if (archive_write_header(a, e) != ARCHIVE_OK ||
        (!is_dir && !add_file_data(a, p))) {
      ok = false;
    }
    archive_entry_free(e);
  }

  if (!ok) {
    status.post("Failed to write " + file_name_of(dest_zip) + ": " +
                    archive_error_string(a),
                MSG_ERROR);
  }

  archive_write_close(a);
  archive_write_free(a);
  return ok;
}

}  // namespace archive_helper
