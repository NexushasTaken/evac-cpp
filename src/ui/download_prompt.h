#ifndef EVAC_DOWNLOAD_PROMPT_H
#define EVAC_DOWNLOAD_PROMPT_H

#include <QString>

#include <functional>
#include <string>

#include "ui/app_context.h"
#include "ui/task_runner.h"

class QWidget;

// What to ask before a download. detail may be empty; the download size is
// added after it.
struct DownloadQuestion
{
    std::string title;
    std::string detail;
    std::string button;
    std::string url;
    std::string filename;  // Name of the archive in the cache.
};

// Looks up the download size on the runner's worker thread, with busy disabled
// meanwhile, then asks; yes is called only if the user agrees.
void confirm_download(AppContext& ctx, TaskRunner& runner, QWidget& parent, QWidget& busy,
                      const DownloadQuestion& question, const std::function<void()>& yes);

#endif
