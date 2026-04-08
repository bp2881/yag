#include "core/repo.h"
#include "utils/file_utils.h"

#include <iostream>
#include <stdexcept>
#include <sstream>

namespace fs = std::filesystem;

namespace yag::core {

static const std::string YAG_DIR = ".yag";

bool init(const std::string& project_name) {
    fs::path yag_path = fs::current_path() / YAG_DIR;

    // Don't re-initialize an existing repo
    if (fs::exists(yag_path)) {
        std::cerr << "Repository already initialized in " << fs::current_path().string() << "\n";
        return false;
    }

    // Create directory structure
    fs::create_directories(yag_path / "objects");
    fs::create_directories(yag_path / "commits");
    fs::create_directories(yag_path / "branches");

    // HEAD defaults to "main"
    utils::write_file(yag_path / "HEAD", "main");

    // Create the default branch file (no commits yet)
    utils::write_file(yag_path / "branches" / "main", "none");

    // --- Write project config ---
    // Use provided name, or fall back to current folder name
    std::string name = project_name;
    if (name.empty()) {
        name = fs::current_path().filename().string();
    }
    utils::write_file(yag_path / "config", "project_name=" + name + "\n");

    std::cout << "Initialized empty YAG repository in "
              << yag_path.string() << "\n";
    std::cout << "Project name: " << name << "\n";
    return true;
}

fs::path find_yag_root() {
    fs::path current = fs::current_path();

    // Walk up the directory tree looking for .yag/
    while (true) {
        if (fs::exists(current / YAG_DIR) && fs::is_directory(current / YAG_DIR)) {
            return current;
        }

        fs::path parent = current.parent_path();
        if (parent == current) {
            // Reached filesystem root without finding .yag
            throw std::runtime_error(
                "Not a YAG repository (or any parent up to root)");
        }
        current = parent;
    }
}

bool is_initialized() {
    try {
        find_yag_root();
        return true;
    } catch (const std::runtime_error&) {
        return false;
    }
}

std::string get_current_branch() {
    fs::path root = find_yag_root();
    return utils::read_file(root / YAG_DIR / "HEAD");
}

void set_current_branch(const std::string& branch_name) {
    fs::path root = find_yag_root();
    utils::write_file(root / YAG_DIR / "HEAD", branch_name);
}

std::string get_project_name() {
    fs::path root = find_yag_root();
    fs::path config_path = root / YAG_DIR / "config";

    if (!fs::exists(config_path)) {
        throw std::runtime_error("Missing .yag/config — is this a valid YAG repository?");
    }

    // Parse simple key=value config
    std::string content = utils::read_file(config_path);
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.rfind("project_name=", 0) == 0) {
            return line.substr(std::string("project_name=").size());
        }
    }

    throw std::runtime_error("project_name not found in .yag/config");
}

} // namespace yag::core
