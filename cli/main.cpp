#include <iostream>
#include <string>

#include "core/repo.h"
#include "core/commit.h"
#include "core/branch.h"
#include "core/sync.h"
#include "core/staging.h"

void print_usage() {
    std::cout << "YAG - Yet Another Git\n"
              << "Usage:\n"
              << "  yag init [name]          Initialize a new repository\n"
              << "  yag add <file|.>         Stage file(s) for commit\n"
              << "  yag status               Show working tree status\n"
              << "  yag commit \"message\"    Commit staged snapshot\n"
              << "  yag log                 Show commit history\n"
              << "  yag branch <name>       Create a new branch\n"
              << "  yag checkout <name>     Switch to a branch\n"
              << "  yag push                Push to central repository\n"
              << "  yag pull                Pull from central repository\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];

    // --- "init" is the only command that works without an existing repo ---
    if (command == "init") {
        std::string name = (argc >= 3) ? argv[2] : "";
        yag::core::init(name);
        return 0;
    }

    // --- All other commands require an initialized repo ---
    if (!yag::core::is_initialized()) {
        std::cerr << "Error: not a YAG repository (run 'yag init' first).\n";
        return 1;
    }

    if (command == "add") {
        if (argc < 3) {
            std::cerr << "Error: add requires a file path or '.' for all.\n";
            return 1;
        }
        std::string target = argv[2];
        if (target == ".") {
            yag::core::stage_all();
        } else {
            yag::core::stage_file(target);
        }
    }
    else if (command == "status") {
        yag::core::show_status();
    }
    else if (command == "commit") {
        if (argc < 3) {
            std::cerr << "Error: commit requires a message.\n";
            return 1;
        }
        yag::core::create_commit(argv[2]);
    }
    else if (command == "log") {
        yag::core::show_log();
    }
    else if (command == "branch") {
        if (argc < 3) {
            // No argument: list branches
            yag::core::list_branches();
        } else {
            yag::core::create_branch(argv[2]);
        }
    }
    else if (command == "checkout") {
        if (argc < 3) {
            std::cerr << "Error: checkout requires a branch name.\n";
            return 1;
        }
        yag::core::checkout(argv[2]);
    }
    else if (command == "push") {
        yag::core::push();
    }
    else if (command == "pull") {
        yag::core::pull();
    }
    else {
        std::cerr << "Unknown command: " << command << "\n";
        print_usage();
        return 1;
    }

    return 0;
}
