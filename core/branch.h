#pragma once

#include <string>
#include <vector>

namespace yag::core {

// Create a new branch pointing to the current branch's latest commit
bool create_branch(const std::string& name);

// Switch to an existing branch by updating HEAD
bool checkout(const std::string& name);

// List all branches, marking the current one with *
void list_branches();

} // namespace yag::core
