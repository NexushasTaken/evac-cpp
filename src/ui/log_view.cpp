#include "ui/log_view.h"

LogView::LogView(Status& status) : status_(status)
{
    set_min_content_height(150);
    set_vexpand(false);

    text_view_.set_editable(false);
    text_view_.set_cursor_visible(false);
    text_view_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    text_view_.set_monospace(true);
    set_child(text_view_);

    Glib::RefPtr<Gtk::TextBuffer> buffer = text_view_.get_buffer();
    warning_tag_ = buffer->create_tag();
    warning_tag_->property_foreground() = "#c8a000";
    error_tag_ = buffer->create_tag();
    error_tag_->property_foreground() = "#d02020";
    debug_tag_ = buffer->create_tag();
    debug_tag_->property_foreground() = "#808080";
    end_mark_ = buffer->create_mark(buffer->end(), false);

    dispatcher_.connect(sigc::mem_fun(*this, &LogView::flush));
    status_.set_listener(sigc::mem_fun(*this, &LogView::on_message));
}

LogView::~LogView()
{
    status_.set_listener(Status::Listener());
}

void LogView::on_message(const StatusMessage& message)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.push_back(message);
    }
    dispatcher_.emit();
}

void LogView::flush()
{
    std::deque<StatusMessage> messages;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        messages.swap(pending_);
    }

    Glib::RefPtr<Gtk::TextBuffer> buffer = text_view_.get_buffer();
    for (size_t i = 0; i < messages.size(); ++i)
    {
        Glib::RefPtr<Gtk::TextBuffer::Tag> tag;
        if (messages[i].type == MSG_WARN)
            tag = warning_tag_;
        else if (messages[i].type == MSG_ERROR)
            tag = error_tag_;
        else if (messages[i].type == MSG_DEBUG)
            tag = debug_tag_;

        if (tag)
            buffer->insert_with_tag(buffer->end(), messages[i].text + "\n", tag);
        else
            buffer->insert(buffer->end(), messages[i].text + "\n");
    }
    text_view_.scroll_to(end_mark_);
}
