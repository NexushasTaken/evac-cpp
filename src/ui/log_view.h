#ifndef CATAPULT_LOG_VIEW_H
#define CATAPULT_LOG_VIEW_H

#include <glibmm/dispatcher.h>
#include <gtkmm.h>

#include <deque>
#include <mutex>

#include "core/status.h"

// Shows the status messages. Messages can be posted from any thread.
class LogView : public Gtk::ScrolledWindow
{
public:
    explicit LogView(Status& status);
    ~LogView() override;

private:
    void on_message(const StatusMessage& message);  // Any thread.
    void flush();                                   // Main thread.

    Status& status_;
    Gtk::TextView text_view_;
    Glib::RefPtr<Gtk::TextBuffer::Tag> warning_tag_;
    Glib::RefPtr<Gtk::TextBuffer::Tag> error_tag_;
    Glib::RefPtr<Gtk::TextBuffer::Tag> debug_tag_;
    Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark_;

    Glib::Dispatcher dispatcher_;
    std::mutex mutex_;
    std::deque<StatusMessage> pending_;
};

#endif
