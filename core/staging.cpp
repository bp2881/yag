#include "core/staging.h"
#include "core/repo.h"
#include "core/commit.h"
#include "utils/file_utils.h"
#include "utils/hash.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

namespace yag::core {

// ---------------------------------------------------------------------------
// Index file format (.yag/index):
//   <relative_path> <sha256_hash>
// One entry per line, sorted by path. Empty file = nothing staged.
// ---------------------------------------------------------------------------

std::vector<std::pair<std::string, std::string>> read_index() {
    fs::path root = find_yag_root();
    fs::path index_path = root / ".yag" / "index";

    std::vector<std::pair<std::string, std::string>> entries;

    if (!fs::exists(index_path)) {
        return entries; // empty index
    }

    std::string content = utils::read_file(index_path);
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        auto space = line.find(' ');
        if (space != std::string::npos) {
            entries.emplace_back(line.substr(0, space), line.substr(space + 1));
        }
    }

    return entries;
}

void write_index(const std::vector<std::pair<std::string, std::string>>& entries) {
    fs::path root = find_yag_root();
    fs::path index_path = root / ".yag" / "index";

    // Sort entries by path for deterministic output
    auto sorted = entries;
    std::sort(sorted.begin(), sorted.end());

    std::ostringstream oss;
    for (const auto& [path, hash] : sorted) {
        oss << path << " " << hash << "\n";
    }

    utils::write_file(index_path, oss.str());
}

bool stage_file(const std::string& filepath) {
    fs::path root = find_yag_root();

    // Resolve the file path relative to repo root
    fs::path abs_path;
    if (fs::path(filepath).is_absolute()) {
        abs_path = filepath;
    } else {
        abs_path = fs::current_path() / filepath;
    }
    abs_path = fs::canonical(abs_path);

    // Safety: file must exist
    if (!fs::exists(abs_path) || !fs::is_regular_file(abs_path)) {
        std::cerr << "Error: file not found: " << filepath << "\n";
        return false;
    }

    // Safety: file must be inside the repo root (and not in .yag/)
    fs::path rel_path = fs::relative(abs_path, root);
    std::string rel_str = rel_path.generic_string();

    if (rel_str.rfind(".yag", 0) == 0 || rel_str.rfind(".", 0) == 0) {
        std::cerr << "Error: cannot stage hidden or .yag files: " << rel_str << "\n";
        return false;
    }

    // Hash the file content and store the object
    std::string content = utils::read_file(abs_path);
    std::string obj_hash = utils::hash_string(content);

    fs::path obj_path = root / ".yag" / "objects" / obj_hash;
    if (!fs::exists(obj_path)) {
        utils::write_file(obj_path, content);
    }

    // Read current index, update or add entry for this path
    auto entries = read_index();
    bool found = false;
    for (auto& [path, hash] : entries) {
        if (path == rel_str) {
            hash = obj_hash;
            found = true;
            break;
        }
    }
    if (!found) {
        entries.emplace_back(rel_str, obj_hash);
    }

    write_index(entries);
    std::cout << "Staged: " << rel_str << "\n";
    return true;
}

bool stage_all() {
    fs::path root = find_yag_root();

    // Get all tracked files in the working directory
    auto tracked = utils::list_tracked_files(root);
    if (tracked.empty()) {
        std::cout << "No files to stage.\n";
        return false;
    }

    // Read current index to compare — only stage files that actually changed
    auto current_index = read_index();
    std::map<std::string, std::string> index_map(current_index.begin(), current_index.end());

    // Also keep index entries for files that still exist
    std::vector<std::pair<std::string, std::string>> new_entries;
    int staged_count = 0;

    for (const auto& rel_path : tracked) {
        std::string rel_str = rel_path.generic_string();
        fs::path abs_path = root / rel_path;

        std::string content = utils::read_file(abs_path);
        std::string obj_hash = utils::hash_string(content);

        // Store object if not present
        fs::path obj_path = root / ".yag" / "objects" / obj_hash;
        if (!fs::exists(obj_path)) {
            utils::write_file(obj_path, content);
        }

        // Check if this file's hash differs from index
        auto it = index_map.find(rel_str);
        if (it == index_map.end() || it->second != obj_hash) {
            staged_count++;
        }

        new_entries.emplace_back(rel_str, obj_hash);
    }

    write_index(new_entries);

    if (staged_count == 0) {
        std::cout << "Nothing to stage (working directory matches index).\n";
    } else {
        std::cout << "Staged " << staged_count << " file(s).\n";
    }

    return staged_count > 0;
}

void show_status() {
    fs::path root = find_yag_root();
    fs::path yag = root / ".yag";

    std::string branch = get_current_branch();
    std::cout << "On branch " << branch << "\n\n";

    // Read the current index
    auto index_entries = read_index();
    std::map<std::string, std::string> index_map(index_entries.begin(), index_entries.end());

    // Read the last commit's file list (HEAD)
    std::string head_id = utils::read_file(yag / "branches" / branch);
    std::map<std::string, std::string> head_map;
    if (head_id != "none") {
        Commit head = read_commit(head_id);
        head_map = std::map<std::string, std::string>(head.files.begin(), head.files.end());
    }

    // Get all tracked files in working directory
    auto tracked = utils::list_tracked_files(root);
    std::map<std::string, std::string> working_map;
    for (const auto& rel_path : tracked) {
        std::string rel_str = rel_path.generic_string();
        std::string content = utils::read_file(root / rel_path);
        working_map[rel_str] = utils::hash_string(content);
    }

    // --- Changes staged for commit (index vs HEAD) ---
    bool has_staged = false;
    for (const auto& [path, hash] : index_map) {
        auto it = head_map.find(path);
        if (it == head_map.end()) {
            if (!has_staged) { std::cout << "Changes staged for commit:\n"; has_staged = true; }
            std::cout << "  new file:   " << path << "\n";
        } else if (it->second != hash) {
            if (!has_staged) { std::cout << "Changes staged for commit:\n"; has_staged = true; }
            std::cout << "  modified:   " << path << "\n";
        }
    }
    // Files in HEAD but not in index = staged for deletion
    for (const auto& [path, hash] : head_map) {
        if (index_map.find(path) == index_map.end()) {
            if (!has_staged) { std::cout << "Changes staged for commit:\n"; has_staged = true; }
            std::cout << "  deleted:    " << path << "\n";
        }
    }
    if (has_staged) std::cout << "\n";

    // --- Changes not staged (working dir vs index) ---
    bool has_unstaged = false;
    for (const auto& [path, hash] : working_map) {
        auto it = index_map.find(path);
        if (it == index_map.end()) {
            // File exists in working dir but not in index → new/untracked
            // Only show as "not staged" if it was in HEAD (modified but not re-staged)
            // Otherwise it's just a new file not yet added
            if (head_map.find(path) != head_map.end()) {
                if (!has_unstaged) { std::cout << "Changes not staged for commit:\n"; has_unstaged = true; }
                std::cout << "  deleted from index: " << path << "\n";
            }
        } else if (it->second != hash) {
            if (!has_unstaged) { std::cout << "Changes not staged for commit:\n"; has_unstaged = true; }
            std::cout << "  modified:   " << path << "\n";
        }
    }
    if (has_unstaged) std::cout << "\n";

    // --- Untracked files (in working dir, not in index, not in HEAD) ---
    bool has_untracked = false;
    for (const auto& [path, hash] : working_map) {
        if (index_map.find(path) == index_map.end() && head_map.find(path) == head_map.end()) {
            if (!has_untracked) { std::cout << "Untracked files:\n"; has_untracked = true; }
            std::cout << "  " << path << "\n";
        }
    }
    if (has_untracked) std::cout << "\n";

    if (!has_staged && !has_unstaged && !has_untracked) {
        std::cout << "Nothing to commit, working tree clean.\n";
    } else if (!has_staged) {
        std::cout << "No changes staged for commit (use 'yag add' to stage).\n";
    }
}

} // namespace yag::core
