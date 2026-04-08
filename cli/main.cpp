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
              << "  yag init [name] [user@host[:port]]   Initialize a new repository\n"
              << "  yag add <file|.>                     Stage file(s) for commit\n"
              << "  yag status                           Show working tree status\n"
              << "  yag commit \"message\"                 Commit staged snapshot\n"
              << "  yag log                              Show commit history\n"
              << "  yag branch [name]                    List or create branches\n"
              << "  yag checkout <name>                  Switch to a branch\n"
              << "  yag push                             Push to central repository (SSH)\n"
              << "  yag pull                             Pull from central repository (SSH)\n"
              << "  yag remote set <user@host[:port]>    Set/change remote server\n"
              << "  yag remote show                      Show current remote config\n";
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
        std::string remote = (argc >= 4) ? argv[3] : "";
        yag::core::init(name, remote);
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
    else if (command == "remote") {
        // --- yag remote set <user@host[:port]> ---
        // --- yag remote show ---
        if (argc < 3) {
            std::cerr << "Usage:\n"
                      << "  yag remote set <user@host[:port]>\n"
                      << "  yag remote show\n";
            return 1;
        }

        std::string subcmd = argv[2];

        if (subcmd == "set") {
            if (argc < 4) {
                std::cerr << "Error: 'yag remote set' requires user@host[:port].\n";
                return 1;
            }
            try {
                yag::core::set_remote_spec(argv[3]);
                std::cout << "Remote set to: " << argv[3] << "\n";
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << "\n";
                return 1;
            }
        }
        else if (subcmd == "show") {
            if (yag::core::has_remote()) {
                std::cout << "Remote host: " << yag::core::get_remote_host() << "\n"
                          << "Remote user: " << yag::core::get_remote_user() << "\n"
                          << "Remote port: " << yag::core::get_remote_port() << "\n"
                          << "Base path:   " << yag::core::get_remote_base_path() << "\n";
            } else {
                std::cout << "No remote configured. Use 'yag remote set user@host[:port]'.\n";
            }
        }
        else {
            std::cerr << "Unknown remote subcommand: " << subcmd << "\n"
                      << "Usage: yag remote set|show\n";
            return 1;
        }
    }
    else {
        std::cerr << "Unknown command: " << command << "\n";
        print_usage();
        return 1;
    }

    return 0;
}
