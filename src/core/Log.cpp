#include "orbduel/core/Log.hpp"

#include <cstdio>
#include <deque>

namespace orbduel {
namespace log {

namespace {
bool consoleEnabled = true;
std::deque<std::string> &buffer() {
    static std::deque<std::string> lines;
    return lines;
}

void push(const char *tag, const std::string &message) {
    if (consoleEnabled) {
        std::printf("[%s] %s\n", tag, message.c_str());
        std::fflush(stdout);
    }
    auto &lines = buffer();
    lines.push_back(std::string("[") + tag + "] " + message);
    while (lines.size() > 16) {
        lines.pop_front();
    }
}
} // namespace

void info(const std::string &message) {
    push("info", message);
}
void warn(const std::string &message) {
    push("warn", message);
}
void error(const std::string &message) {
    push("error", message);
}
void setConsole(bool enabled) {
    consoleEnabled = enabled;
}

void clear() {
    buffer().clear();
}

std::vector<std::string> recentLines(std::size_t lines) {
    std::vector<std::string> out;
    const auto &source = buffer();
    const std::size_t take = lines < source.size() ? lines : source.size();
    out.reserve(take);
    for (std::size_t i = source.size() - take; i < source.size(); ++i) {
        out.push_back(source[i]);
    }
    return out;
}

} // namespace log
} // namespace orbduel
