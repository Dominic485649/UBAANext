#include "PlainFileStore.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

#if defined(_WIN32)
#include <windows.h>
#include <dpapi.h>
#endif

namespace UBAANextCli {

namespace {

std::string serialize_data(const std::unordered_map<std::string, std::string> &data) {
    std::ostringstream out;
    for (const auto &[key, value] : data) {
        out << key << '\t' << value << '\n';
    }
    return out.str();
}

std::unordered_map<std::string, std::string> parse_data(const std::string &text) {
    std::unordered_map<std::string, std::string> data;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        auto tab_pos = line.find('\t');
        if (tab_pos == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, tab_pos);
        std::string value = line.substr(tab_pos + 1);
        if (!value.empty() && value.back() == '\r') {
            value.pop_back();
        }
        data[std::move(key)] = std::move(value);
    }
    return data;
}

#if defined(_WIN32)
std::string protect_bytes(const std::string &plain) {
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plain.data()));
    in.cbData = static_cast<DWORD>(plain.size());

    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"UBAANext session", nullptr, nullptr, nullptr, 0, &out)) {
        return {};
    }

    std::string encrypted(reinterpret_cast<char *>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return encrypted;
}

std::string unprotect_bytes(const std::string &encrypted) {
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(encrypted.data()));
    in.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        return {};
    }

    std::string plain(reinterpret_cast<char *>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return plain;
}
#endif

} // namespace

PlainFileStore::PlainFileStore(std::filesystem::path file_path, bool encrypted)
    : m_file_path(std::move(file_path)), m_encrypted(encrypted) {
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

void PlainFileStore::clear() {
    m_data.clear();
}

void PlainFileStore::load_from_file() {
    if (!std::filesystem::exists(m_file_path)) {
        return;
    }

    std::ifstream file(m_file_path, std::ios::binary);
    if (!file.is_open()) {
        return;
    }

    std::string raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (m_encrypted) {
#if defined(_WIN32)
        raw = unprotect_bytes(raw);
#else
        raw.clear();
#endif
    }

    if (raw.empty()) {
        return;
    }
    m_data = parse_data(raw);
}

void PlainFileStore::save_to_file() const {
    auto parent = m_file_path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::filesystem::create_directories(parent);
    }

    std::string raw = serialize_data(m_data);
    if (m_encrypted) {
#if defined(_WIN32)
        raw = protect_bytes(raw);
        if (raw.empty() && !m_data.empty()) {
            return;
        }
#else
        return;
#endif
    }

    std::ofstream file(m_file_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return;
    }
    file.write(raw.data(), static_cast<std::streamsize>(raw.size()));
}

} // namespace UBAANextCli
