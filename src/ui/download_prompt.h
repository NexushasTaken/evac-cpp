#ifndef CATAPULT_DOWNLOAD_PROMPT_H
#define CATAPULT_DOWNLOAD_PROMPT_H

#include <gtkmm.h>

#include <functional>
#include <string>

#include "ui/app_context.h"
#include "ui/task_runner.h"

// What to ask before a download. detail may be empty; the download size is
// added after it.
struct DownloadQuestion
{
    Glib::ustring title;
    std::string detail;
    Glib::ustring button;
    std::string url;
    std::string filename;  // Name of the archive in the cache.
};

// Looks up the download size on the runner's worker thread, with busy made
// insensitive meanwhile, then asks; yes is called only if the user agrees.
void confirm_download(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent, Gtk::Widget& busy,
                      const DownloadQuestion& question, const std::function<void()>& yes);

#endif
