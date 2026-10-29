#include "ui/task_runner.h"

#include <exception>

TaskRunner::TaskRunner(Status& status) : status_(status), busy_(false) {}

TaskRunner::~TaskRunner() { wait(); }

bool TaskRunner::run(const std::function<void()>& work,
                     const std::function<void()>& done) {
  if (busy_) return false;

  busy_ = true;
  done_ = done;
  thread_ = std::thread([this, work]() {
    try {
      work();
    } catch (const std::exception& e) {
      status_.post(std::string("Unexpected error: ") + e.what(), MSG_ERROR);
    }
    // Queued: runs on the thread this object lives in (the main thread).
    // Dropped if the runner is gone by then.
    QMetaObject::invokeMethod(
        this, [this]() { on_finished(); }, Qt::QueuedConnection);
  });
  return true;
}

void TaskRunner::on_finished() {
  if (thread_.joinable()) thread_.join();
  busy_ = false;

  std::function<void()> done = done_;
  done_ = nullptr;
  if (done) done();
  emit finished();
}

void TaskRunner::wait() {
  if (thread_.joinable()) thread_.join();
  busy_ = false;
  done_ = nullptr;
}
