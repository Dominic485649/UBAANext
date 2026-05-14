#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace UBAANextCli {
namespace Console {

inline void append_format(std::ostringstream &out, const std::string &fmt, std::size_t pos) {
    out << fmt.substr(pos);
}

template <typename T, typename... Rest>
void append_format(std::ostringstream &out, const std::string &fmt, std::size_t pos, T &&value, Rest &&...rest) {
    const auto marker = fmt.find("{}", pos);
    if (marker == std::string::npos) {
        out << fmt.substr(pos);
        return;
    }
    out << fmt.substr(pos, marker - pos) << std::forward<T>(value);
    append_format(out, fmt, marker + 2, std::forward<Rest>(rest)...);
}

template <typename... Args>
[[nodiscard]] std::string format(const std::string &fmt, Args &&...args) {
    std::ostringstream out;
    append_format(out, fmt, 0, std::forward<Args>(args)...);
    return out.str();
}

inline void print(const std::string &text) {
    std::cout << text;
}

template <typename... Args>
void print(const std::string &fmt, Args &&...args) {
    std::cout << format(fmt, std::forward<Args>(args)...);
}

inline void println() {
    std::cout << '\n';
}

inline void println(const std::string &text) {
    std::cout << text << '\n';
}

template <typename... Args>
void println(const std::string &fmt, Args &&...args) {
    std::cout << format(fmt, std::forward<Args>(args)...) << '\n';
}

template <typename... Args>
void eprintln(const std::string &fmt, Args &&...args) {
    std::cerr << format(fmt, std::forward<Args>(args)...) << '\n';
}

} // namespace Console
} // namespace UBAANextCli
