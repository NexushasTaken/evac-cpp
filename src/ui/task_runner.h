#ifndef EVAC_TASK_RUNNER_H
#define EVAC_TASK_RUNNER_H

#include <QObject>
#include <atomic>
#include <functional>
#include <thread>

#include "core/status.h"

// Runs one slow job at a time on a worker thread, and then calls a function
// on the Qt main (GUI) thread. Create it on the main thread.
class TaskRunner : public QObject {
  Q_OBJECT

 public:
  explicit TaskRunner(Status& status);
  ~TaskRunner() override;

  // Returns false (and does nothing) if a job is already running.
  bool run(const std::function<void()>& work,
           const std::function<void()>& done);

  bool busy() const { return m_busy; }

  // Waits for the running job to end, without calling its done function.
  void wait();

 signals:
  // Emitted on the main thread after a job has ended and its done function ran.
  void finished();

 private:
  void on_finished();

  Status& m_status;
  std::thread m_thread;
  std::function<void()> m_done;
  std::atomic<bool> m_busy;
};

#endif
