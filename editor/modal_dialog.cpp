#include "modal_dialog.h"

#include <utility>

namespace lunalite::editor {

ModalDialog::ModalDialog(std::string title, bool show_close_button)
    : m_title(std::move(title)),
      m_show_close_button(show_close_button)
{}

void ModalDialog::open()
{
    m_open_requested = true;
    m_open = true;
}

void ModalDialog::close()
{
    m_open_requested = false;
    m_open = false;
}

bool ModalDialog::isOpen() const
{
    return m_open;
}

void ModalDialog::draw(ImGuiWindowFlags flags)
{
    if (!m_open) {
        return;
    }

    if (m_open_requested) {
        ImGui::OpenPopup(m_title.c_str());
        m_open_requested = false;
    }

    bool open = true;
    bool* openPtr = m_show_close_button ? &open : nullptr;
    if (!ImGui::BeginPopupModal(m_title.c_str(), openPtr, flags)) {
        if (m_show_close_button && !open) {
            close();
            onDismissed();
        }
        return;
    }

    onDrawContent();

    if (m_show_close_button && !open) {
        close();
        onDismissed();
    }

    ImGui::EndPopup();
}

void ModalDialog::closeCurrent()
{
    close();
    ImGui::CloseCurrentPopup();
}

void ModalDialog::onDismissed() {}

} // namespace lunalite::editor
