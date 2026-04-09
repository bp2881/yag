#pragma once

#include <string>

namespace yag::core {

// Lock a file — prevents other users from committing changes to it
bool lock_file(const std::string &filepath);

// Unlock a previously locked file
bool unlock_file(const std::string &filepath);

// Check if a file is currently locked
bool is_locked(const std::string &filepath);

// Show all currently locked files
void show_locks();

} // namespace yag::core
