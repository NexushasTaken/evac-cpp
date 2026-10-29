#include "ui/download_prompt.h"

#include <QWidget>
#include <memory>

#include "core/http.h"
#include "ui/dialogs.h"
#include "ui/qt_helpers.h"

void confirm_download(AppContext& ctx, TaskRunner& runner, QWidget& parent,
                      QWidget& busy, const DownloadQuestion& question,
                      const std::function<void()>& yes) {
  // Shared with the worker thread, which fills in the size line.
  std::shared_ptr<std::string> size_line = std::make_shared<std::string>();

  busy.setEnabled(false);
  bool started = runner.run(
      [&ctx, question, size_line]() {
        Http http(ctx.status);
        *size_line = http.describe_download(ctx.settings, ctx.paths.cache_dir(),
                                            question.url, question.filename);
      },
      [&parent, &busy, question, size_line, yes]() {
        busy.setEnabled(true);
        std::string detail = question.detail.empty()
                                 ? *size_line
                                 : question.detail + "\n\n" + *size_line;
        dialogs::confirm(parent, qstr(question.title), qstr(detail),
                         qstr(question.button), yes);
      });
  if (!started) busy.setEnabled(true);
}
