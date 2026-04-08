#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace yag::core {

// Represents a single commit
struct Commit {
    std::string id;
    std::string parent;
    std::string branch;
    std::string timestamp;
    std::string message;
    std::vector<std::pair<std::string, std::string>> files; // {relative_path, object_hash}
};

// Create a new commit with the given message, returns commit ID
std::string create_commit(const std::string& message);

// Display commit history for the current branch (reverse chronological)
void show_log();

// Parse a commit file into a Commit struct
Commit read_commit(const std::string& commit_id);

} // namespace yag::core
