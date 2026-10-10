#ifndef CATAPULT_TASK_RUNNER_H
#define CATAPULT_TASK_RUNNER_H

#include <glibmm/dispatcher.h>

#include <atomic>
#include <functional>
#include <thread>

#include "core/status.h"

// Runs one slow job at a time on a worker thread, and then calls a function
// on the GTK main thread. Create it on the main thread.
class TaskRunner
{
public:
    explicit TaskRunner(Status& status);
    ~TaskRunner();

    // Returns false (and does nothing) if a job is already running.
    bool run(const std::function<void()>& work, const std::function<void()>& done);

    bool busy() const { return busy_; }

    // Emitted on the main thread after a job has ended and its done function ran.
    sigc::signal<void()>& signal_finished() { return finished_; }

    // Waits for the running job to end, without calling its done function.
    void wait();

private:
    void on_finished();

    Status& status_;
    Glib::Dispatcher dispatcher_;
    std::thread thread_;
    std::function<void()> done_;
    std::atomic<bool> busy_;
    sigc::signal<void()> finished_;
};

#endif
