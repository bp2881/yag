#include <iostream>
#include <string>

std::string quote_remote_path(const std::string& path) {
    if (path.length() >= 2 && path[0] == '~' && path[1] == '/') {
        return "~/'" + path.substr(2) + "'";
    }
    if (path == "~") {
        return "~";
    }
    return "'" + path + "'";
}

int main() {
    std::cout << "Testing quote_remote_path:\n";
    std::cout << "  ~/foo          -> " << quote_remote_path("~/foo") << " (Expected: ~/'foo')\n";
    std::cout << "  ~/foo bar      -> " << quote_remote_path("~/foo bar") << " (Expected: ~/'foo bar')\n";
    std::cout << "  ~              -> " << quote_remote_path("~") << " (Expected: ~)\n";
    std::cout << "  /var/log       -> " << quote_remote_path("/var/log") << " (Expected: '/var/log')\n";
    std::cout << "  projects/yag   -> " << quote_remote_path("projects/yag") << " (Expected: 'projects/yag')\n";
    return 0;
}
