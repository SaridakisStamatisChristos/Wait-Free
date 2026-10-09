#pragma once

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

#define VQ_CHECK(expr)                                                                            \
    do {                                                                                          \
        if (!(expr)) {                                                                            \
            throw std::runtime_error(std::string("check failed: ") + #expr + " at " + __FILE__ + \
                                     ":" + std::to_string(__LINE__));                             \
        }                                                                                         \
    } while (false)

namespace vqtest {

template <class Fn>
void run(std::string_view name, Fn&& fn) {
    try {
        fn();
        std::cout << "[PASS] " << name << '\n';
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << name << ": " << ex.what() << '\n';
        std::exit(1);
    } catch (...) {
        std::cerr << "[FAIL] " << name << ": unknown exception\n";
        std::exit(1);
    }
}

} // namespace vqtest
