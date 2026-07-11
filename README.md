# YAG

A lightweight version control system written in C++17. YAG combines local version control with a central repository and uses **SSH/SCP** to sync repositories between machines. It works across Linux and Windows clients without requiring any server-side installation.

## Features

* Sync repositories over **SSH/SCP**.
* Works on Linux and Windows (using OpenSSH) with a Linux-based central repository.
* Retries failed transfers automatically (up to 3 times) and verifies every transfer using **SHA-256** hashes.
* Avoids unnecessary transfers by comparing branch heads before pushing or pulling.
* `yag doctor` checks repository integrity by validating commit history and object hashes.
* View line-by-line differences between the working directory and staging area with `yag diff`.
* Lock files to avoid conflicting pushes.
* Recover previous HEAD states using `yag reflog`.
* Remove unreachable objects and commits with `yag gc`.
* Optional protection for the `main` branch.
* Placeholder support for future AI-assisted merge conflict resolution.
* Deterministic hashing produces the same commit IDs across operating systems.
* Create, switch, and manage multiple branches.

## Commands

| Command                 | Description                                 |
| :---------------------- | :------------------------------------------ |
| `yag init [name]`       | Create a new repository                     |
| `yag add <file\|.>`     | Stage files                                 |
| `yag status`            | Show repository status                      |
| `yag diff`              | Compare working directory with staged files |
| `yag commit "msg"`      | Create a commit                             |
| `yag branch [name]`     | List or create branches                     |
| `yag checkout <name>`   | Switch branches                             |
| `yag log`               | Show commit history                         |
| `yag reflog`            | Show HEAD history                           |
| `yag doctor`            | Verify repository integrity                 |
| `yag lock <file>`       | Lock a file                                 |
| `yag unlock <file>`     | Unlock a file                               |
| `yag locks`             | List active locks                           |
| `yag gc`                | Remove unreachable objects                  |
| `yag push`              | Push changes to the remote repository       |
| `yag pull`              | Pull changes from the remote repository     |
| `yag remote set <spec>` | Configure the remote server                 |
| `yag remote show`       | Display the current remote configuration    |

---

## Building

### Linux

```bash
git clone <repo-url> yag
cd yag
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo cp yag /usr/local/bin/
```

### Windows (PowerShell)

```powershell
git clone <repo-url> yag
cd yag
mkdir build ; cd build
cmake ..
cmake --build . --config Release
```

Add `build\Release\yag.exe` to your PATH.

---

## Getting Started

Create a project and initialize a repository:

```bash
mkdir my-project && cd my-project
yag init my-project

echo "Hello, YAG!" > hello.txt
yag add .
yag commit "Initial commit"
```

---

## Setting up a Remote

YAG uses SSH key authentication. Make sure you can connect to your server without entering a password.

```bash
yag remote set user@yourserver.com
yag push
```

---

## Using the Repository on Another Machine

```bash
mkdir my-project && cd my-project
yag init my-project

yag remote set user@yourserver.com
yag pull
```

---

The central repository is stored in `~/yag-central/projects/<project_name>/` by default. No additional software needs to be installed on the server.
