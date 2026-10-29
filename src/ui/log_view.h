#ifndef EVAC_LOG_VIEW_H
#define EVAC_LOG_VIEW_H

#include <QPlainTextEdit>

#include "core/status.h"

// Shows the status messages. Messages can be posted from any thread.
class LogView : public QPlainTextEdit {
 public:
  explicit LogView(Status& status);
  ~LogView() override;

 private:
  void on_message(const StatusMessage& message);  // Any thread.
  void add(const StatusMessage& message);         // Main thread.

  Status& status_;
};

#endif
