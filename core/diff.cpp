#include "core/diff.h"
#include "core/repo.h"
#include "core/staging.h"
#include "utils/file_utils.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace yag::core {

// Split a string into lines
static std::vector<std::string> split_lines(const std::string &s) {
  std::vector<std::string> lines;
  std::istringstream iss(s);
  std::string line;
  while (std::getline(iss, line)) {
    lines.push_back(line);
  }
  return lines;
}

void show_diff() {
  fs::path root = find_yag_root();
  auto index = read_index();

  if (index.empty()) {
    std::cout << "Nothing staged. Use 'yag add' first.\n";
    return;
  }

  bool any_diff = false;

  for (const auto &[rel_path, obj_hash] : index) {
    fs::path full_path = root / rel_path;

    // File deleted from working dir
    if (!fs::exists(full_path)) {
      any_diff = true;
      std::cout << "--- a/" << rel_path << "\n";
      std::cout << "+++ /dev/null\n";
      auto old_lines =
          split_lines(utils::read_file(root / ".yag" / "objects" / obj_hash));
      for (const auto &l : old_lines) {
        std::cout << "- " << l << "\n";
      }
      std::cout << "\n";
      continue;
    }

    std::string staged = utils::read_file(root / ".yag" / "objects" / obj_hash);
    std::string working = utils::read_file(full_path);

    if (staged == working)
      continue;

    any_diff = true;
    std::cout << "--- a/" << rel_path << "  (staged)\n";
    std::cout << "+++ b/" << rel_path << "  (working)\n";

    auto lines_a = split_lines(staged);
    auto lines_b = split_lines(working);
    size_t max_n = std::max(lines_a.size(), lines_b.size());

    for (size_t i = 0; i < max_n; ++i) {
      bool has_a = i < lines_a.size();
      bool has_b = i < lines_b.size();

      if (has_a && has_b) {
        if (lines_a[i] != lines_b[i]) {
          std::cout << "- " << lines_a[i] << "\n";
          std::cout << "+ " << lines_b[i] << "\n";
        }
      } else if (has_a) {
        std::cout << "- " << lines_a[i] << "\n";
      } else {
        std::cout << "+ " << lines_b[i] << "\n";
      }
    }
    std::cout << "\n";
  }

  if (!any_diff) {
    std::cout << "No differences.\n";
  }
}

} // namespace yag::core
