#include "core/commit.h"
#include "core/repo.h"
#include "core/staging.h"
#include "utils/file_utils.h"
#include "utils/hash.h"

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace yag::core {

// Format current time as ISO 8601 (UTC)
static std::string now_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time_t, &tm);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

// Build the deterministic metadata string for a commit
static std::string build_metadata(const std::string& parent,
                                  const std::string& branch,
                                  const std::string& timestamp,
                                  const std::string& message,
                                  const std::vector<std::pair<std::string, std::string>>& files)
{
    std::ostringstream oss;
    oss << "parent: " << parent << "\n"
        << "branch: " << branch << "\n"
        << "timestamp: " << timestamp << "\n"
        << "message: " << message << "\n"
        << "files:\n";
    for (const auto& [path, hash] : files) {
        oss << "  " << path << " " << hash << "\n";
    }
    return oss.str();
}

std::string create_commit(const std::string& message) {
    fs::path root = find_yag_root();
    fs::path yag = root / ".yag";

    // 1. Get current branch and its latest commit (parent)
    std::string branch = get_current_branch();
    std::string parent = utils::read_file(yag / "branches" / branch);

    // ------------------------------------------------------------------
    // 2. Read file list from the INDEX (staging area)
    //    Commit uses what's been staged, not a raw filesystem scan.
    // ------------------------------------------------------------------
    auto file_entries = read_index();

    if (file_entries.empty()) {
        std::cerr << "Nothing staged. Use 'yag add' to stage files.\n";
        return "";
    }

    // ------------------------------------------------------------------
    // 3. CHANGE DETECTION: compare index against parent commit
    //    If the staged snapshot is identical to the parent → block commit
    // ------------------------------------------------------------------
    if (parent != "none") {
        Commit parent_commit = read_commit(parent);
        std::map<std::string, std::string> parent_map(
            parent_commit.files.begin(), parent_commit.files.end());
        std::map<std::string, std::string> index_map(
            file_entries.begin(), file_entries.end());

        if (index_map == parent_map) {
            std::cout << "No changes to commit.\n";
            return "";
        }
    }

    // 4. Ensure all objects referenced by the index exist
    //    (they should — stage_file/stage_all stores them — but be safe)
    for (const auto& [rel_path, obj_hash] : file_entries) {
        fs::path obj_path = yag / "objects" / obj_hash;
        if (!fs::exists(obj_path)) {
            // Object missing — re-hash from working directory
            fs::path abs_path = root / rel_path;
            if (fs::exists(abs_path)) {
                std::string content = utils::read_file(abs_path);
                utils::write_file(obj_path, content);
            }
        }
    }

    // 5. Build metadata (deterministic: sorted files, fixed field order)
    std::string timestamp = now_iso8601();
    std::string metadata = build_metadata(parent, branch, timestamp, message, file_entries);

    // 6. Hash metadata → commit ID
    std::string commit_id = utils::hash_string(metadata);

    // 7. Write full commit file (with id: header)
    std::string commit_content = "id: " + commit_id + "\n" + metadata;
    utils::write_file(yag / "commits" / commit_id, commit_content);

    // 8. Update branch pointer to this new commit
    utils::write_file(yag / "branches" / branch, commit_id);

    std::cout << "[" << branch << " " << commit_id.substr(0, 8) << "] "
              << message << "\n";
    return commit_id;
}

Commit read_commit(const std::string& commit_id) {
    fs::path root = find_yag_root();
    fs::path commit_path = root / ".yag" / "commits" / commit_id;

    if (!fs::exists(commit_path)) {
        throw std::runtime_error("Commit not found: " + commit_id);
    }

    std::string content = utils::read_file(commit_path);
    std::istringstream iss(content);
    std::string line;
    Commit commit;
    bool in_files = false;

    while (std::getline(iss, line)) {
        if (in_files) {
            // Parse file entry: "  <path> <hash>"
            if (line.size() > 2 && line[0] == ' ' && line[1] == ' ') {
                std::string entry = line.substr(2);
                auto space = entry.find(' ');
                if (space != std::string::npos) {
                    commit.files.emplace_back(
                        entry.substr(0, space),
                        entry.substr(space + 1)
                    );
                }
            }
            continue;
        }

        // Parse header fields
        if (line.rfind("id: ", 0) == 0) {
            commit.id = line.substr(4);
        } else if (line.rfind("parent: ", 0) == 0) {
            commit.parent = line.substr(8);
        } else if (line.rfind("branch: ", 0) == 0) {
            commit.branch = line.substr(8);
        } else if (line.rfind("timestamp: ", 0) == 0) {
            commit.timestamp = line.substr(11);
        } else if (line.rfind("message: ", 0) == 0) {
            commit.message = line.substr(9);
        } else if (line == "files:") {
            in_files = true;
        }
    }

    return commit;
}

void show_log() {
    fs::path root = find_yag_root();
    fs::path yag = root / ".yag";

    // Start from current branch's latest commit
    std::string branch = get_current_branch();
    std::string commit_id = utils::read_file(yag / "branches" / branch);

    if (commit_id == "none") {
        std::cout << "No commits yet on branch '" << branch << "'.\n";
        return;
    }

    // Walk the parent chain
    while (commit_id != "none") {
        Commit c = read_commit(commit_id);

        std::cout << "commit " << c.id << "\n"
                  << "Branch: " << c.branch << "\n"
                  << "Date:   " << c.timestamp << "\n"
                  << "\n"
                  << "    " << c.message << "\n"
                  << "\n";

        commit_id = c.parent;
    }
}

} // namespace yag::core
