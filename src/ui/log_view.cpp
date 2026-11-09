#include "ui/log_view.h"

#include <qcolor.h>
#include <qnamespace.h>
#include <qobjectdefs.h>
#include <qplaintextedit.h>
#include <qtextoption.h>

#include <QFontDatabase>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>

#include "core/status.h"
#include "ui/qt_helpers.h"

LogView::LogView(Status& status) : m_status(status) {
  setFixedHeight(150);
  setReadOnly(true);
  setLineWrapMode(QPlainTextEdit::WidgetWidth);
  setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  m_status.set_listener(
      [this](const StatusMessage& message) { on_message(message); });
}

LogView::~LogView() { m_status.set_listener(Status::Listener()); }

void LogView::on_message(const StatusMessage& message) {
  // Queued: runs on the main thread. Dropped if the view is gone by then.
  QMetaObject::invokeMethod(
      this, [this, message] { add(message); }, Qt::QueuedConnection);
}

void LogView::add(const StatusMessage& message) {
  QTextCharFormat format;
  if (message.type == MSG_WARN) {
    format.setForeground(QColor("#c8a000"));
  } else if (message.type == MSG_ERROR) {
    format.setForeground(QColor("#d02020"));
  } else if (message.type == MSG_DEBUG) {
    format.setForeground(QColor("#808080"));
  }

  QTextCursor cursor(document());
  cursor.movePosition(QTextCursor::End);
  if (!document()->isEmpty()) {
    cursor.insertBlock();
  }
  cursor.insertText(qstr(message.text), format);
  verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}
