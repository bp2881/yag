#include "core/sync.h"
#include "core/repo.h"
#include "core/commit.h"
#include "core/staging.h"
#include "utils/file_utils.h"

#include <iostream>
#include <cstdlib>
#include <map>

namespace fs = std::filesystem;

namespace yag::core {

// ---------------------------------------------------------------------------
// Resolve central repo path using project_name from .yag/config.
// Path: ~/.yag-central/projects/<project_name>/
// ---------------------------------------------------------------------------
static fs::path get_central_path() {
    const char* home = std::getenv("HOME");
    if (!home) {
        throw std::runtime_error("HOME environment variable not set.");
    }

    // Use config-based project name (NOT folder name)
    std::string project_name = get_project_name();
    return fs::path(home) / ".yag-central" / "projects" / project_name;
}

// ---------------------------------------------------------------------------
// Copy only files that don't already exist in dst_dir (skip existing).
// This ensures push/pull only transfers NEW objects and commits.
// ---------------------------------------------------------------------------
static void copy_missing(const fs::path& src_dir, const fs::path& dst_dir) {
    if (!fs::exists(src_dir)) return;
    fs::create_directories(dst_dir);

    for (const auto& entry : fs::directory_iterator(src_dir)) {
        if (!entry.is_regular_file()) continue;
        fs::path dst_file = dst_dir / entry.path().filename();
        if (!fs::exists(dst_file)) {
            fs::copy_file(entry.path(), dst_file);
        }
    }
}

// ---------------------------------------------------------------------------
// Build a file hash map from a commit's file list for easy comparison.
// ---------------------------------------------------------------------------
static std::map<std::string, std::string> file_hash_map(const Commit& c) {
    return std::map<std::string, std::string>(c.files.begin(), c.files.end());
}

// ---------------------------------------------------------------------------
// Restore working directory and index from a commit.
// Removes current tracked files, then writes files from the commit's snapshot.
// ---------------------------------------------------------------------------
static void restore_from_commit(const fs::path& root, const Commit& c) {
    fs::path yag = root / ".yag";

    // Remove current tracked files
    auto current_files = utils::list_tracked_files(root);
    for (const auto& f : current_files) {
        fs::remove(root / f);
    }

    // Restore each file from objects/
    for (const auto& [rel_path, obj_hash] : c.files) {
        fs::path obj_path = yag / "objects" / obj_hash;
        std::string content = utils::read_file(obj_path);
        utils::write_file(root / rel_path, content);
    }

    // Clean up empty directories left behind
    for (auto it = fs::recursive_directory_iterator(root);
         it != fs::recursive_directory_iterator(); ++it) {
        const auto& entry = *it;
        std::string fname = entry.path().filename().string();
        if (!fname.empty() && fname[0] == '.') {
            if (entry.is_directory()) it.disable_recursion_pending();
            continue;
        }
        if (entry.is_directory() && fs::is_empty(entry.path())) {
            fs::remove(entry.path());
        }
    }

    // Update the index to match the commit
    write_index(c.files);
}

bool push() {
    fs::path root = find_yag_root();
    fs::path yag = root / ".yag";
    fs::path central = get_central_path();

    std::string branch = get_current_branch();
    std::string commit_id = utils::read_file(yag / "branches" / branch);

    if (commit_id == "none") {
        std::cerr << "Nothing to push (no commits on '" << branch << "').\n";
        return false;
    }

    // Create central directory structure if it doesn't exist yet
    fs::create_directories(central / "objects");
    fs::create_directories(central / "commits");
    fs::create_directories(central / "branches");

    // 1. Copy ONLY missing objects (content-addressed, so duplicates are skipped)
    copy_missing(yag / "objects", central / "objects");

    // 2. Copy ONLY missing commits
    copy_missing(yag / "commits", central / "commits");

    // 3. Update branch pointer in central
    utils::write_file(central / "branches" / branch, commit_id);

    std::cout << "Pushed to central: " << branch
              << " → " << commit_id.substr(0, 8) << "\n";
    return true;
}

bool pull() {
    fs::path root = find_yag_root();
    fs::path yag = root / ".yag";
    fs::path central = get_central_path();

    // Safety: if central repo doesn't exist, nothing to pull
    if (!fs::exists(central)) {
        std::cerr << "No central repository found at "
                  << central.string() << "\n";
        return false;
    }

    std::string branch = get_current_branch();
    fs::path central_branch = central / "branches" / branch;

    // Safety: branch must exist in central
    if (!fs::exists(central_branch)) {
        std::cerr << "Branch '" << branch << "' not found in central.\n";
        return false;
    }

    // 1. Copy ONLY missing objects from central
    copy_missing(central / "objects", yag / "objects");

    // 2. Copy ONLY missing commits from central
    copy_missing(central / "commits", yag / "commits");

    // 3. Read both local and central branch pointers
    std::string central_id = utils::read_file(central_branch);
    std::string local_id  = utils::read_file(yag / "branches" / branch);

    // Already in sync — nothing to do
    if (local_id == central_id) {
        std::cout << "Already up to date.\n";
        return true;
    }

    // -----------------------------------------------------------------------
    // 4. Conflict detection
    //    First check if local is simply behind (fast-forward case):
    //    Walk the central commit's parent chain. If local_id appears as an
    //    ancestor → local is just behind, safe to fast-forward.
    //    Only flag a conflict when local has diverged (local has commits
    //    that central doesn't know about, AND file contents differ).
    // -----------------------------------------------------------------------
    if (local_id != "none" && central_id != "none") {
        // Check if local_id is an ancestor of central_id (fast-forward)
        bool is_ancestor = false;
        std::string walk_id = central_id;
        while (walk_id != "none") {
            if (walk_id == local_id) {
                is_ancestor = true;
                break;
            }
            Commit walk = read_commit(walk_id);
            walk_id = walk.parent;
        }

        if (!is_ancestor) {
            // Local has diverged — check for actual file conflicts
            Commit local_commit   = read_commit(local_id);
            Commit central_commit = read_commit(central_id);

            auto local_map   = file_hash_map(local_commit);
            auto central_map = file_hash_map(central_commit);

            if (local_map != central_map) {
                // Files differ AND histories have diverged → real conflict
                std::cerr << "Conflict detected. Manual resolution required.\n";

                // Show which files conflict
                for (const auto& [path, hash] : central_map) {
                    auto it = local_map.find(path);
                    if (it == local_map.end()) {
                        std::cerr << "  new in central: " << path << "\n";
                    } else if (it->second != hash) {
                        std::cerr << "  modified:       " << path << "\n";
                    }
                }
                for (const auto& [path, hash] : local_map) {
                    if (central_map.find(path) == central_map.end()) {
                        std::cerr << "  deleted in central: " << path << "\n";
                    }
                }

                // Objects/commits were already copied for manual inspection,
                // but we do NOT update the branch pointer or working directory.
                return false;
            }
            // Files are identical despite diverged histories — safe to fast-forward
        }
        // If is_ancestor == true, local is simply behind → fast-forward
    }

    // 5. Safe to fast-forward: update local branch pointer
    utils::write_file(yag / "branches" / branch, central_id);

    // 6. Restore working directory and index from the pulled commit
    if (central_id != "none") {
        Commit pulled = read_commit(central_id);
        restore_from_commit(root, pulled);
    }

    std::cout << "Pulled from central: " << branch
              << " → " << central_id.substr(0, 8) << "\n";
    return true;
}

} // namespace yag::core
