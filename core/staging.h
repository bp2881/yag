#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace yag::core {

// Read the current index (.yag/index) → sorted list of {path, hash} pairs
std::vector<std::pair<std::string, std::string>> read_index();

// Write entries to the index file (.yag/index)
void write_index(const std::vector<std::pair<std::string, std::string>>& entries);

// Stage a single file: hash it, store the object, update its index entry
bool stage_file(const std::string& filepath);

// Stage all modified/new tracked files (equivalent to "yag add .")
bool stage_all();

// Show working directory status: staged, unstaged, and new files
void show_status();

} // namespace yag::core
