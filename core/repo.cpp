#include "core/repo.h"
#include "utils/file_utils.h"

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <vector>

namespace fs = std::filesystem;

namespace yag::core {

static const std::string YAG_DIR = ".yag";

// ---------------------------------------------------------------------------
// Helper: trim whitespace from both ends of a string
// Prevents subtle bugs when parsing config values with trailing \r\n or spaces
// ---------------------------------------------------------------------------
static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// ---------------------------------------------------------------------------
// Generic config get/set — operates on .yag/config (key=value format)
// ---------------------------------------------------------------------------

std::string get_config_value(const std::string& key,
                             const std::string& default_val) {
    fs::path root = find_yag_root();
    fs::path config_path = root / YAG_DIR / "config";

    if (!fs::exists(config_path)) {
        return default_val;
    }

    std::string content = utils::read_file(config_path);
    std::istringstream iss(content);
    std::string line;

    std::string prefix = key + "=";
    while (std::getline(iss, line)) {
        std::string trimmed_line = trim(line);
        if (trimmed_line.rfind(prefix, 0) == 0) {
            // Found the key — extract and trim the value
            return trim(trimmed_line.substr(prefix.size()));
        }
    }

    return default_val;
}

void set_config_value(const std::string& key, const std::string& value) {
    fs::path root = find_yag_root();
    fs::path config_path = root / YAG_DIR / "config";

    // Read existing config (if any)
    std::vector<std::string> lines;
    if (fs::exists(config_path)) {
        std::string content = utils::read_file(config_path);
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
    }

    // Replace existing key or append
    std::string prefix = key + "=";
    std::string new_line = key + "=" + value;
    bool found = false;

    for (auto& line : lines) {
        std::string trimmed = trim(line);
        if (trimmed.rfind(prefix, 0) == 0) {
            line = new_line;
            found = true;
            break;
        }
    }

    if (!found) {
        lines.push_back(new_line);
    }

    // Write back
    std::ostringstream oss;
    for (const auto& line : lines) {
        oss << line << "\n";
    }
    utils::write_file(config_path, oss.str());
}

// ---------------------------------------------------------------------------
// Remote accessors
// ---------------------------------------------------------------------------

std::string get_remote_host() {
    return get_config_value("remote_host");
}

std::string get_remote_user() {
    return get_config_value("remote_user");
}

int get_remote_port() {
    std::string port_str = get_config_value("remote_port", "22");
    try {
        return std::stoi(port_str);
    } catch (...) {
        return 22;
    }
}

std::string get_remote_base_path() {
    return get_config_value("remote_base_path", "~/yag-central/projects");
}

bool has_remote() {
    return !get_remote_host().empty() && !get_remote_user().empty();
}

// ---------------------------------------------------------------------------
// Parse "user@host[:port]" and store in config
// ---------------------------------------------------------------------------
void set_remote_spec(const std::string& spec) {
    if (spec.empty()) return;

    // Split on '@'
    auto at_pos = spec.find('@');
    if (at_pos == std::string::npos) {
        throw std::runtime_error(
            "Invalid remote spec: '" + spec + "'. Expected: user@host[:port]");
    }

    std::string user = spec.substr(0, at_pos);
    std::string host_port = spec.substr(at_pos + 1);

    // Split host_port on ':' for optional port
    std::string host;
    std::string port = "22";
    auto colon_pos = host_port.find(':');
    if (colon_pos != std::string::npos) {
        host = host_port.substr(0, colon_pos);
        port = host_port.substr(colon_pos + 1);
    } else {
        host = host_port;
    }

    if (user.empty() || host.empty()) {
        throw std::runtime_error(
            "Invalid remote spec: '" + spec + "'. User and host must be non-empty.");
    }

    set_config_value("remote_user", user);
    set_config_value("remote_host", host);
    set_config_value("remote_port", port);
}

// ---------------------------------------------------------------------------
// Repository initialization
// ---------------------------------------------------------------------------

bool init(const std::string& project_name, const std::string& remote_spec) {
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

    // --- Optional remote configuration ---
    if (!remote_spec.empty()) {
        set_remote_spec(remote_spec);
        std::cout << "Remote: " << remote_spec << "\n";
    }

    return true;
}

// ---------------------------------------------------------------------------
// Repository discovery and branch management
// ---------------------------------------------------------------------------

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
    std::string name = get_config_value("project_name");
    if (name.empty()) {
        throw std::runtime_error("project_name not found in .yag/config");
    }
    return name;
}

} // namespace yag::core
