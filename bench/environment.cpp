#include "environment.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

namespace {
std::string read_first_line(const char* path) {
    std::ifstream in(path);
    std::string s;
    std::getline(in, s);
    return s;
}

std::string escape(std::string s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
        case '"':
        case '\\':
            out.push_back('\\');
            out.push_back(ch);
            break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) >= 0x20U) {
                out.push_back(ch);
            }
            break;
        }
    }
    return out;
}
}

namespace vqbench {
std::string environment_json() {
    std::ostringstream out;
    out << "{\"hardware_threads\":" << std::thread::hardware_concurrency();
#if defined(__linux__)
    out << ",\"kernel\":\"" << escape(read_first_line("/proc/sys/kernel/osrelease")) << "\"";
    std::ifstream cpu("/proc/cpuinfo");
    std::string line;
    std::string model;
    while (std::getline(cpu, line)) {
        if (line.rfind("model name", 0) == 0) { model = line; break; }
    }
    out << ",\"cpu\":\"" << escape(model) << "\"";
#endif
#if defined(__clang__)
    out << ",\"compiler\":\"clang\",\"compiler_version\":\"" << __clang_version__ << "\"";
#elif defined(__GNUC__)
    out << ",\"compiler\":\"gcc\",\"compiler_version\":\"" << __VERSION__ << "\"";
#else
    out << ",\"compiler\":\"unknown\"";
#endif
    out << '}';
    return out.str();
}
} // namespace vqbench
