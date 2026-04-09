#include "core/sync.h"
#include "core/commit.h"
#include "core/repo.h"
#include "core/scp_transport.h"
#include "core/staging.h"
#include "utils/file_utils.h"

#include <cstdlib>
#include <iostream>
#include <map>

namespace fs = std::filesystem;

namespace yag::core {

// ---------------------------------------------------------------------------
// Build the remote central repo path for this project.
// E.g.: ~/yag-central/projects/my_project
// ---------------------------------------------------------------------------
static std::string get_remote_project_path() {
  std::string base = get_remote_base_path();
  std::string project = get_project_name();
  return base + "/" + project;
}

// ---------------------------------------------------------------------------
// Build a file hash map from a commit's file list for easy comparison.
// ---------------------------------------------------------------------------
static std::map<std::string, std::string> file_hash_map(const Commit &c) {
  return std::map<std::string, std::string>(c.files.begin(), c.files.end());
}

// ---------------------------------------------------------------------------
// Restore working directory and index from a commit.
// Removes current tracked files, then writes files from the commit's snapshot.
// ---------------------------------------------------------------------------
static void restore_from_commit(const fs::path &root, const Commit &c) {
  fs::path yag = root / ".yag";

  // Remove current tracked files
  auto current_files = utils::list_tracked_files(root);
  for (const auto &f : current_files) {
    fs::remove(root / f);
  }

  // Restore each file from objects/
  for (const auto &[rel_path, obj_hash] : c.files) {
    fs::path obj_path = yag / "objects" / obj_hash;
    std::string content = utils::read_file(obj_path);
    utils::write_file(root / rel_path, content);
  }

  // Clean up empty directories left behind
  for (auto it = fs::recursive_directory_iterator(root);
       it != fs::recursive_directory_iterator(); ++it) {
    const auto &entry = *it;
    std::string fname = entry.path().filename().string();
    if (!fname.empty() && fname[0] == '.') {
      if (entry.is_directory())
        it.disable_recursion_pending();
      continue;
    }
    if (entry.is_directory() && fs::is_empty(entry.path())) {
      fs::remove(entry.path());
    }
  }

  // Update the index to match the commit
  write_index(c.files);
}

// ===========================================================================
//  PUSH — Upload local commits/objects to the central repo via SSH/SCP
//
//  Flow:
//    1. Read remote config (host, user, port, project_name)
//    2. Compare branch tip commits BEFORE any transfer
//    3. If identical → "Already up to date", zero transfer
//    4. Otherwise: ensure remote dirs, batch-upload missing objects + commits,
//       update remote branch pointer
// ===========================================================================

bool push() {
  fs::path root = find_yag_root();
  fs::path yag = root / ".yag";

  // --- Check remote is configured ---
  if (!has_remote()) {
    std::cerr
        << "No remote configured. Use 'yag remote set user@host[:port]'.\n";
    return false;
  }

  std::string host = get_remote_host();
  std::string user = get_remote_user();
  int port = get_remote_port();
  std::string remote_project = get_remote_project_path();

  // --- Read local branch tip ---
  std::string branch = get_current_branch();

  // --- Protected branches check ---
  if (branch == "main" && get_config_value("protect_main") == "true") {
    std::cerr << "Error: 'main' is a protected branch. Direct push is "
                 "forbidden.\n";
    return false;
  }

  std::string local_commit = utils::read_file(yag / "branches" / branch);

  if (local_commit == "none") {
    std::cerr << "Nothing to push (no commits on '" << branch << "').\n";
    return false;
  }

  // --- Step 1: Compare branch tips BEFORE any file transfer ---
  std::string remote_branch_path = remote_project + "/branches/" + branch;
  std::string remote_commit = "none";

  if (transport::ssh_file_exists(host, user, port, remote_branch_path)) {
    remote_commit =
        transport::ssh_read_file(host, user, port, remote_branch_path);
  }

  if (local_commit == remote_commit) {
    std::cout << "Already up to date.\n";
    return true;
  }

  // --- Step 2: Ensure remote directory structure exists ---
  transport::ssh_mkdir(host, user, port, remote_project + "/objects");
  transport::ssh_mkdir(host, user, port, remote_project + "/commits");
  transport::ssh_mkdir(host, user, port, remote_project + "/branches");

  // --- Step 3: Upload only missing objects (batch diff + SCP) ---
  transport::scp_upload_missing(yag / "objects", host, user, port,
                                remote_project + "/objects");

  // --- Step 4: Upload only missing commits ---
  transport::scp_upload_missing(yag / "commits", host, user, port,
                                remote_project + "/commits");

  // --- Step 5: Update remote branch pointer ---
  transport::ssh_write_file(host, user, port, remote_branch_path, local_commit);

  std::cout << "Pushed to central: " << branch << " → "
            << local_commit.substr(0, 8) << "\n";
  return true;
}

// ===========================================================================
//  PULL — Download new commits/objects from the central repo via SSH/SCP
//
//  Flow:
//    1. Read remote config
//    2. Compare branch tips BEFORE any transfer
//    3. If identical → "Already up to date", zero transfer
//    4. Conflict detection: walk remote commit ancestry via SSH
//    5. Batch-download missing objects + commits
//    6. Update local branch pointer
//    7. Restore working directory from pulled commit
// ===========================================================================

bool pull() {
  fs::path root = find_yag_root();
  fs::path yag = root / ".yag";

  // --- Check remote is configured ---
  if (!has_remote()) {
    std::cerr
        << "No remote configured. Use 'yag remote set user@host[:port]'.\n";
    return false;
  }

  std::string host = get_remote_host();
  std::string user = get_remote_user();
  int port = get_remote_port();
  std::string remote_project = get_remote_project_path();

  // --- Read local branch info ---
  std::string branch = get_current_branch();
  std::string local_id = utils::read_file(yag / "branches" / branch);

  // --- Step 1: Check remote branch exists ---
  std::string remote_branch_path = remote_project + "/branches/" + branch;

  if (!transport::ssh_file_exists(host, user, port, remote_branch_path)) {
    std::cerr << "Branch '" << branch << "' not found in central.\n";
    return false;
  }

  // --- Step 2: Compare branch tips BEFORE any transfer ---
  std::string central_id =
      transport::ssh_read_file(host, user, port, remote_branch_path);

  if (local_id == central_id) {
    std::cout << "Already up to date.\n";
    return true;
  }

  // -------------------------------------------------------------------
  // Step 3: Conflict detection
  //   Walk the central commit's parent chain via SSH.
  //   If local_id appears as an ancestor → safe to fast-forward.
  //   If not → check file hashes for actual conflicts.
  // -------------------------------------------------------------------
  if (local_id != "none" && central_id != "none") {
    // First, download the commits we need for ancestry walk
    // (we need them locally to read parent chains)
    transport::scp_download_missing(
        host, user, port, remote_project + "/commits", yag / "commits");

    // Also download objects (we'll need them for restore anyway,
    // and for conflict file-hash comparison)
    transport::scp_download_missing(
        host, user, port, remote_project + "/objects", yag / "objects");

    // Walk the central commit's parent chain to check if local is ancestor
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
      // Local has diverged — compare file hashes for real conflicts
      Commit local_commit = read_commit(local_id);
      Commit central_commit = read_commit(central_id);

      auto local_map = file_hash_map(local_commit);
      auto central_map = file_hash_map(central_commit);

      if (local_map != central_map) {
        // Files differ AND histories diverged → real conflict
        std::cerr << "Conflict detected. Manual resolution required.\n";

        // Show which files conflict
        for (const auto &[path, hash] : central_map) {
          auto it = local_map.find(path);
          if (it == local_map.end()) {
            std::cerr << "  new in central: " << path << "\n";
          } else if (it->second != hash) {
            std::cerr << "  modified:       " << path << "\n";
          }
        }
        for (const auto &[path, hash] : local_map) {
          if (central_map.find(path) == central_map.end()) {
            std::cerr << "  deleted in central: " << path << "\n";
          }
        }

        // --- AI Merge Hook ---
        std::cout << "Attempting AI-assisted merge resolution...\n";
        for (const auto &[path, hash] : central_map) {
          auto it = local_map.find(path);
          if (it != local_map.end() && it->second != hash) {
            // resolve_conflict_with_ai(path); // Future hook
            std::cout << "  [AI Placeholder] Analyzed conflict in: " << path
                      << "\n";
          }
        }

        // Objects/commits were already downloaded for inspection,
        // but we do NOT update branch pointer or working directory.
        return false;
      }
      // Files identical despite diverged history → safe to fast-forward
    }
    // is_ancestor == true → local is simply behind → fast-forward
  } else {
    // Simple case: local has no commits, or remote has no commits on this
    // branch Download all missing objects and commits
    transport::scp_download_missing(
        host, user, port, remote_project + "/commits", yag / "commits");

    transport::scp_download_missing(
        host, user, port, remote_project + "/objects", yag / "objects");
  }

  // --- Step 4: Update local branch pointer ---
  utils::write_file(yag / "branches" / branch, central_id);

  // --- Step 5: Restore working directory from the pulled commit ---
  if (central_id != "none") {
    Commit pulled = read_commit(central_id);
    restore_from_commit(root, pulled);
  }

  std::cout << "Pulled from central: " << branch << " → "
            << central_id.substr(0, 8) << "\n";
  return true;
}

} // namespace yag::core
