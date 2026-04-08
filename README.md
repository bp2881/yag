# YAG — Yet Another Git

A minimal, file-based version control system written in C++17. YAG supports local repository management (init, staging, commit, log, branching) and centralized synchronization (push/pull) through the local filesystem.

Built for learning, experimentation, and as a foundation for more advanced features.

---

## Table of Contents

- [Features](#features)
- [Quick Start](#quick-start)
- [Installation](#installation)
- [Command Reference](#command-reference)
  - [yag init](#yag-init)
  - [yag add](#yag-add)
  - [yag status](#yag-status)
  - [yag commit](#yag-commit)
  - [yag log](#yag-log)
  - [yag branch](#yag-branch)
  - [yag checkout](#yag-checkout)
  - [yag push](#yag-push)
  - [yag pull](#yag-pull)
- [How It Works](#how-it-works)
  - [Repository Structure](#repository-structure)
  - [Content-Addressed Storage](#content-addressed-storage)
  - [Staging Area (Index)](#staging-area-index)
  - [Commit Format](#commit-format)
  - [Branching Model](#branching-model)
  - [Centralized Sync](#centralized-sync)
  - [Conflict Detection](#conflict-detection)
- [Centralized Sync Guide](#centralized-sync-guide)
  - [Single User Workflow](#single-user-workflow)
  - [Multi-User Collaboration](#multi-user-collaboration)
  - [Handling Conflicts](#handling-conflicts)
- [Testing](#testing)
- [Project Architecture](#project-architecture)
- [Limitations](#limitations)
- [License](#license)

---

## Features

- **Staging area** — `yag add` stages files before committing, just like `git add`
- **Content-addressed storage** — files are stored by their SHA-256 hash, deduplicating identical content
- **Empty commit prevention** — commits are blocked when nothing has changed
- **Branching** — create and switch between branches with full working directory restore
- **Commit history** — walk the parent chain to see full log
- **Centralized sync** — push/pull through `~/.yag-central/` to share between users
- **Conflict detection** — pull detects diverged histories and reports conflicting files
- **Incremental transfers** — push/pull only copies missing objects and commits
- **Project identity** — `.yag/config` stores the project name, used for central repo mapping
- **Safety checks** — all commands validate repo state before executing

---

## Quick Start

```bash
# Build
mkdir -p build && cd build
cmake .. && make
cd ..

# Create a repository
mkdir my-project && cd my-project
../build/yag init my-project

# Create some files
echo "Hello, World!" > hello.txt
echo "int main() { return 0; }" > main.cpp

# Stage and commit
../build/yag add .
../build/yag commit "initial commit"

# View history
../build/yag log

# Push to central
../build/yag push
```

---

## Installation

### Requirements

- C++17 compatible compiler (GCC 8+, Clang 7+)
- CMake 3.16+
- Linux (uses `gmtime_r` and `$HOME`)

### Build

```bash
git clone <repo-url> yag
cd yag
mkdir -p build && cd build
cmake ..
make
```

The binary is built at `build/yag`. You can optionally copy it to your PATH:

```bash
sudo cp build/yag /usr/local/bin/yag
```

---

## Command Reference

### `yag init`

Initialize a new YAG repository in the current directory.

```bash
yag init              # uses current folder name as project name
yag init my-project   # uses "my-project" as project name
```

**What it creates:**
```
.yag/
├── HEAD              # current branch name ("main")
├── config            # project_name=<name>
├── branches/
│   └── main          # points to "none" (no commits yet)
├── commits/          # commit files stored here
└── objects/          # file content stored here (by SHA-256 hash)
```

**Project name** is critical for centralized sync — it determines the central repo path at `~/.yag-central/projects/<project_name>/`. Multiple users must use the **same project name** to share a repo.

---

### `yag add`

Stage files for the next commit.

```bash
yag add file.txt      # stage a single file
yag add src/main.cpp  # stage a file in a subdirectory
yag add .             # stage ALL modified/new files
```

**What happens:**
1. The file's content is hashed (SHA-256)
2. The content is stored in `.yag/objects/<hash>` (if not already there)
3. The file's entry in `.yag/index` is updated with the new hash

**Rules:**
- Hidden files (starting with `.`) are skipped
- The `.yag/` directory itself is always skipped
- `yag add .` only reports files that actually changed since last staging

---

### `yag status`

Show the state of the working directory and staging area.

```bash
yag status
```

**Output sections:**

```
On branch main

Changes staged for commit:
  new file:   hello.txt        # in index, not in last commit
  modified:   main.cpp         # in index with different hash than last commit

Changes not staged for commit:
  modified:   utils.cpp        # working dir differs from index

Untracked files:
  notes.txt                    # not in index, not in any commit
```

If everything is clean:
```
On branch main

Nothing to commit, working tree clean.
```

**Three-way comparison:**
| | HEAD (last commit) | Index (staged) | Working Directory |
|---|---|---|---|
| Staged change | hash A | hash B | hash B |
| Unstaged change | hash A | hash A | hash B |
| Untracked | — | — | exists |

---

### `yag commit`

Create a commit from the currently staged files.

```bash
yag commit "add login feature"
```

**Output:**
```
[main a1b2c3d4] add login feature
```

**What happens:**
1. Reads the index (`.yag/index`) as the file snapshot
2. Compares against the parent commit's file list
3. If identical → `"No changes to commit."` (blocked)
4. If different → creates commit file, updates branch pointer

**Blocked when:**
- Nothing is staged: `"Nothing staged. Use 'yag add' to stage files."`
- Index matches parent: `"No changes to commit."`

---

### `yag log`

Display commit history for the current branch, newest first.

```bash
yag log
```

**Output:**
```
commit a1b2c3d4e5f6...
Branch: main
Date:   2026-04-08T12:00:00Z

    add login feature

commit 9f8e7d6c5b4a...
Branch: main
Date:   2026-04-08T11:00:00Z

    initial commit
```

The log walks the parent chain from the current branch's latest commit until it reaches `"none"`.

---

### `yag branch`

Create a new branch or list existing branches.

```bash
yag branch              # list all branches
yag branch feature-x    # create new branch "feature-x"
```

**List output:**
```
* main
  feature-x
```

The `*` marks the current branch. A new branch points to the same commit as the current branch.

---

### `yag checkout`

Switch to an existing branch.

```bash
yag checkout feature-x
```

**What happens:**
1. Updates `.yag/HEAD` to the new branch name
2. Reads the branch's latest commit
3. Removes current tracked files from the working directory
4. Restores all files from the commit's snapshot (via objects)
5. Rebuilds the index to match the commit

> ⚠️ **Warning:** Checkout replaces your working directory. Make sure to commit or stage your work before switching branches.

---

### `yag push`

Push local commits and objects to the central repository.

```bash
yag push
```

**What happens:**
1. Reads the project name from `.yag/config`
2. Resolves central path: `~/.yag-central/projects/<project_name>/`
3. Creates the central directory structure if it doesn't exist
4. Copies **only missing** objects (content-addressed, so duplicates are skipped)
5. Copies **only missing** commits
6. Updates the central branch pointer

**Output:**
```
Pushed to central: main → a1b2c3d4
```

---

### `yag pull`

Pull new commits and objects from the central repository.

```bash
yag pull
```

**Scenarios:**

| Situation | What Happens |
|-----------|-------------|
| Already in sync | `"Already up to date."` |
| Local is behind central | Fast-forward: files restored, index rebuilt |
| Local has diverged | `"Conflict detected. Manual resolution required."` |
| No central repo exists | Error message |

**Fast-forward pull:**
```
Pulled from central: main → b2c3d4e5
```

**Conflict:**
```
Conflict detected. Manual resolution required.
  modified:       readme.txt
  new in central: config.yaml
```

---

## How It Works

### Repository Structure

```
your-project/
├── .yag/
│   ├── HEAD                 # current branch name (e.g., "main")
│   ├── config               # project_name=your-project
│   ├── index                # staging area: "path hash" per line
│   ├── branches/
│   │   ├── main             # latest commit ID for this branch
│   │   └── feature-x        # latest commit ID for this branch
│   ├── commits/
│   │   ├── a1b2c3d4...      # commit file (named by its hash)
│   │   └── 9f8e7d6c...
│   └── objects/
│       ├── e3b0c442...      # file content (named by SHA-256 hash)
│       └── 5d41402a...
├── hello.txt
└── main.cpp
```

### Content-Addressed Storage

Every file's content is hashed with SHA-256. The hash becomes the filename in `.yag/objects/`. This means:

- **Identical content is stored once** — if two files have the same content, only one object exists
- **Integrity is guaranteed** — if the content changes, the hash changes
- **Push/pull is efficient** — objects are skipped if they already exist at the destination

```
echo "hello" > file.txt
# SHA-256 of "hello\n" → 5891b5b522...
# Stored at: .yag/objects/5891b5b522...
```

### Staging Area (Index)

The index file (`.yag/index`) is a simple text file:

```
file1.txt a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2
src/main.cpp 9f8e7d6c5b4a9f8e7d6c5b4a9f8e7d6c5b4a9f8e7d6c5b4a9f8e7d6c5b4a9f8e
```

Each line is `<relative_path> <sha256_hash>`, sorted alphabetically. This represents the exact snapshot that will go into the next commit.

**Workflow:**
```
Working Dir  →  yag add  →  Index  →  yag commit  →  Commit
   (files)       (hash)     (.yag/index)  (snapshot)  (.yag/commits/)
```

### Commit Format

Each commit is a plain text file stored in `.yag/commits/<commit_id>`:

```
id: a1b2c3d4e5f6...
parent: 9f8e7d6c5b4a...
branch: main
timestamp: 2026-04-08T12:00:00Z
message: add login feature
files:
  hello.txt e3b0c44298fc...
  src/main.cpp 5d41402abc4b...
```

- **id** — SHA-256 hash of the metadata below it (everything except the `id:` line)
- **parent** — previous commit ID, or `"none"` for the first commit
- **branch** — which branch this was committed on
- **files** — sorted list of `path hash` pairs (the full snapshot)

### Branching Model

- **HEAD** (`.yag/HEAD`) stores the current branch name
- **Branch files** (`.yag/branches/<name>`) store the latest commit ID
- Creating a branch copies the current branch's commit pointer
- Checkout restores the working directory from the branch's latest commit

```
main:      A → B → C        (HEAD)
                 ↘
feature-x:       B → D
```

### Centralized Sync

YAG uses a shared directory (`~/.yag-central/`) for centralized collaboration:

```
~/.yag-central/
└── projects/
    └── my-project/          # determined by project_name in .yag/config
        ├── branches/
        │   └── main         # latest pushed commit ID
        ├── commits/
        │   └── a1b2c3d4...
        └── objects/
            └── e3b0c442...
```

**Push** copies missing objects/commits from local → central and updates the central branch pointer.

**Pull** copies missing objects/commits from central → local, checks for conflicts, and restores files if safe.

### Conflict Detection

When pulling, YAG checks if your local history has **diverged** from central:

1. Walk central's commit parent chain
2. If local HEAD appears as an ancestor → **fast-forward** (you're just behind)
3. If local HEAD is NOT an ancestor → histories have diverged:
   - Compare file hash maps between local and central tips
   - If files differ → **conflict** (block the pull)
   - If files are identical → safe to fast-forward anyway

```
           Central
             ↓
A → B → C → D        ← local is at C, central is at D
                        C is an ancestor of D → fast-forward ✓

A → B → C             ← local (committed E on top of C)
         ↘
          D → E        ← central pushed D
                        C is NOT an ancestor of E → check files → conflict ✗
```

---

## Centralized Sync Guide

### Single User Workflow

Sync your project across multiple machines using the same `$HOME`:

```bash
# Machine A
mkdir project && cd project
yag init my-project
echo "hello" > file.txt
yag add .
yag commit "first commit"
yag push

# Machine B (same $HOME, e.g., shared NFS/home)
mkdir project && cd project
yag init my-project        # SAME project name
yag pull                   # gets file.txt
```

### Multi-User Collaboration

For multiple users on the same machine (or sharing `~/.yag-central/` via NFS/shared mount):

**User A:**
```bash
mkdir ~/work/project && cd ~/work/project
yag init team-project
echo "# Team Project" > README.md
yag add .
yag commit "initial setup"
yag push
```

**User B:**
```bash
mkdir ~/work/project && cd ~/work/project
yag init team-project      # MUST match User A's project name
yag pull                   # downloads README.md
cat README.md              # → "# Team Project"

# Make changes
echo "## Contributing" >> README.md
yag add .
yag commit "add contributing section"
yag push
```

**User A (getting B's changes):**
```bash
yag pull                   # fast-forwards to B's commit
cat README.md              # → includes "## Contributing"
```

> **Key rule:** Both users must `yag init` with the **same project name**. This maps them to the same central directory at `~/.yag-central/projects/<name>/`.

### Handling Conflicts

When two users commit different changes to the same file without pulling first:

```
User A: committed "version A" to readme.txt
User B: committed "version B" to readme.txt, then pushed

User A tries to pull:
→ Conflict detected. Manual resolution required.
    modified:       readme.txt
```

**To resolve:**

1. Note which files conflict from the error output
2. Manually compare your version with central's:
   ```bash
   # Your current file
   cat readme.txt

   # Central's version: find the commit, then the object hash
   # (look in .yag/commits/ for the central commit)
   ```
3. Edit the file to the desired state
4. Stage, commit, and push:
   ```bash
   yag add readme.txt
   yag commit "resolve conflict in readme"
   yag push
   ```

---

## Testing

### Automated Test Script

A full integration test is included that simulates two users:

```bash
# Run from the project root
bash test_central.sh ./build/yag
```

**What it tests:**
- Init with project name
- Push from User A
- Pull to User B (files arrive correctly)
- Modify + push from User B
- Fast-forward pull for User A
- Status shows clean after pull
- Empty commit prevention
- Conflict detection on diverged histories
- Log shows complete history

### Manual Smoke Test

```bash
# Create a test workspace
mkdir /tmp/yag-test && cd /tmp/yag-test

# Init
yag init test

# Create files
echo "hello" > a.txt
echo "world" > b.txt

# Check status (should show untracked)
yag status

# Stage and commit
yag add .
yag status          # should show "staged for commit"
yag commit "first"

# Try empty commit (should be blocked)
yag commit "nope"   # → "No changes to commit."

# Modify and check status
echo "updated" > a.txt
yag status          # → "not staged: modified a.txt"

# Stage, commit, push
yag add a.txt
yag commit "update a"
yag push

# Branch workflow
yag branch feature
yag checkout feature
echo "new file" > c.txt
yag add .
yag commit "add c.txt"
yag checkout main   # c.txt disappears
yag checkout feature # c.txt reappears

# Cleanup
rm -rf /tmp/yag-test ~/.yag-central/projects/test
```

---

## Project Architecture

```
yag/
├── CMakeLists.txt           # build configuration
├── README.md                # this file
├── test_central.sh          # automated integration test
├── cli/
│   └── main.cpp             # CLI entry point + argument parsing
├── core/
│   ├── repo.h / repo.cpp    # init, find root, HEAD, config
│   ├── staging.h / .cpp     # index read/write, add, status
│   ├── commit.h / .cpp      # create commit, read commit, log
│   ├── branch.h / .cpp      # create branch, checkout, list
│   └── sync.h / .cpp        # push, pull, conflict detection
└── utils/
    ├── file_utils.h / .cpp  # read/write files, list tracked files
    └── hash.h / .cpp        # SHA-256 implementation (no dependencies)
```

**Key design decisions:**
- **No external dependencies** — SHA-256 is implemented from scratch (FIPS 180-4)
- **Pure C++17** — uses `<filesystem>` for all path operations
- **Full snapshots** — every commit stores the complete file list (not diffs)
- **Text-based storage** — all metadata is human-readable plain text
- **Sorted determinism** — file lists are always sorted for reproducible hashes

---

## Limitations

YAG is intentionally minimal. It does **not** support:

- **Merge** — conflicts are detected but not auto-resolved
- **Rebase** — no history rewriting
- **Diff** — no line-by-line diff output (only hash comparison)
- **Remote networking** — sync is file-based only (`~/.yag-central/`)
- **Partial staging** — `yag add` stages the entire file, not hunks
- **Ignore files** — no `.yagignore` (all non-hidden files are tracked)
- **Tags** — no named references to specific commits
- **Stash** — no temporary storage for uncommitted changes

These are all potential future extensions.

---

## License

MIT
