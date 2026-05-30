#pragma once
#include <filesystem>
#include <string>

namespace lunalite {
namespace FileDialogs {

std::filesystem::path openFile(const char* filter, const std::string& defaultPath);
std::filesystem::path saveFile(const char* filter, const std::string& defaultPath);
std::filesystem::path selectDirectory(const std::string& defaultPath);

} // namespace FileDialogs
} // namespace lunalite
