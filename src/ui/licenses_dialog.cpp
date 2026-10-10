#include "ui/licenses_dialog.h"

#include "core/licenses.h"

LicensesDialog::LicensesDialog(Gtk::Window& parent, const std::string& data_dir)
{
    set_title("Open source licenses");
    set_transient_for(parent);
    set_modal(true);
    set_hide_on_close(true);
    set_default_size(720, 600);

    text_view_.set_editable(false);
    text_view_.set_cursor_visible(false);
    text_view_.set_wrap_mode(Gtk::WrapMode::WORD);
    text_view_.set_monospace(true);
    text_view_.set_left_margin(12);
    text_view_.set_right_margin(12);
    scroller_.set_child(text_view_);
    set_child(scroller_);

    Glib::RefPtr<Gtk::TextBuffer> buffer = text_view_.get_buffer();
    Glib::RefPtr<Gtk::TextBuffer::Tag> heading = buffer->create_tag();
    heading->property_weight() = 700;
    heading->property_scale() = 1.3;
    heading->property_justification() = Gtk::Justification::CENTER;

    std::vector<LicenseSection> sections = load_license_sections(data_dir);
    for (size_t i = 0; i < sections.size(); ++i)
    {
        buffer->insert_with_tag(buffer->end(), "\n" + sections[i].heading + "\n\n", heading);
        buffer->insert(buffer->end(), sections[i].text + "\n\n");
    }
}
