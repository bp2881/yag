#include "core/scp_transport.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

// ============================================================================
// Base64 encoder (self-contained, no external dependency)
// Used to safely pipe binary file content through SSH.
// ============================================================================
namespace {

static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const std::string &input) {
  std::string output;
  output.reserve(((input.size() + 2) / 3) * 4);

  size_t i = 0;
  const auto *data = reinterpret_cast<const unsigned char *>(input.data());
  size_t len = input.size();

  while (i < len) {
    uint32_t octet_a = i < len ? data[i++] : 0;
    uint32_t octet_b = i < len ? data[i++] : 0;
    uint32_t octet_c = i < len ? data[i++] : 0;

    uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

    output += BASE64_CHARS[(triple >> 18) & 0x3F];
    output += BASE64_CHARS[(triple >> 12) & 0x3F];
    output += (i > len + 1) ? '=' : BASE64_CHARS[(triple >> 6) & 0x3F];
    output += (i > len) ? '=' : BASE64_CHARS[triple & 0x3F];
  }

  return output;
}

// Trim trailing whitespace and newlines from a string
std::string trim_trailing(const std::string &s) {
  auto end = s.find_last_not_of(" \t\r\n");
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

// Correctly quote a remote path for the shell.
// If it starts with ~/, we leave the ~/ unquoted so the remote shell expands
// it.
std::string quote_remote_path(const std::string &path) {
  if (path.length() >= 2 && path[0] == '~' && path[1] == '/') {
    return "~/'" + path.substr(2) + "'";
  }
  if (path == "~") {
    return "~";
  }
  return "'" + path + "'";
}

} // anonymous namespace

namespace fs = std::filesystem;

namespace yag::core::transport {

// ============================================================================
// Cross-platform popen/pclose abstraction
// ============================================================================

std::pair<std::string, int> exec_command(const std::string &cmd) {
  // Open a pipe to the child process
#ifdef _WIN32
  FILE *pipe = _popen(cmd.c_str(), "r");
#else
  FILE *pipe = popen(cmd.c_str(), "r");
#endif

  if (!pipe) {
    throw std::runtime_error("Failed to execute command: " + cmd);
  }

  // Read all stdout from the child
  std::string output;
  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }

  // Close and get exit status
#ifdef _WIN32
  int raw_status = _pclose(pipe);
#else
  int raw_status = pclose(pipe);
#endif

  // On POSIX, pclose returns the wait status — extract the real exit code
#ifndef _WIN32
  int exit_code = WIFEXITED(raw_status) ? WEXITSTATUS(raw_status) : -1;
#else
  int exit_code = raw_status;
#endif

  return {output, exit_code};
}

int run_command_quiet(const std::string &cmd) {
  // Redirect stdout and stderr to /dev/null (or NUL on Windows)
#ifdef _WIN32
  std::string quiet_cmd = cmd + " > NUL 2>&1";
#else
  std::string quiet_cmd = cmd + " > /dev/null 2>&1";
#endif
  return std::system(quiet_cmd.c_str());
}

// ============================================================================
// Helper: build SSH command prefix
// ============================================================================
static std::string ssh_prefix(const std::string &host, const std::string &user,
                              int port) {
  // -o BatchMode=yes        → fail immediately if key auth fails (no prompt)
  // -o StrictHostKeyChecking=no → skip host key prompt (dev convenience)
  // -o ConnectTimeout=10    → don't hang forever
  std::ostringstream oss;
  oss << "ssh -o BatchMode=yes -o StrictHostKeyChecking=no "
      << "-o ConnectTimeout=10 -p " << port << " " << user << "@" << host;
  return oss.str();
}

// ============================================================================
// SSH operations
// ============================================================================

std::string ssh_read_file(const std::string &host, const std::string &user,
                          int port, const std::string &remote_path) {
  std::string cmd = ssh_prefix(host, user, port) + " \"cat " +
                    quote_remote_path(remote_path) + "\"";

  auto [output, exit_code] = exec_command(cmd);
  if (exit_code != 0) {
    throw std::runtime_error("ssh_read_file failed (exit " +
                             std::to_string(exit_code) + "): " + remote_path);
  }

  return trim_trailing(output);
}

bool ssh_file_exists(const std::string &host, const std::string &user, int port,
                     const std::string &remote_path) {
  std::string cmd = ssh_prefix(host, user, port) + " \"test -f " +
                    quote_remote_path(remote_path) + "\"";

  // run_command_quiet returns raw system() result
  // On POSIX: 0 if file exists, non-zero otherwise
  int rc = run_command_quiet(cmd);
  return (rc == 0);
}

std::vector<std::string> ssh_list_dir(const std::string &host,
                                      const std::string &user, int port,
                                      const std::string &remote_dir) {
  // Use find instead of ls — handles spaces/special chars in filenames.
  // -maxdepth 1: don't recurse into subdirectories
  // -type f: only regular files
  // -printf '%f\n': print basename only (one per line)
  std::string cmd = ssh_prefix(host, user, port) + " \"find " +
                    quote_remote_path(remote_dir) +
                    " -maxdepth 1 -type f -printf '%f\\n'\" 2>/dev/null";

  auto [output, exit_code] = exec_command(cmd);

  // If the directory doesn't exist, find returns non-zero — return empty
  if (exit_code != 0) {
    return {};
  }

  // Parse one filename per line
  std::vector<std::string> files;
  std::istringstream iss(output);
  std::string line;
  while (std::getline(iss, line)) {
    std::string trimmed = trim_trailing(line);
    if (!trimmed.empty()) {
      files.push_back(trimmed);
    }
  }

  return files;
}

void ssh_mkdir(const std::string &host, const std::string &user, int port,
               const std::string &remote_dir) {
  std::string cmd = ssh_prefix(host, user, port) + " \"mkdir -p " +
                    quote_remote_path(remote_dir) + "\"";

  auto [output, exit_code] = exec_command(cmd);
  if (exit_code != 0) {
    throw std::runtime_error("ssh_mkdir failed (exit " +
                             std::to_string(exit_code) + "): " + remote_dir);
  }
}

void ssh_write_file(const std::string &host, const std::string &user, int port,
                    const std::string &remote_path,
                    const std::string &content) {
  // Encode content as base64 for binary-safe transfer:
  //   echo "<base64>" | ssh ... "base64 -d > <path>"
  std::string encoded = base64_encode(content);

  std::string cmd = "echo '" + encoded + "' | " + ssh_prefix(host, user, port) +
                    " \"base64 -d > " + quote_remote_path(remote_path) + "\"";

  auto [output, exit_code] = exec_command(cmd);
  if (exit_code != 0) {
    throw std::runtime_error("ssh_write_file failed (exit " +
                             std::to_string(exit_code) + "): " + remote_path);
  }
}

// ============================================================================
// SCP file transfer
// ============================================================================

static std::string scp_prefix(int port) {
  std::ostringstream oss;
  oss << "scp -o BatchMode=yes -o StrictHostKeyChecking=no "
      << "-o ConnectTimeout=10 -P " << port;
  return oss.str();
}

void scp_upload(const fs::path &local_path, const std::string &host,
                const std::string &user, int port,
                const std::string &remote_path) {
  std::string cmd = scp_prefix(port) + " '" + local_path.string() + "'" + " " +
                    user + "@" + host + ":" + quote_remote_path(remote_path);

  auto [output, exit_code] = exec_command(cmd);
  if (exit_code != 0) {
    throw std::runtime_error("scp_upload failed (exit " +
                             std::to_string(exit_code) +
                             "): " + local_path.string() + " → " + remote_path);
  }
}

void scp_download(const std::string &host, const std::string &user, int port,
                  const std::string &remote_path, const fs::path &local_path) {
  // Ensure local parent directory exists
  if (local_path.has_parent_path()) {
    fs::create_directories(local_path.parent_path());
  }

  std::string cmd = scp_prefix(port) + " " + user + "@" + host + ":" +
                    quote_remote_path(remote_path) + " '" +
                    local_path.string() + "'";

  auto [output, exit_code] = exec_command(cmd);
  if (exit_code != 0) {
    throw std::runtime_error("scp_download failed (exit " +
                             std::to_string(exit_code) + "): " + remote_path +
                             " → " + local_path.string());
  }
}

// ============================================================================
// Batch transfer: upload/download only files that are missing on the other side
// ============================================================================

void scp_upload_missing(const fs::path &local_dir, const std::string &host,
                        const std::string &user, int port,
                        const std::string &remote_dir) {
  if (!fs::exists(local_dir))
    return;

  // 1. List what's already on the remote
  auto remote_files = ssh_list_dir(host, user, port, remote_dir);
  std::set<std::string> remote_set(remote_files.begin(), remote_files.end());

  // 2. Walk local directory, upload anything missing on remote
  for (const auto &entry : fs::directory_iterator(local_dir)) {
    if (!entry.is_regular_file())
      continue;
    std::string filename = entry.path().filename().string();

    if (remote_set.find(filename) == remote_set.end()) {
      std::string remote_path = remote_dir + "/" + filename;
      scp_upload(entry.path(), host, user, port, remote_path);
    }
  }
}

void scp_download_missing(const std::string &host, const std::string &user,
                          int port, const std::string &remote_dir,
                          const fs::path &local_dir) {
  fs::create_directories(local_dir);

  // 1. List what's on the remote
  auto remote_files = ssh_list_dir(host, user, port, remote_dir);

  // 2. Build set of local filenames
  std::set<std::string> local_set;
  if (fs::exists(local_dir)) {
    for (const auto &entry : fs::directory_iterator(local_dir)) {
      if (entry.is_regular_file()) {
        local_set.insert(entry.path().filename().string());
      }
    }
  }

  // 3. Download anything missing locally
  for (const auto &filename : remote_files) {
    if (local_set.find(filename) == local_set.end()) {
      std::string remote_path = remote_dir + "/" + filename;
      fs::path local_path = local_dir / filename;
      scp_download(host, user, port, remote_path, local_path);
    }
  }
}

} // namespace yag::core::transport
