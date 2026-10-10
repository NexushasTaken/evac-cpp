#include "core/archive.h"

#include <archive.h>
#include <archive_entry.h>

#include <filesystem>
#include <ctime>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace archive_helper
{

static std::string file_name_of(const std::string& path)
{
    return fs::path(path).filename().string();
}

bool extract(const std::string& archive_path, const std::string& dest_dir, Status& status)
{
    std::error_code ec;
    fs::create_directories(dest_dir, ec);
    if (ec)
    {
        status.post("Cannot create directory " + dest_dir + ": " + ec.message(), MSG_ERROR);
        return false;
    }

    status.post("Extracting " + file_name_of(archive_path));

    struct archive* in = archive_read_new();
    archive_read_support_format_zip(in);
    archive_read_support_format_tar(in);
    archive_read_support_filter_gzip(in);

    struct archive* out = archive_write_disk_new();
    int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_SECURE_NODOTDOT
                | ARCHIVE_EXTRACT_SECURE_SYMLINKS;
    archive_write_disk_set_options(out, flags);

    bool ok = true;
    std::string error;

    if (archive_read_open_filename(in, archive_path.c_str(), 65536) != ARCHIVE_OK)
    {
        ok = false;
        error = archive_error_string(in) ? archive_error_string(in) : "cannot open archive";
    }

    struct archive_entry* entry = nullptr;
    while (ok)
    {
        int r = archive_read_next_header(in, &entry);
        if (r == ARCHIVE_EOF)
            break;
        if (r < ARCHIVE_WARN)
        {
            ok = false;
            error = archive_error_string(in) ? archive_error_string(in) : "read error";
            break;
        }

        std::string target = (fs::path(dest_dir) / archive_entry_pathname(entry)).string();
        archive_entry_set_pathname(entry, target.c_str());

        // Hard links point inside the archive, so they need the same prefix too.
        const char* hardlink = archive_entry_hardlink(entry);
        if (hardlink)
        {
            std::string link_target = (fs::path(dest_dir) / hardlink).string();
            archive_entry_set_hardlink(entry, link_target.c_str());
        }

        if (archive_write_header(out, entry) < ARCHIVE_WARN)
        {
            ok = false;
            error = archive_error_string(out) ? archive_error_string(out) : "write error";
            break;
        }

        const void* buff;
        size_t size;
        la_int64_t offset;
        while (true)
        {
            r = archive_read_data_block(in, &buff, &size, &offset);
            if (r == ARCHIVE_EOF)
                break;
            if (r < ARCHIVE_WARN)
            {
                ok = false;
                error = archive_error_string(in) ? archive_error_string(in) : "read error";
                break;
            }
            if (archive_write_data_block(out, buff, size, offset) < ARCHIVE_WARN)
            {
                ok = false;
                error = archive_error_string(out) ? archive_error_string(out) : "write error";
                break;
            }
        }
        if (ok)
            archive_write_finish_entry(out);
    }

    archive_read_free(in);
    archive_write_free(out);

    if (!ok)
        status.post("Extraction of " + file_name_of(archive_path) + " failed: " + error, MSG_ERROR);
    return ok;
}

static bool add_file_data(struct archive* a, const fs::path& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    char buf[65536];
    while (f)
    {
        f.read(buf, sizeof(buf));
        std::streamsize n = f.gcount();
        if (n > 0 && archive_write_data(a, buf, (size_t)n) < 0)
            return false;
    }
    return true;
}

bool zip(const std::string& parent, const std::string& dir_to_zip, const std::string& dest_zip, Status& status)
{
    status.post("Creating " + file_name_of(dest_zip));

    fs::path base(parent);
    fs::path root = base / dir_to_zip;
    std::error_code ec;
    if (!fs::is_directory(root, ec))
    {
        status.post("Cannot zip, no such directory: " + root.string(), MSG_ERROR);
        return false;
    }

    struct archive* a = archive_write_new();
    archive_write_set_format_zip(a);
    if (archive_write_open_filename(a, dest_zip.c_str()) != ARCHIVE_OK)
    {
        status.post("Cannot create " + dest_zip + ": " + archive_error_string(a), MSG_ERROR);
        archive_write_free(a);
        return false;
    }

    bool ok = true;

    // The directory itself first, then everything in it.
    std::vector<fs::path> items;
    items.push_back(root);
    fs::recursive_directory_iterator it(root, ec);
    for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        items.push_back(it->path());

    for (size_t i = 0; i < items.size() && ok; ++i)
    {
        const fs::path& p = items[i];
        fs::file_status st = fs::symlink_status(p, ec);
        if (fs::is_symlink(st))
            continue;
        bool is_dir = fs::is_directory(st);

        struct archive_entry* e = archive_entry_new();
        std::string rel = fs::relative(p, base).generic_string();
        archive_entry_set_pathname(e, (is_dir ? rel + "/" : rel).c_str());
        archive_entry_set_perm(e, (unsigned)st.permissions() & 07777);
        archive_entry_set_mtime(e, std::time(nullptr), 0);
        if (is_dir)
        {
            archive_entry_set_filetype(e, AE_IFDIR);
            archive_entry_set_size(e, 0);
        }
        else
        {
            archive_entry_set_filetype(e, AE_IFREG);
            archive_entry_set_size(e, (la_int64_t)fs::file_size(p, ec));
        }

        if (archive_write_header(a, e) != ARCHIVE_OK)
            ok = false;
        else if (!is_dir && !add_file_data(a, p))
            ok = false;
        archive_entry_free(e);
    }

    if (!ok)
        status.post("Failed to write " + file_name_of(dest_zip) + ": " + archive_error_string(a), MSG_ERROR);

    archive_write_close(a);
    archive_write_free(a);
    return ok;
}

}  // namespace archive_helper
