#pragma once

#include <string>
#include <filesystem>

namespace yag::core {

// Initialize a new .yag/ repository in the current directory.
// If project_name is empty, uses the current folder name.
// If remote_spec is provided (user@host[:port]), stores SSH remote config.
bool init(const std::string& project_name = "",
          const std::string& remote_spec = "");

// Find the repository root by walking up from cwd
std::filesystem::path find_yag_root();

// Check if a .yag/ repository exists at or above cwd
bool is_initialized();

// Read the current branch name from .yag/HEAD
std::string get_current_branch();

// Write the current branch name to .yag/HEAD
void set_current_branch(const std::string& branch_name);

// Read the project name from .yag/config
std::string get_project_name();

// ---------------------------------------------------------------------------
// Generic config key=value access (with whitespace trimming)
// ---------------------------------------------------------------------------

// Read a value from .yag/config. Returns default_val if key not found.
std::string get_config_value(const std::string& key,
                             const std::string& default_val = "");

// Write (or overwrite) a key=value pair in .yag/config.
void set_config_value(const std::string& key, const std::string& value);

// ---------------------------------------------------------------------------
// Remote SSH config accessors (convenience wrappers)
// ---------------------------------------------------------------------------

std::string get_remote_host();             // "" if not configured
std::string get_remote_user();             // "" if not configured
int         get_remote_port();             // defaults to 22
std::string get_remote_base_path();        // defaults to ~/yag-central/projects

// Check if a remote is configured (host and user both non-empty)
bool has_remote();

// Parse and store "user@host[:port]" into config
void set_remote_spec(const std::string& spec);

} // namespace yag::core
