#include "ui/log_view.h"

#include <QFontDatabase>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>

#include "ui/qt_helpers.h"

LogView::LogView(Status& status) : status_(status) {
  setFixedHeight(150);
  setReadOnly(true);
  setLineWrapMode(QPlainTextEdit::WidgetWidth);
  setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  status_.set_listener(
      [this](const StatusMessage& message) { on_message(message); });
}

LogView::~LogView() { status_.set_listener(Status::Listener()); }

void LogView::on_message(const StatusMessage& message) {
  // Queued: runs on the main thread. Dropped if the view is gone by then.
  QMetaObject::invokeMethod(
      this, [this, message]() { add(message); }, Qt::QueuedConnection);
}

void LogView::add(const StatusMessage& message) {
  QTextCharFormat format;
  if (message.type == MSG_WARN)
    format.setForeground(QColor("#c8a000"));
  else if (message.type == MSG_ERROR)
    format.setForeground(QColor("#d02020"));
  else if (message.type == MSG_DEBUG)
    format.setForeground(QColor("#808080"));

  QTextCursor cursor(document());
  cursor.movePosition(QTextCursor::End);
  if (!document()->isEmpty()) cursor.insertBlock();
  cursor.insertText(qstr(message.text), format);
  verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}
