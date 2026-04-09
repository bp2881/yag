#include "utils/file_utils.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace yag::utils {

std::string read_file(const fs::path &filepath) {
  std::ifstream ifs(filepath, std::ios::binary);
  if (!ifs) {
    throw std::runtime_error("Cannot open file: " + filepath.string());
  }
  std::ostringstream oss;
  oss << ifs.rdbuf();
  return oss.str();
}

void write_file(const fs::path &filepath, const std::string &content) {
  // Ensure parent directories exist
  if (filepath.has_parent_path()) {
    fs::create_directories(filepath.parent_path());
  }

  std::ofstream ofs(filepath, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    throw std::runtime_error("Cannot write file: " + filepath.string());
  }
  ofs << content;
}

std::vector<fs::path> list_tracked_files(const fs::path &root) {
  std::vector<fs::path> files;

  for (auto it = fs::recursive_directory_iterator(
           root, fs::directory_options::skip_permission_denied);
       it != fs::recursive_directory_iterator(); ++it) {
    const auto &entry = *it;
    std::string name = entry.path().filename().string();

    // Skip hidden files/directories (starting with '.')
    if (!name.empty() && name[0] == '.') {
      if (entry.is_directory()) {
        it.disable_recursion_pending(); // don't descend into .yag/, .git/, etc.
      }
      continue;
    }

    if (entry.is_regular_file()) {
      // Store as relative path from root
      files.push_back(fs::relative(entry.path(), root));
    }
  }

  // Sort for deterministic output
  std::sort(files.begin(), files.end());
  return files;
}

void copy_file_safe(const fs::path &src, const fs::path &dst) {
  // Ensure destination parent directories exist
  if (dst.has_parent_path()) {
    fs::create_directories(dst.parent_path());
  }

  fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
}

} // namespace yag::utils
