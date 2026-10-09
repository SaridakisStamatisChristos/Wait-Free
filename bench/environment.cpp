#include "environment.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#if defined(__linux__)
#include <sys/utsname.h>
#endif

namespace {

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string read_first_line(const char* path) {
    std::ifstream in(path);
    std::string value;
    std::getline(in, value);
    return trim(value);
}

std::string read_status_value(std::string_view key) {
    std::ifstream in("/proc/self/status");
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind(key, 0) == 0) {
            const auto colon = line.find(':');
            if (colon != std::string::npos) return trim(line.substr(colon + 1));
        }
    }
    return {};
}

std::string read_cpuinfo_value(std::string_view key) {
    std::ifstream in("/proc/cpuinfo");
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind(key, 0) == 0) {
            const auto colon = line.find(':');
            if (colon != std::string::npos) return trim(line.substr(colon + 1));
        }
    }
    return {};
}

std::string read_command(const char* command) {
#if defined(__linux__)
    std::string output;
    if (FILE* pipe = popen(command, "r")) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) output += buffer;
        static_cast<void>(pclose(pipe));
    }
    return output;
#else
    (void)command;
    return {};
#endif
}

std::string lscpu_value(const std::string& text, std::string_view key) {
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        if (trim(line.substr(0, colon)) == key) return trim(line.substr(colon + 1));
    }
    return {};
}

std::string escape(std::string value) {
    std::string out;
    for (char ch : value) {
        switch (ch) {
        case '"':
        case '\\': out.push_back('\\'); out.push_back(ch); break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) >= 0x20U) out.push_back(ch);
            break;
        }
    }
    return out;
}

void emit_string(std::ostringstream& out, std::string_view key, const std::string& value) {
    out << ",\"" << key << "\":\"" << escape(value.empty() ? "unknown" : value) << "\"";
}

} // namespace

namespace vqbench {

std::string environment_json() {
    std::ostringstream out;
    out << "{\"hardware_threads\":" << std::thread::hardware_concurrency();
#if defined(__linux__)
    const std::string lscpu = read_command("lscpu 2>/dev/null");
    std::string kernel = read_first_line("/proc/sys/kernel/osrelease");
    std::string machine;
    struct utsname uts {};
    if (uname(&uts) == 0) {
        if (kernel.empty()) kernel = uts.release;
        machine = uts.machine;
    }

    const std::string reported_model = read_cpuinfo_value("model name");
    const std::string hardware = read_cpuinfo_value("Hardware");
    const std::string implementer = read_cpuinfo_value("CPU implementer");
    const std::string part = read_cpuinfo_value("CPU part");
    std::string model = lscpu_value(lscpu, "Model name");
    if (model.empty()) model = !reported_model.empty() ? reported_model : hardware;
    std::string vendor = lscpu_value(lscpu, "Vendor ID");
    if (vendor.empty()) vendor = implementer;

    emit_string(out, "kernel", kernel);
    emit_string(out, "architecture", lscpu_value(lscpu, "Architecture"));
    emit_string(out, "uname_machine", machine);
    emit_string(out, "cpu_vendor_or_implementer", vendor);
    emit_string(out, "cpu_model", model);
    emit_string(out, "reported_model_name", reported_model);
    emit_string(out, "cpu_implementer", implementer);
    emit_string(out, "cpu_part", part);
    emit_string(out, "logical_cpu_count", lscpu_value(lscpu, "CPU(s)"));
    emit_string(out, "sockets", lscpu_value(lscpu, "Socket(s)"));
    emit_string(out, "cores_per_socket", lscpu_value(lscpu, "Core(s) per socket"));
    emit_string(out, "threads_per_core", lscpu_value(lscpu, "Thread(s) per core"));
    emit_string(out, "numa_nodes", lscpu_value(lscpu, "NUMA node(s)"));
    emit_string(out, "l1d_cache", lscpu_value(lscpu, "L1d cache"));
    emit_string(out, "l1i_cache", lscpu_value(lscpu, "L1i cache"));
    emit_string(out, "l2_cache", lscpu_value(lscpu, "L2 cache"));
    emit_string(out, "l3_cache", lscpu_value(lscpu, "L3 cache"));
    emit_string(out, "cpu_affinity_set", read_status_value("Cpus_allowed_list"));
#endif
#if defined(__clang__)
    out << ",\"compiler\":\"clang\",\"compiler_version\":\"" << escape(__clang_version__) << "\"";
#elif defined(__GNUC__)
    out << ",\"compiler\":\"gcc\",\"compiler_version\":\"" << escape(__VERSION__) << "\"";
#else
    out << ",\"compiler\":\"unknown\"";
#endif
    out << '}';
    return out.str();
}

} // namespace vqbench
