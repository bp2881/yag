#include "core/lock.h"
#include "core/repo.h"
#include "utils/file_utils.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace yag::core {

// Lock files live in .yag/locks/ with the relative path as filename
// (slashes replaced with double underscores to flatten the namespace).
// Contents = username of whoever holds the lock.

static std::string path_to_lockname(const std::string &relpath) {
  std::string result = relpath;
  for (auto &c : result) {
    if (c == '/' || c == '\\')
      c = '_';
  }
  return result;
}

static fs::path get_locks_dir() {
  fs::path root = find_yag_root();
  fs::path locks = root / ".yag" / "locks";
  if (!fs::exists(locks))
    fs::create_directories(locks);
  return locks;
}

bool lock_file(const std::string &filepath) {
  fs::path root = find_yag_root();
  fs::path abs = fs::canonical(fs::absolute(filepath));
  std::string rel = fs::relative(abs, root).generic_string();

  fs::path lock_path = get_locks_dir() / path_to_lockname(rel);

  if (fs::exists(lock_path)) {
    std::string holder = utils::read_file(lock_path);
    std::cerr << "Error: '" << rel << "' is already locked by: " << holder
              << "\n";
    return false;
  }

  std::string user = get_config_value("remote_user", "local");
  utils::write_file(lock_path, user);
  std::cout << "Locked: " << rel << " (by " << user << ")\n";
  return true;
}

bool unlock_file(const std::string &filepath) {
  fs::path root = find_yag_root();
  fs::path abs = fs::canonical(fs::absolute(filepath));
  std::string rel = fs::relative(abs, root).generic_string();

  fs::path lock_path = get_locks_dir() / path_to_lockname(rel);

  if (!fs::exists(lock_path)) {
    std::cerr << "Error: '" << rel << "' is not locked.\n";
    return false;
  }

  fs::remove(lock_path);
  std::cout << "Unlocked: " << rel << "\n";
  return true;
}

bool is_locked(const std::string &filepath) {
  fs::path root = find_yag_root();
  std::string rel =
      fs::relative(fs::absolute(filepath), root).generic_string();
  fs::path lock_path = get_locks_dir() / path_to_lockname(rel);
  return fs::exists(lock_path);
}

void show_locks() {
  fs::path locks_dir = get_locks_dir();
  bool found = false;

  for (const auto &entry : fs::directory_iterator(locks_dir)) {
    if (!entry.is_regular_file())
      continue;
    found = true;
    std::string lockname = entry.path().filename().string();
    std::string holder = utils::read_file(entry.path());
    // Restore original path (approx — underscores were slashes)
    std::string display = lockname;
    for (auto &c : display) {
      if (c == '_')
        c = '/';
    }
    std::cout << "  " << display << "  (locked by " << holder << ")\n";
  }

  if (!found) {
    std::cout << "No files are currently locked.\n";
  }
}

} // namespace yag::core
