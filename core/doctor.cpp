#include "core/doctor.h"
#include "core/commit.h"
#include "core/repo.h"
#include "utils/file_utils.h"
#include "utils/hash.h"

#include <filesystem>
#include <iostream>
#include <set>

namespace fs = std::filesystem;

namespace yag::core {

void run_doctor() {
  int issues = 0;
  std::cout << "Running repository health check...\n\n";

  // 1. Check repo is initialized
  fs::path root;
  try {
    root = find_yag_root();
    std::cout << "[OK] Repository found at " << root.string() << "\n";
  } catch (...) {
    std::cout << "[FAIL] Not a YAG repository.\n";
    return;
  }

  fs::path yag = root / ".yag";

  // 2. Check HEAD exists and points to a valid branch
  fs::path head_path = yag / "HEAD";
  if (!fs::exists(head_path)) {
    std::cout << "[FAIL] HEAD file missing.\n";
    issues++;
  } else {
    std::string branch = utils::read_file(head_path);
    std::cout << "[OK] HEAD points to: " << branch << "\n";
    fs::path branch_file = yag / "branches" / branch;
    if (!fs::exists(branch_file)) {
      std::cout << "[FAIL] Branch file missing for HEAD branch '" << branch
                << "'.\n";
      issues++;
    }
  }

  // 3. Validate all branch pointers
  fs::path branches_dir = yag / "branches";
  if (fs::exists(branches_dir)) {
    for (const auto &entry : fs::directory_iterator(branches_dir)) {
      if (!entry.is_regular_file())
        continue;
      std::string bname = entry.path().filename().string();
      std::string commit_id = utils::read_file(entry.path());
      if (commit_id == "none") {
        std::cout << "[OK] Branch '" << bname << "' — no commits yet.\n";
      } else if (!fs::exists(yag / "commits" / commit_id)) {
        std::cout << "[FAIL] Branch '" << bname << "' points to missing commit "
                  << commit_id.substr(0, 8) << ".\n";
        issues++;
      } else {
        std::cout << "[OK] Branch '" << bname << "' → "
                  << commit_id.substr(0, 8) << "\n";
      }
    }
  }

  // 4. Verify object hashes match contents
  fs::path objects_dir = yag / "objects";
  int obj_count = 0;
  int bad_objects = 0;
  if (fs::exists(objects_dir)) {
    for (const auto &entry : fs::directory_iterator(objects_dir)) {
      if (!entry.is_regular_file())
        continue;
      obj_count++;
      std::string expected = entry.path().filename().string();
      std::string actual = utils::hash_file(entry.path());
      if (expected != actual) {
        std::cout << "[FAIL] Object " << expected << " hash mismatch!\n";
        bad_objects++;
        issues++;
      }
    }
  }
  std::cout << "[OK] Checked " << obj_count << " objects, " << bad_objects
            << " corrupted.\n";

  // 5. Walk commit chains for integrity
  std::set<std::string> visited;
  if (fs::exists(branches_dir)) {
    for (const auto &entry : fs::directory_iterator(branches_dir)) {
      if (!entry.is_regular_file())
        continue;
      std::string cid = utils::read_file(entry.path());
      while (cid != "none" && !cid.empty() &&
             visited.find(cid) == visited.end()) {
        visited.insert(cid);
        if (!fs::exists(yag / "commits" / cid)) {
          std::cout << "[FAIL] Commit " << cid.substr(0, 8)
                    << " in chain is missing.\n";
          issues++;
          break;
        }
        Commit c = read_commit(cid);
        // Verify all files referenced by commit have objects
        for (const auto &[path, hash] : c.files) {
          if (!fs::exists(yag / "objects" / hash)) {
            std::cout << "[FAIL] Commit " << cid.substr(0, 8)
                      << " references missing object for " << path << "\n";
            issues++;
          }
        }
        cid = c.parent;
      }
    }
  }
  std::cout << "[OK] Walked " << visited.size() << " commits in history.\n";

  // Summary
  std::cout << "\n";
  if (issues == 0) {
    std::cout << "Repository is healthy. No issues found.\n";
  } else {
    std::cout << issues << " issue(s) found.\n";
  }
}

} // namespace yag::core
