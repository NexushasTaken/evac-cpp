#include "ui/download_prompt.h"

#include <QWidget>
#include <functional>
#include <memory>
#include <string>

#include "core/http.h"
#include "ui/app_context.h"
#include "ui/dialogs.h"
#include "ui/qt_helpers.h"
#include "ui/task_runner.h"

void confirm_download(AppContext& ctx, TaskRunner& runner, QWidget& parent,
                      QWidget& busy, const DownloadQuestion& question,
                      const std::function<void()>& yes) {
  // Shared with the worker thread, which fills in the size line.
  const std::shared_ptr<std::string> size_line =
      std::make_shared<std::string>();

  busy.setEnabled(false);
  const bool started = runner.run(
      [&ctx, question, size_line] {
        Http http(ctx.status);
        *size_line = http.describe_download(ctx.settings, ctx.paths.cache_dir(),
                                            question.url, question.filename);
      },
      [&parent, &busy, question, size_line, yes] {
        busy.setEnabled(true);
        const std::string detail = question.detail.empty()
                                       ? *size_line
                                       : question.detail + "\n\n" + *size_line;
        dialogs::confirm(parent, qstr(question.title), qstr(detail),
                         qstr(question.button), yes);
      });
  if (!started) {
    busy.setEnabled(true);
  }
}
