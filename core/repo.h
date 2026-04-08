#pragma once

#include <string>
#include <filesystem>

namespace yag::core {

// Initialize a new .yag/ repository in the current directory.
// If project_name is empty, uses the current folder name.
bool init(const std::string& project_name = "");

// Find the repository root by walking up from cwd
std::filesystem::path find_yag_root();

// Check if a .yag/ repository exists at or above cwd
bool is_initialized();

// Read the current branch name from .yag/HEAD
std::string get_current_branch();

// Write the current branch name to .yag/HEAD
void set_current_branch(const std::string& branch_name);

// Read the project name from .yag/config
std::string get_project_name();

} // namespace yag::core
