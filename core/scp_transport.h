#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <utility>

// ============================================================================
// Cross-platform SSH/SCP transport layer for YAG
//
// Every remote operation goes through this module. Internally uses
// popen/pclose (Linux) or _popen/_pclose (Windows) so callers never
// touch platform-specific process APIs.
//
// All functions throw std::runtime_error on non-zero exit codes.
// ============================================================================

namespace yag::core::transport {

// ---- Low-level shell execution --------------------------------------------

// Execute a shell command, capture stdout and exit code.
// Throws std::runtime_error if exit code != 0.
std::pair<std::string, int> exec_command(const std::string& cmd);

// Execute a shell command, discard stdout, return exit code.
// Does NOT throw on non-zero — caller decides what to do.
int run_command_quiet(const std::string& cmd);

// ---- SSH operations -------------------------------------------------------

// Read the full contents of a remote file via "ssh ... cat <path>"
std::string ssh_read_file(const std::string& host,
                          const std::string& user,
                          int port,
                          const std::string& remote_path);

// Check if a remote file exists (test -f). Returns true/false, does not throw.
bool ssh_file_exists(const std::string& host,
                     const std::string& user,
                     int port,
                     const std::string& remote_path);

// List filenames (basenames only) in a remote directory.
// Uses "find <dir> -maxdepth 1 -type f -printf '%f\n'" for safety.
// Returns empty vector if directory doesn't exist.
std::vector<std::string> ssh_list_dir(const std::string& host,
                                      const std::string& user,
                                      int port,
                                      const std::string& remote_dir);

// Create remote directories recursively (mkdir -p).
void ssh_mkdir(const std::string& host,
               const std::string& user,
               int port,
               const std::string& remote_dir);

// Write content to a remote file using base64 encoding for binary safety:
//   echo "<base64>" | ssh ... "base64 -d > <path>"
void ssh_write_file(const std::string& host,
                    const std::string& user,
                    int port,
                    const std::string& remote_path,
                    const std::string& content);

// ---- SCP file transfer ----------------------------------------------------

// Upload a single local file to a remote path.
void scp_upload(const std::filesystem::path& local_path,
                const std::string& host,
                const std::string& user,
                int port,
                const std::string& remote_path);

// Download a single remote file to a local path.
void scp_download(const std::string& host,
                  const std::string& user,
                  int port,
                  const std::string& remote_path,
                  const std::filesystem::path& local_path);

// ---- Batch transfer (upload/download only missing files) ------------------

// Upload files from local_dir that don't exist in remote_dir.
// Lists remote dir via SSH, diffs against local, SCP-uploads the delta.
void scp_upload_missing(const std::filesystem::path& local_dir,
                        const std::string& host,
                        const std::string& user,
                        int port,
                        const std::string& remote_dir);

// Download files from remote_dir that don't exist in local_dir.
// Lists remote dir via SSH, diffs against local, SCP-downloads the delta.
void scp_download_missing(const std::string& host,
                          const std::string& user,
                          int port,
                          const std::string& remote_dir,
                          const std::filesystem::path& local_dir);

} // namespace yag::core::transport
