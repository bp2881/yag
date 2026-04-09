# YAG — Yet Another Git

A lightweight, hybrid local + centralized version control system written in C++17. Syncs project history between machines over **SSH/SCP** — works across Linux↔Linux and Windows→Linux environments.

## Features

- **SSH/SCP sync** — Synchronize your project via pure SSH/SCP. No SMB or shared filesystems required.
- **Cross-platform** — Supports Linux and Windows (OpenSSH) clients with a Linux-based central repository.
- **Idempotent push/pull** — Compares branch tips before any file transfer to save bandwidth.
- **Deterministic hashing** — Stable commit IDs regardless of filesystem order or operating system.
- **Conflict detection** — Flags diverged histories with differing file contents to prevent silent overwrites.
- **Empty commit prevention** — Prevents cluttering history with commits that contain no changes.
- **Branching** — Full support for creating, switching, and listing branches.
- **Self-contained** — Zero external dependencies for core logic (includes native SHA-256).

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
