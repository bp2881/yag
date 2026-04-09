#include "core/gc.h"
#include "core/commit.h"
#include "core/repo.h"
#include "core/staging.h"
#include "utils/file_utils.h"

#include <filesystem>
#include <iostream>
#include <set>

namespace fs = std::filesystem;

namespace yag::core {

void run_gc() {
  fs::path root = find_yag_root();
  fs::path yag = root / ".yag";

  std::set<std::string> reachable_commits;
  std::set<std::string> reachable_objects;

  // 1. Walk all branches → collect reachable commits and their objects
  fs::path branches_dir = yag / "branches";
  if (fs::exists(branches_dir)) {
    for (const auto &entry : fs::directory_iterator(branches_dir)) {
      if (!entry.is_regular_file())
        continue;
      std::string cid = utils::read_file(entry.path());
      while (cid != "none" && !cid.empty() &&
             reachable_commits.find(cid) == reachable_commits.end()) {
        reachable_commits.insert(cid);
        try {
          Commit c = read_commit(cid);
          for (const auto &[path, hash] : c.files) {
            reachable_objects.insert(hash);
          }
          cid = c.parent;
        } catch (...) {
          break; // broken chain, stop walking
        }
      }
    }
  }

  // 2. Also mark objects referenced by the current index as reachable
  auto index = read_index();
  for (const auto &[path, hash] : index) {
    reachable_objects.insert(hash);
  }

  // 3. Delete unreachable commits
  int removed_commits = 0;
  fs::path commits_dir = yag / "commits";
  if (fs::exists(commits_dir)) {
    for (const auto &entry : fs::directory_iterator(commits_dir)) {
      if (!entry.is_regular_file())
        continue;
      std::string name = entry.path().filename().string();
      if (reachable_commits.find(name) == reachable_commits.end()) {
        fs::remove(entry.path());
        removed_commits++;
      }
    }
  }

  // 4. Delete unreachable objects
  int removed_objects = 0;
  fs::path objects_dir = yag / "objects";
  if (fs::exists(objects_dir)) {
    for (const auto &entry : fs::directory_iterator(objects_dir)) {
      if (!entry.is_regular_file())
        continue;
      std::string name = entry.path().filename().string();
      if (reachable_objects.find(name) == reachable_objects.end()) {
        fs::remove(entry.path());
        removed_objects++;
      }
    }
  }

  std::cout << "Garbage collection complete.\n"
            << "  Removed " << removed_commits << " unreachable commit(s).\n"
            << "  Removed " << removed_objects << " unreachable object(s).\n";
}

} // namespace yag::core
