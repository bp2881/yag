#pragma once

#include <string>
#include <filesystem>

namespace yag::utils {

// Hash a string and return hex digest
std::string hash_string(const std::string& content);

// Hash a file's contents and return hex digest
std::string hash_file(const std::filesystem::path& filepath);

} // namespace yag::utils
