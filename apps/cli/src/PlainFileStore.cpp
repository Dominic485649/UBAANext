#include "PlainFileStore.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace UBAANextCli {

namespace {

constexpr const char *kStoreV2Prefix = "UBAANext-PlainStore-v2\n";

std::string escape_value(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '\t': escaped += "\\t"; break;
        case '\r': escaped += "\\r"; break;
        case '\n': escaped += "\\n"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

std::string unescape_value(const std::string &value) {
    std::string unescaped;
    unescaped.reserve(value.size());
    bool escaping = false;
    for (char ch : value) {
        if (!escaping) {
            if (ch == '\\') {
                escaping = true;
            } else {
                unescaped += ch;
            }
            continue;
        }
        switch (ch) {
        case '\\': unescaped += '\\'; break;
        case 't': unescaped += '\t'; break;
        case 'r': unescaped += '\r'; break;
        case 'n': unescaped += '\n'; break;
        default:
            unescaped += '\\';
            unescaped += ch;
            break;
        }
        escaping = false;
    }
    if (escaping) {
        unescaped += '\\';
    }
    return unescaped;
}

std::string serialize_data(const std::unordered_map<std::string, std::string> &data) {
    std::ostringstream out;
    out << kStoreV2Prefix;
    for (const auto &[key, value] : data) {
        out << escape_value(key) << '\t' << escape_value(value) << '\n';
    }
    return out.str();
}

std::unordered_map<std::string, std::string> parse_data(const std::string &text) {
    std::unordered_map<std::string, std::string> data;
    const bool is_v2 = text.rfind(kStoreV2Prefix, 0) == 0;
    std::istringstream lines(is_v2 ? text.substr(std::string(kStoreV2Prefix).size()) : text);
    std::string line;
    while (std::getline(lines, line)) {
        auto tab_pos = line.find('\t');
        if (tab_pos == std::string::npos) continue;
        std::string key = line.substr(0, tab_pos);
        std::string value = line.substr(tab_pos + 1);
        if (!value.empty() && value.back() == '\r') value.pop_back();
        if (is_v2) {
            key = unescape_value(key);
            value = unescape_value(value);
        }
        data[std::move(key)] = std::move(value);
    }
    return data;
}

} // namespace

PlainFileStore::PlainFileStore(std::filesystem::path file_path) : m_file_path(std::move(file_path)) {
    load_from_file();
}

PlainFileStore::~PlainFileStore() {
    save_to_file();
}

void PlainFileStore::set_string(const std::string &key, const std::string &value) {
    m_data[key] = value;
}

std::optional<std::string> PlainFileStore::get_string(const std::string &key) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it->second;
    }
    return std::nullopt;
}

void PlainFileStore::remove(const std::string &key) {
    m_data.erase(key);
}

UBAANext::Result<void> PlainFileStore::flush() {
    save_to_file();
    return {};
}

void PlainFileStore::clear() {
    m_data.clear();
}

void PlainFileStore::load_from_file() {
    if (!std::filesystem::exists(m_file_path)) return;
    std::ifstream file(m_file_path, std::ios::binary);
    if (!file.is_open()) return;

    std::string raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (raw.empty()) return;
    m_data = parse_data(raw);
}

void PlainFileStore::save_to_file() const {
    auto parent = m_file_path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::filesystem::create_directories(parent);
    }

    auto raw = serialize_data(m_data);
    std::ofstream file(m_file_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return;
    file.write(raw.data(), static_cast<std::streamsize>(raw.size()));
}

} // namespace UBAANextCli
