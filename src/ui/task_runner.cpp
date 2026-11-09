#include "ui/task_runner.h"

#include <qnamespace.h>
#include <qobjectdefs.h>
#include <qtmetamacros.h>

#include <exception>
#include <functional>
#include <string>

#include "core/status.h"

TaskRunner::TaskRunner(Status& status) : m_status(status), m_busy(false) {}

TaskRunner::~TaskRunner() { wait(); }

bool TaskRunner::run(const std::function<void()>& work,
                     const std::function<void()>& done) {
  if (m_busy) {
    return false;
  }

  m_busy = true;
  m_done = done;
  m_thread = std::thread([this, work] {
    try {
      work();
    } catch (const std::exception& e) {
      m_status.post(std::string("Unexpected error: ") + e.what(), MSG_ERROR);
    }
    // Queued: runs on the thread this object lives in (the main thread).
    // Dropped if the runner is gone by then.
    QMetaObject::invokeMethod(
        this, [this] { on_finished(); }, Qt::QueuedConnection);
  });
  return true;
}

void TaskRunner::on_finished() {
  if (m_thread.joinable()) {
    m_thread.join();
  }
  m_busy = false;

  const std::function<void()> done = m_done;
  m_done = nullptr;
  if (done) {
    done();
  }
  emit finished();
}

void TaskRunner::wait() {
  if (m_thread.joinable()) {
    m_thread.join();
  }
  m_busy = false;
  m_done = nullptr;
}
