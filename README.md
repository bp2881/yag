# YAG — Yet Another Git

A lightweight, hybrid local + centralized version control system written in C++17. Syncs project history between machines over **SSH/SCP** — works across Linux↔Linux and Windows→Linux environments.

## Features

- **SSH/SCP sync** — Synchronize your project via pure SSH/SCP. No SMB or shared filesystems required.
- **Cross-platform** — Supports Linux and Windows (OpenSSH) clients with a Linux-based central repository.
- **Robust Transfers** — Automatic **retry logic** (3 attempts) and post-transfer **SHA-256 verification** ensure data integrity.
- **Idempotent push/pull** — Compares branch tips before any file transfer to save bandwidth.
- **Repository Doctor** — Deep health checks (`yag doctor`) to verify commit chains and object hashing.
- **Staging & Diff** — Line-by-line diff (`yag diff`) between working directory and staged index.
- **File Locking** — Prevent concurrent push conflicts with per-file locks (`yag lock`).
- **Reflog** — Local history of HEAD movements for recovery from accidental branch deletes or checkouts.
- **Garbage Collection** — Cleanup tool (`yag gc`) to remove orphaned objects and commits.
- **Protected Branches** — Optional protection for the `main` branch to prevent direct push modifications.
- **AI Merge Hook** — Placeholder for future AI-assisted conflict resolution.
- **Deterministic hashing** — Stable commit IDs regardless of filesystem order or operating system.
- **Branching** — Full support for creating, switching, and listing branches.

## Command Reference

| Command | Description |
| :--- | :--- |
| `yag init [name]` | Initialize a new repository |
| `yag add <file\|.>` | Stage file(s) for commit |
| `yag status` | Show working tree status |
| `yag diff` | Show changes between working dir and index |
| `yag commit "msg"` | Commit staged snapshot |
| `yag branch [name]` | List or create branches |
| `yag checkout <name>` | Switch to a branch |
| `yag log` | Show commit history |
| `yag reflog` | Show local HEAD movement history |
| `yag doctor` | Check repository health/integrity |
| `yag lock <file>` | Lock a file to prevent pushes |
| `yag unlock <file>` | Unlock a file |
| `yag locks` | Show all active file locks |
| `yag gc` | Clean unreachable objects/commits |
| `yag push` | Push to central repository (SSH) |
| `yag pull` | Pull from central repository (SSH) |
| `yag remote set <spec>` | Set remote server (user@host) |
| `yag remote show` | Show current remote config |

---

## Setup Instructions

### 1. Build from Source

#### Linux
```bash
git clone <repo-url> yag
cd yag
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo cp yag /usr/local/bin/
```

#### Windows (PowerShell)
```powershell
git clone <repo-url> yag
cd yag
mkdir build ; cd build
cmake ..
cmake --build . --config Release
```
*Add `build\Release\yag.exe` to your PATH.*

### 2. Initialize and Commit

```bash
# Create a new project
mkdir my-project && cd my-project

# Initialize YAG
yag init my-project

# Add and commit files
echo "Hello, YAG!" > hello.txt
yag add .
yag commit "Initial commit"
```

### 3. Remote Configuration

YAG requires **key-based SSH authentication**. Ensure you can SSH into your server without a password prompt.

```bash
# Connect to your remote server
yag remote set user@yourserver.com

# Push your history to the central repository
yag push
```

### 4. Syncing on Another Machine

```bash
# Initialize the project locally with the SAME name
mkdir my-project && cd my-project
yag init my-project

# Set the same remote and pull
yag remote set user@yourserver.com
yag pull
```

---

*The central repository is stored at `~/yag-central/projects/<project_name>/` on the server by default. No server-side YAG installation is required.*
