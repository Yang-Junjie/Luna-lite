#pragma once

#include <imgui.h>
#include <string>

namespace lunalite::editor {

class ModalDialog {
public:
    explicit ModalDialog(std::string title, bool show_close_button = false);
    virtual ~ModalDialog() = default;

    void open();
    void close();
    bool isOpen() const;

protected:
    void draw(ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize);
    void closeCurrent();

    virtual void onDrawContent() = 0;
    virtual void onDismissed();

private:
    std::string m_title;
    bool m_show_close_button{false};
    bool m_open_requested{false};
    bool m_open{false};
};

} // namespace lunalite::editor
