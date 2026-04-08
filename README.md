# YAG — Yet Another Git

A lightweight, hybrid local + centralized version control system written in C++17. Syncs project history between machines over **SSH/SCP** — works across Linux↔Linux and Windows→Linux environments.

```
┌──────────────┐   SSH/SCP   ┌──────────────────────────────┐   SSH/SCP   ┌──────────────┐
│  Client A    │ ──────────► │  Central Repo (Linux Server)  │ ◄────────── │  Client B    │
│  Linux/Win   │             │  ~/yag-central/projects/...   │             │  Linux/Win   │
└──────────────┘             └──────────────────────────────┘             └──────────────┘
```

---

## Features

- **SSH/SCP sync** — No SMB, no Samba, no shared filesystems. Pure SSH.
- **Cross-platform** — Linux and Windows (OpenSSH on Win10+) clients, Linux server.
- **Idempotent push/pull** — Compares branch tips before any file transfer. No wasted bandwidth.
- **Deterministic hashing** — Files are sorted before hashing, so commit IDs are stable regardless of filesystem order.
- **Conflict detection** — Diverged histories with differing file contents are flagged, not silently overwritten.
- **Empty commit prevention** — Won't create a commit if nothing changed.
- **Branching** — Create, switch, and list branches.
- **Self-contained SHA-256** — No OpenSSL dependency.

---

## Table of Contents

1. [Building from Source](#building-from-source)
2. [Quick Start](#quick-start)
3. [SSH Setup](#ssh-setup)
4. [Command Reference](#command-reference)
5. [Configuration](#configuration)
6. [Multi-User Workflow](#multi-user-workflow)
7. [Running Tests](#running-tests)
8. [Architecture](#architecture)
9. [Troubleshooting](#troubleshooting)

---

## Building from Source

### Prerequisites

| Tool       | Minimum Version | Notes                          |
|------------|-----------------|--------------------------------|
| C++ compiler | C++17 support | GCC 8+, Clang 7+, MSVC 2019+  |
| CMake      | 3.16+           |                                |
| SSH client | Any             | OpenSSH (built-in on Linux, Win10+) |

### Linux

```bash
git clone <repo-url> yag
cd yag
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

The binary is at `build/yag`. Optionally install it:

```bash
sudo cp build/yag /usr/local/bin/
```

### Windows

```powershell
git clone <repo-url> yag
cd yag
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

The binary is at `build\Release\yag.exe`. Add it to your PATH.

---

## Quick Start

### 1. Initialize a project

```bash
mkdir my-project && cd my-project
yag init my-project
```

### 2. Add and commit files

```bash
echo "Hello, YAG!" > hello.txt
yag add .
yag commit "Initial commit"
```

### 3. Connect to a remote server

```bash
yag remote set youruser@yourserver.com
```

Or with a custom SSH port:

```bash
yag remote set youruser@yourserver.com:2222
```

### 4. Push to central

```bash
yag push
```

### 5. Pull on another machine

```bash
mkdir my-project && cd my-project
yag init my-project
yag remote set youruser@yourserver.com
yag pull
```

---

## SSH Setup

YAG uses `ssh` and `scp` commands under the hood. You **must** have key-based SSH auth configured — YAG runs in batch mode and will not prompt for passwords.

### Linux Client → Linux Server

```bash
# 1. Generate a key (if you don't have one)
ssh-keygen -t ed25519

# 2. Copy the key to your server
ssh-copy-id youruser@yourserver.com

# 3. Test the connection (should print "ok" with no password prompt)
ssh -o BatchMode=yes youruser@yourserver.com "echo ok"
```

### Windows Client → Linux Server

Windows 10+ comes with OpenSSH built-in.

```powershell
# 1. Generate a key (if you don't have one)
ssh-keygen -t ed25519

# 2. Copy the public key to the server manually
type $env:USERPROFILE\.ssh\id_ed25519.pub | ssh youruser@yourserver.com "cat >> ~/.ssh/authorized_keys"

# 3. Test the connection
ssh -o BatchMode=yes youruser@yourserver.com "echo ok"
```

### Server-Side Setup

The server needs:
- `sshd` running (usually default on any Linux server)
- The `base64` command available (standard on all Linux distros)
- `find` command available (standard coreutils)

No YAG installation is needed on the server — it's just a file store.

The central repo is stored at `~/yag-central/projects/<project_name>/` on the server. This directory is created automatically on first push.

---

## Command Reference

### Repository

| Command | Description |
|---------|-------------|
| `yag init [name] [user@host[:port]]` | Initialize a new repo. Uses folder name if `name` is omitted. |
| `yag status` | Show staged, unstaged, and untracked files. |

### Staging & Committing

| Command | Description |
|---------|-------------|
| `yag add <file>` | Stage a single file. |
| `yag add .` | Stage all modified/new tracked files. |
| `yag commit "message"` | Commit the staged snapshot. Blocked if nothing changed. |
| `yag log` | Show commit history (current branch, reverse chronological). |

### Branching

| Command | Description |
|---------|-------------|
| `yag branch` | List all branches (`*` marks current). |
| `yag branch <name>` | Create a new branch at the current commit. |
| `yag checkout <name>` | Switch to a branch (restores working directory). |

### Remote Sync (SSH/SCP)

| Command | Description |
|---------|-------------|
| `yag remote set <user@host[:port]>` | Set or change the remote server. |
| `yag remote show` | Display current remote configuration. |
| `yag push` | Push commits/objects to the central repo via SCP. |
| `yag pull` | Pull commits/objects from the central repo via SCP. |

---

## Configuration

All config is stored in `.yag/config` as simple `key=value` pairs:

```ini
project_name=my-project
remote_user=pranav
remote_host=192.168.1.100
remote_port=22
remote_base_path=~/yag-central/projects
```

| Key | Description | Default |
|-----|-------------|---------|
| `project_name` | Project identifier (all users must use the same name) | Folder name |
| `remote_host` | SSH server hostname or IP | *(none)* |
| `remote_user` | SSH username | *(none)* |
| `remote_port` | SSH port | `22` |
| `remote_base_path` | Where central repos live on the server | `~/yag-central/projects` |

> **Important:** The `project_name` is what links all users to the same central folder. If User A inits with `yag init coolproject` and User B inits with `yag init coolproject`, they'll sync to the same central directory — regardless of their local folder names.

---

## Multi-User Workflow

### Setup (each user, one time)

```bash
# User A (on their machine)
mkdir coolproject && cd coolproject
yag init coolproject
yag remote set alice@server.example.com

# User B (on their machine)
mkdir my-local-folder && cd my-local-folder
yag init coolproject                          # same project name!
yag remote set bob@server.example.com         # same server, different user
```

### Daily workflow

```bash
# Pull latest changes
yag pull

# Work...
echo "new feature" > feature.txt
yag add .
yag commit "Add feature"

# Push your changes
yag push
```

### Conflict handling

If both users modify the same file and push independently:

```
$ yag pull
Conflict detected. Manual resolution required.
  modified:       feature.txt
```

YAG downloads the remote commits/objects for inspection but does **not** overwrite your working directory. Resolve manually, then add + commit + push.

---

## Running Tests

The test script simulates two users syncing through SSH to `localhost`:

```bash
# 1. Ensure sshd is running
sudo systemctl start sshd

# 2. Set up key-based auth to yourself (one time)
ssh-keygen -t ed25519              # skip if you already have a key
ssh-copy-id localhost

# 3. Verify
ssh -o BatchMode=yes localhost "echo ok"

# 4. Run the tests
./test_central.sh ./build/yag
```

The tests cover:
- Init + remote configuration
- Add + commit + push via SCP
- Pull + working tree restore
- Idempotent push ("Already up to date")
- Empty commit blocking
- Conflict detection (diverged histories)
- Full commit log history

---

## Architecture

```
yag/
├── cli/
│   └── main.cpp              # CLI entry point, argument parsing
├── core/
│   ├── repo.cpp/.h           # Init, config, branch management
│   ├── commit.cpp/.h         # Commit creation, parsing, log
│   ├── branch.cpp/.h         # Branch create, checkout, list
│   ├── staging.cpp/.h        # Index (staging area), status
│   ├── sync.cpp/.h           # Push/pull logic (calls transport)
│   └── scp_transport.cpp/.h  # SSH/SCP wrapper (cross-platform)
├── utils/
│   ├── file_utils.cpp/.h     # File I/O, directory listing
│   └── hash.cpp/.h           # Self-contained SHA-256
├── CMakeLists.txt
├── test_central.sh
└── README.md
```

### Data flow

```
push():  local tip == remote tip?  ──yes──►  "Already up to date"
              │ no
              ▼
         ssh_mkdir (ensure remote dirs)
              │
         scp_upload_missing (objects/)
         scp_upload_missing (commits/)
              │
         ssh_write_file (branch pointer)
              │
              ▼
         "Pushed to central: main → abc12345"


pull():  remote tip == local tip?  ──yes──►  "Already up to date"
              │ no
              ▼
         scp_download_missing (commits/, objects/)
              │
         ancestry walk (is local an ancestor of remote?)
              │
        ┌─────┴─────┐
        │ yes        │ no
        ▼            ▼
   fast-forward   compare file hashes
        │            │
        │       ┌────┴────┐
        │       │ same    │ differ
        │       ▼         ▼
        │  fast-forward  "Conflict detected"
        │                 (abort, keep local)
        ▼
   update branch pointer
   restore working tree
```

---

## Troubleshooting

### "No remote configured"

```bash
yag remote set youruser@yourserver.com
```

### "ssh: connect to host ... Connection refused"

The SSH server isn't running on the remote:

```bash
# On the server:
sudo systemctl start sshd
sudo systemctl enable sshd   # to start on boot
```

### "Permission denied (publickey)"

SSH key auth isn't set up. See [SSH Setup](#ssh-setup).

### "Conflict detected. Manual resolution required."

Your local branch and the central branch have diverged with different file contents. YAG does **not** auto-merge. Steps:

1. Inspect what changed: `yag log`
2. Manually edit conflicting files
3. `yag add . && yag commit "Resolve conflict"`
4. `yag push`

### Push/pull hangs

Usually a SSH prompt waiting for input (password or host key confirmation). Check:

```bash
ssh -o BatchMode=yes -o StrictHostKeyChecking=no youruser@yourserver.com "echo ok"
```

If this hangs or prompts, fix your SSH config first.
