#include "core/branch.h"
#include "core/repo.h"
#include "core/commit.h"
#include "core/staging.h"
#include "utils/file_utils.h"

#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace yag::core {

bool create_branch(const std::string& name) {
    fs::path root = find_yag_root();
    fs::path branch_file = root / ".yag" / "branches" / name;

    if (fs::exists(branch_file)) {
        std::cerr << "Branch '" << name << "' already exists.\n";
        return false;
    }

    // New branch points to the same commit as the current branch
    std::string current = get_current_branch();
    std::string commit_id = utils::read_file(root / ".yag" / "branches" / current);
    utils::write_file(branch_file, commit_id);

    std::cout << "Created branch '" << name << "'";
    if (commit_id != "none") {
        std::cout << " at " << commit_id.substr(0, 8);
    }
    std::cout << "\n";
    return true;
}

bool checkout(const std::string& name) {
    fs::path root = find_yag_root();
    fs::path branch_file = root / ".yag" / "branches" / name;

    if (!fs::exists(branch_file)) {
        std::cerr << "Branch '" << name << "' does not exist.\n";
        return false;
    }

    // Update HEAD
    set_current_branch(name);

    // Restore working directory from the branch's latest commit
    std::string commit_id = utils::read_file(branch_file);
    if (commit_id != "none") {
        Commit c = read_commit(commit_id);

        // Remove current tracked files, then restore from snapshot
        auto current_files = utils::list_tracked_files(root);
        for (const auto& f : current_files) {
            fs::remove(root / f);
        }

        // Restore each file from objects/
        for (const auto& [rel_path, obj_hash] : c.files) {
            fs::path obj_path = root / ".yag" / "objects" / obj_hash;
            std::string content = utils::read_file(obj_path);
            utils::write_file(root / rel_path, content);
        }

        // Clean up empty directories left behind
        for (auto it = fs::recursive_directory_iterator(root); it != fs::recursive_directory_iterator(); ++it) {
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

        // Rebuild the index from the commit's file list
        write_index(c.files);
    } else {
        // No commits on this branch — clear the index
        write_index({});
    }

    std::cout << "Switched to branch '" << name << "'\n";
    return true;
}

void list_branches() {
    fs::path root = find_yag_root();
    fs::path branches_dir = root / ".yag" / "branches";
    std::string current = get_current_branch();

    for (const auto& entry : fs::directory_iterator(branches_dir)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();

        if (name == current) {
            std::cout << "* " << name << "\n";
        } else {
            std::cout << "  " << name << "\n";
        }
    }
}

} // namespace yag::core
