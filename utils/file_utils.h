#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace yag::utils {

// Read entire file into a string
std::string read_file(const std::filesystem::path& filepath);

// Write a string to a file (creates parent directories if needed)
void write_file(const std::filesystem::path& filepath, const std::string& content);

// Recursively list all tracked files under root, skipping .yag/ and hidden files
std::vector<std::filesystem::path> list_tracked_files(const std::filesystem::path& root);

// Copy a file, creating destination parent directories if needed
void copy_file_safe(const std::filesystem::path& src, const std::filesystem::path& dst);

} // namespace yag::utils
