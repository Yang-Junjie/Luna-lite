#include "../../core/log.h"
#include "file_dialogs.h"

#include <filesystem>

namespace lunalite::FileDialogs {

std::filesystem::path openFile(const char*, const std::string&)
{
    LUNA_CORE_WARN("Native open file dialog is not implemented on this platform");
    return {};
}

std::filesystem::path saveFile(const char*, const std::string&)
{
    LUNA_CORE_WARN("Native save file dialog is not implemented on this platform");
    return {};
}

std::filesystem::path selectDirectory(const std::string&)
{
    LUNA_CORE_WARN("Native select directory dialog is not implemented on this platform");
    return {};
}

} // namespace lunalite::FileDialogs
