#include "ui/download_prompt.h"

#include <memory>

#include "core/http.h"
#include "ui/dialogs.h"

void confirm_download(AppContext& ctx, TaskRunner& runner, Gtk::Window& parent, Gtk::Widget& busy,
                      const DownloadQuestion& question, const std::function<void()>& yes)
{
    // Shared with the worker thread, which fills in the size line.
    std::shared_ptr<std::string> size_line = std::make_shared<std::string>();

    busy.set_sensitive(false);
    bool started = runner.run(
        [&ctx, question, size_line]() {
            Http http(ctx.status);
            *size_line = http.describe_download(ctx.settings, ctx.paths.cache_dir(), question.url, question.filename);
        },
        [&parent, &busy, question, size_line, yes]() {
            busy.set_sensitive(true);
            std::string detail = question.detail.empty() ? *size_line : question.detail + "\n\n" + *size_line;
            dialogs::confirm(parent, question.title, detail, question.button, yes);
        });
    if (!started)
        busy.set_sensitive(true);
}
