#pragma once

#include <string>
#include <filesystem>

namespace yag::core {

// Push local commits and objects to the central repository
bool push();

// Pull new commits and objects from the central repository
bool pull();

// --- Future extension hooks ---

// Placeholder: AI-based merge conflict resolution
// void ai_resolve_conflicts(const std::string& filepath);

// Placeholder: VS Code extension status query
// std::string get_repo_status_json();

} // namespace yag::core
