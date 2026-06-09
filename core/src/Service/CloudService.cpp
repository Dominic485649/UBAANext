#include <UBAANext/Service/CloudService.hpp>

#include <UBAANext/Net/HttpHeaders.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/CloudParser.hpp>
#include <UBAANext/Protocol/CasFormParser.hpp>
#include <UBAANext/Protocol/RedirectNavigator.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>
#include <UBAANext/Security/SecurityRedaction.hpp>
#include <UBAANext/Service/ResponseUtils.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace UBAANext {
namespace {

constexpr const char *kCloudLoginUrl = "https://bhpan.buaa.edu.cn/anyshare/oauth2/login?redirect=%2Fanyshare%2Fzh-cn%2Fportal";
constexpr const char *kCloudSsoUrl = "https://sso.buaa.edu.cn/login?service=https://bhpan.buaa.edu.cn/oauth2/signin";
constexpr const char *kSsoLoginUrl = "https://sso.buaa.edu.cn/login";
constexpr const char *kCloudRefreshUrl = "https://bhpan.buaa.edu.cn/anyshare/oauth2/login/refreshToken";
constexpr const char *kCloudRootUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/entry-doc-lib";
constexpr const char *kCloudUserRootUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/owned-doc-lib";
constexpr const char *kCloudListUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/dir/list";
constexpr const char *kCloudSizeUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/dir/size";
constexpr const char *kCloudRecycleUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/recycle/list";
constexpr const char *kCloudShareHistoryUrl = "https://bhpan.buaa.edu.cn/api/doc-share/v1/docs-shared-with-anyone";
constexpr const char *kCloudSuggestNameUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/file/getsuggestname";
constexpr const char *kCloudCreateDirUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/dir/create";
constexpr const char *kCloudRenameUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/dir/rename";
constexpr const char *kCloudMoveUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/dir/move";
constexpr const char *kCloudCopyUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/dir/copy";
constexpr const char *kCloudDeleteUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/file/delete";
constexpr const char *kCloudRecycleDeleteUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/recycle/delete";
constexpr const char *kCloudRecycleRestoreUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/recycle/restore";
constexpr const char *kCloudShareCreateUrl = "https://bhpan.buaa.edu.cn/api/shared-link/v1/document/anonymous";
constexpr const char *kCloudShareLinkInfoUrl = "https://bhpan.buaa.edu.cn/api/shared-link/v1/links/";
constexpr const char *kCloudEntryItemUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/entry-item";
constexpr const char *kCloudSingleDownloadUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/file/osdownload";
constexpr const char *kCloudBatchDownloadUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/file/batchdownload";
constexpr const char *kCloudPreuploadUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/file/predupload";
constexpr const char *kCloudFastUploadUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/file/dupload";
constexpr const char *kCloudBeginUploadUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/file/osbeginupload";
constexpr const char *kCloudEndUploadUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/file/osendupload";
constexpr const char *kCloudInitMultiUploadUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/file/osinitmultiupload";
constexpr const char *kCloudUploadPartUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/file/osuploadpart";
constexpr const char *kCloudCompleteUploadUrl = "https://bhpan.buaa.edu.cn/api/efast/v1/file/oscompleteupload";
constexpr const char *kCloudHost = "bhpan.buaa.edu.cn";
constexpr const char *kSsoHost = "sso.buaa.edu.cn";
constexpr const char *kVpnHost = "d.buaa.edu.cn";
constexpr const char *kCloudTokenCookie = "client.oauth2_token";
constexpr const char *kCloudLoginChallengeCookie = "login_challenge";
constexpr std::uint64_t kCloudUploadPartSize = 20ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kCloudUploadSliceSize = 200ULL * 1024ULL;
constexpr const char *kAnyshareSigninPublicKeyDerBase64 =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4E+eiWRwffhRIPQYvlXU"
    "jf0b3HqCmosiCxbFCYI/gdfDBhrTUzbt3fL3o/gRQQBEPf69vhJMFH2ZMtaJM6oh"
    "E3yQef331liPVM0YvqMOgvoID+zDa1NIZFObSsjOKhvZtv9esO0REeiVEPKNc+Dp"
    "6il3x7TV9VKGEv0+iriNjqv7TGAexo2jVtLm50iVKTju2qmCDG83SnVHzsiNj70M"
    "iviqiLpgz72IxjF+xN4bRw8I5dD0GwwO8kDoJUGWgTds+VckCwdtZA65oui9Osk5"
    "t1a4pg6Xu9+HFcEuqwJTDxATvGAz1/YW0oUisjM0ObKTRDVSfnTYeaBsN6L+M+8g"
    "CwIDAQAB";

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    if (mode == ConnectionMode::WebVPN && url.rfind("https://d.buaa.edu.cn/", 0) != 0) return VpnCipher::to_vpn_url(url);
    return url;
}

void disable_redirects(HttpRequest &request) {
    request.redirect.follow_redirects = false;
    request.redirect.max_redirects = 0;
    request.redirect.expose_location_header = true;
}

std::string header_value(const HttpResponse &response, const std::string &name) {
    for (const auto &[key, value] : response.headers) {
        if (key.size() != name.size()) continue;
        bool same = true;
        for (std::size_t i = 0; i < key.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(key[i])) != std::tolower(static_cast<unsigned char>(name[i]))) {
                same = false;
                break;
            }
        }
        if (same) return value;
    }
    return {};
}

bool is_redirect(int status) {
    return status >= 300 && status < 400;
}

std::string resolve_location(const std::string &current_url, const std::string &location) {
    return Protocol::resolve_location(current_url, location);
}

std::string extract_host(const std::string &url) {
    const auto scheme = url.find("://");
    if (scheme == std::string::npos) return {};
    auto start = scheme + 3;
    auto end = url.find_first_of("/:?#", start);
    auto host = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
    auto at = host.rfind('@');
    if (at != std::string::npos) return {};
    auto colon = host.find(':');
    if (colon != std::string::npos) host = host.substr(0, colon);
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return host;
}

bool allowed_cloud_redirect(const std::string &url) {
    const auto logical_url = VpnCipher::from_vpn_url(url);
    if (logical_url.rfind("https://", 0) != 0) return false;
    const auto host = extract_host(logical_url);
    return host == kSsoHost || host == kCloudHost || host == kVpnHost;
}

std::string path_from_url(const std::string &url);
std::string append_query(const std::string &url, const std::vector<std::pair<std::string, std::string>> &query);
std::string cloud_sso_url(ConnectionMode mode, const std::string &login_challenge);

std::string url_path(const std::string &url) {
    return path_from_url(VpnCipher::from_vpn_url(url));
}

std::string redacted_error_message(const std::string &message) {
    return Security::redact_sensitive_text(message);
}

std::string bearer_header_value(const std::string &token) {
    static constexpr std::string_view prefix = "Bearer ";
    if (token.size() >= prefix.size()) {
        bool has_prefix = true;
        for (std::size_t i = 0; i < prefix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(token[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) {
                has_prefix = false;
                break;
            }
        }
        if (has_prefix) return token;
    }
    return "Bearer " + token;
}

std::uint32_t md5_rotate_left(std::uint32_t value, std::uint32_t count) {
    return (value << count) | (value >> (32U - count));
}

class Md5Digest {
public:
    Md5Digest() { reset(); }

    void update(const unsigned char *data, std::size_t length) {
        if (!data || length == 0) return;
        m_total_size += length;
        std::size_t offset = 0;
        if (m_buffer_size > 0) {
            const auto take = std::min<std::size_t>(64 - m_buffer_size, length);
            std::memcpy(m_buffer.data() + m_buffer_size, data, take);
            m_buffer_size += take;
            offset += take;
            if (m_buffer_size == 64) {
                transform(m_buffer.data());
                m_buffer_size = 0;
            }
        }
        while (offset + 64 <= length) {
            transform(data + offset);
            offset += 64;
        }
        if (offset < length) {
            m_buffer_size = length - offset;
            std::memcpy(m_buffer.data(), data + offset, m_buffer_size);
        }
    }

    [[nodiscard]] std::array<unsigned char, 16> finalize() {
        const auto bit_length = static_cast<std::uint64_t>(m_total_size) * 8ULL;
        std::array<unsigned char, 64> padding{};
        padding[0] = 0x80;
        const auto pad_length = m_buffer_size < 56 ? 56 - m_buffer_size : 120 - m_buffer_size;
        update(padding.data(), pad_length);

        std::array<unsigned char, 8> encoded_length{};
        for (std::size_t i = 0; i < encoded_length.size(); ++i) {
            encoded_length[i] = static_cast<unsigned char>((bit_length >> (8U * i)) & 0xffU);
        }
        update(encoded_length.data(), encoded_length.size());

        std::array<unsigned char, 16> digest{};
        for (std::size_t i = 0; i < 4; ++i) {
            digest[i * 4 + 0] = static_cast<unsigned char>(m_state[i] & 0xffU);
            digest[i * 4 + 1] = static_cast<unsigned char>((m_state[i] >> 8U) & 0xffU);
            digest[i * 4 + 2] = static_cast<unsigned char>((m_state[i] >> 16U) & 0xffU);
            digest[i * 4 + 3] = static_cast<unsigned char>((m_state[i] >> 24U) & 0xffU);
        }
        return digest;
    }

private:
    void reset() {
        m_state = {0x67452301U, 0xefcdab89U, 0x98badcfeU, 0x10325476U};
        m_total_size = 0;
        m_buffer_size = 0;
        m_buffer.fill(0);
    }

    void transform(const unsigned char *chunk) {
        static constexpr std::array<std::uint32_t, 64> shifts = {
            7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
            5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
        };
        static constexpr std::array<std::uint32_t, 64> constants = {
            0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU, 0xf57c0fafU, 0x4787c62aU, 0xa8304613U, 0xfd469501U,
            0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU, 0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U,
            0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU, 0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
            0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU, 0xa9e3e905U, 0xfcefa3f8U, 0x676f02d9U, 0x8d2a4c8aU,
            0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU, 0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
            0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U, 0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,
            0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U, 0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU, 0x85845dd1U,
            0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U, 0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U,
        };

        std::array<std::uint32_t, 16> words{};
        for (std::size_t i = 0; i < words.size(); ++i) {
            words[i] = static_cast<std::uint32_t>(chunk[i * 4 + 0]) |
                       (static_cast<std::uint32_t>(chunk[i * 4 + 1]) << 8U) |
                       (static_cast<std::uint32_t>(chunk[i * 4 + 2]) << 16U) |
                       (static_cast<std::uint32_t>(chunk[i * 4 + 3]) << 24U);
        }

        auto a = m_state[0];
        auto b = m_state[1];
        auto c = m_state[2];
        auto d = m_state[3];
        for (std::uint32_t i = 0; i < 64; ++i) {
            std::uint32_t f = 0;
            std::uint32_t g = 0;
            if (i < 16) {
                f = (b & c) | ((~b) & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | ((~d) & c);
                g = (5U * i + 1U) % 16U;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3U * i + 5U) % 16U;
            } else {
                f = c ^ (b | (~d));
                g = (7U * i) % 16U;
            }
            const auto next = d;
            d = c;
            c = b;
            b = b + md5_rotate_left(a + f + constants[i] + words[g], shifts[i]);
            a = next;
        }
        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
    }

    std::array<std::uint32_t, 4> m_state{};
    std::uint64_t m_total_size = 0;
    std::array<unsigned char, 64> m_buffer{};
    std::size_t m_buffer_size = 0;
};

std::array<std::uint32_t, 256> make_crc32_table() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < table.size(); ++i) {
        auto value = i;
        for (int bit = 0; bit < 8; ++bit) {
            value = (value & 1U) ? (0xedb88320U ^ (value >> 1U)) : (value >> 1U);
        }
        table[i] = value;
    }
    return table;
}

class Crc32Digest {
public:
    void update(const unsigned char *data, std::size_t length) {
        static const auto table = make_crc32_table();
        for (std::size_t i = 0; i < length; ++i) {
            m_value = table[(m_value ^ data[i]) & 0xffU] ^ (m_value >> 8U);
        }
    }

    [[nodiscard]] std::uint32_t finalize() const { return m_value ^ 0xffffffffU; }

private:
    std::uint32_t m_value = 0xffffffffU;
};

std::string hex_lower(const unsigned char *bytes, std::size_t length) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(length * 2);
    for (std::size_t i = 0; i < length; ++i) {
        out.push_back(digits[(bytes[i] >> 4U) & 0x0fU]);
        out.push_back(digits[bytes[i] & 0x0fU]);
    }
    return out;
}

std::string crc32_hex(std::uint32_t value) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0') << std::setw(8) << value;
    return out.str();
}

struct CloudUploadHashes {
    std::uint64_t length = 0;
    std::string slice_md5;
    std::string md5;
    std::string crc32;
};

Result<CloudUploadHashes> compute_upload_hashes(IUploadSource &source) {
    auto declared_size = source.size();
    if (!declared_size) return make_error(declared_size.error().code, declared_size.error().message);
    if (*declared_size == 0) return make_error(ErrorCode::InvalidArgument, "file upload 不支持空文件");

    auto rewound = source.rewind();
    if (!rewound) return make_error(rewound.error().code, rewound.error().message);

    Md5Digest full_md5;
    Md5Digest slice_md5;
    Crc32Digest crc32;
    std::uint64_t total = 0;
    std::uint64_t slice_consumed = 0;
    std::vector<unsigned char> buffer(128 * 1024);

    for (;;) {
        auto read = source.read(buffer.data(), buffer.size());
        if (!read) return make_error(read.error().code, read.error().message);
        if (*read == 0) break;
        full_md5.update(buffer.data(), *read);
        crc32.update(buffer.data(), *read);
        if (slice_consumed < kCloudUploadSliceSize) {
            const auto slice_take = static_cast<std::size_t>(std::min<std::uint64_t>(kCloudUploadSliceSize - slice_consumed, *read));
            slice_md5.update(buffer.data(), slice_take);
            slice_consumed += slice_take;
        }
        total += *read;
        if (total > *declared_size) return make_error(ErrorCode::InvalidArgument, "上传文件读取长度超过声明长度");
    }

    if (total != *declared_size) return make_error(ErrorCode::InvalidArgument, "上传文件读取长度与声明长度不一致");
    rewound = source.rewind();
    if (!rewound) return make_error(rewound.error().code, rewound.error().message);

    const auto full = full_md5.finalize();
    const auto slice = slice_md5.finalize();
    CloudUploadHashes hashes;
    hashes.length = total;
    hashes.md5 = hex_lower(full.data(), full.size());
    hashes.slice_md5 = hex_lower(slice.data(), slice.size());
    hashes.crc32 = crc32_hex(crc32.finalize());
    return hashes;
}

std::optional<std::string> token_from_cookie_jar(const CookieJar *cookies, ConnectionMode mode) {
    if (!cookies) return std::nullopt;
    const auto host = mode == ConnectionMode::WebVPN ? kVpnHost : kCloudHost;
    auto token = cookies->get_cookie(host, kCloudTokenCookie);
    if (token && !token->empty()) return token;
    return std::nullopt;
}

std::string path_from_url(const std::string &url) {
    const auto scheme = url.find("://");
    const auto start = scheme == std::string::npos ? 0 : scheme + 3;
    const auto path_start = url.find('/', start);
    if (path_start == std::string::npos) return "/";
    const auto path_end = url.find_first_of("?#", path_start);
    return url.substr(path_start, path_end == std::string::npos ? std::string::npos : path_end - path_start);
}

std::string origin_from_url(const std::string &url) {
    const auto scheme = url.find("://");
    if (scheme == std::string::npos) return {};
    const auto authority_start = scheme + 3;
    const auto authority_end = url.find_first_of("/?#", authority_start);
    const auto authority = url.substr(authority_start, authority_end == std::string::npos ? std::string::npos : authority_end - authority_start);
    if (authority.empty() || authority.find('@') != std::string::npos) return {};
    return url.substr(0, authority_start) + authority;
}

bool cookie_name_equals(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(left[i])) != std::tolower(static_cast<unsigned char>(right[i]))) return false;
    }
    return true;
}

std::string cookie_pair_name(std::string_view pair) {
    while (!pair.empty() && std::isspace(static_cast<unsigned char>(pair.front()))) pair.remove_prefix(1);
    const auto eq = pair.find('=');
    auto name = pair.substr(0, eq == std::string_view::npos ? pair.size() : eq);
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.remove_suffix(1);
    return std::string{name};
}

bool cookie_header_contains_name(const std::string &header, std::string_view name) {
    for (std::size_t pos = 0; pos < header.size();) {
        auto next = header.find(';', pos);
        const auto pair = std::string_view{header}.substr(pos, next == std::string::npos ? std::string_view::npos : next - pos);
        if (cookie_name_equals(cookie_pair_name(pair), name)) return true;
        if (next == std::string::npos) break;
        pos = next + 1;
    }
    return false;
}

std::string append_cookie_header_filtered(std::string header,
                                          const std::string &extra,
                                          const std::vector<std::string_view> &excluded_names) {
    for (std::size_t pos = 0; pos < extra.size();) {
        auto next = extra.find(';', pos);
        auto pair = std::string_view{extra}.substr(pos, next == std::string::npos ? std::string_view::npos : next - pos);
        while (!pair.empty() && std::isspace(static_cast<unsigned char>(pair.front()))) pair.remove_prefix(1);
        while (!pair.empty() && std::isspace(static_cast<unsigned char>(pair.back()))) pair.remove_suffix(1);
        const auto name = cookie_pair_name(pair);
        bool skip = name.empty() || cookie_header_contains_name(header, name);
        for (auto excluded : excluded_names) {
            if (cookie_name_equals(name, excluded)) {
                skip = true;
                break;
            }
        }
        if (!skip) {
            if (!header.empty()) header += "; ";
            header.append(pair.data(), pair.size());
        }
        if (next == std::string::npos) break;
        pos = next + 1;
    }
    return header;
}

std::string with_login_challenge_cookie(std::string cookie_header, const std::string &login_challenge) {
    if (login_challenge.empty()) return cookie_header;
    if (!cookie_header.empty()) cookie_header += "; ";
    cookie_header += std::string(kCloudLoginChallengeCookie) + "=" + login_challenge;
    return cookie_header;
}

std::string webvpn_forwarded_prefix(const std::string &logical_url) {
    const auto normalized_url = VpnCipher::from_vpn_url(logical_url);
    if (extract_host(normalized_url) != kCloudHost) return {};
    auto vpn_root = VpnCipher::to_vpn_url(std::string{"https://"} + kCloudHost + "/");
    auto prefix = path_from_url(vpn_root);
    while (prefix.size() > 1 && prefix.back() == '/') prefix.pop_back();
    return prefix;
}

std::string with_cookie_pair(std::string cookie_header, std::string_view name, const std::string &value) {
    if (value.empty() || cookie_header_contains_name(cookie_header, name)) return cookie_header;
    if (!cookie_header.empty()) cookie_header += "; ";
    cookie_header.append(name.data(), name.size());
    cookie_header += '=';
    cookie_header += value;
    return cookie_header;
}

std::string current_cookie_header(ICookieStore *cookie_store, const std::string &logical_url, ConnectionMode mode) {
    if (!cookie_store || !cookie_store->current()) return {};
    const auto normalized_url = VpnCipher::from_vpn_url(logical_url);
    const auto host = extract_host(normalized_url);
    const auto path = path_from_url(normalized_url);
    if (mode == ConnectionMode::WebVPN) {
        auto header = cookie_store->current()->to_header(kVpnHost);
        const auto logical_header = cookie_store->current()->to_header(host, path);
        header = append_cookie_header_filtered(std::move(header), logical_header, {kCloudTokenCookie, "client.oauth2_refresh_token"});
        if (host == kCloudHost) {
            header = with_cookie_pair(std::move(header), "X-Forwarded-Prefix", webvpn_forwarded_prefix(normalized_url));
            header = with_cookie_pair(std::move(header), "X-Forwarded-Web-Client-Basepath", "/anyshare");
        }
        return header;
    }
    return cookie_store->current()->to_header(host, path);
}

std::string html_cookie_header(ICookieStore *cookie_store,
                               const std::string &logical_url,
                               ConnectionMode mode,
                               const std::string &login_challenge = {}) {
    auto cookie_header = current_cookie_header(cookie_store, logical_url, mode);
    if (!login_challenge.empty() && extract_host(VpnCipher::from_vpn_url(logical_url)) == kCloudHost) {
        cookie_header = with_login_challenge_cookie(std::move(cookie_header), login_challenge);
    }
    return cookie_header;
}

void apply_cookie_header(HttpRequest &request, const std::string &cookie_header) {
    if (!cookie_header.empty()) request.headers["Cookie"] = cookie_header;
}

struct DirectSsoActivationProbe {
    bool attempted = false;
    bool success = false;
    int page_status = 0;
    int post_status = 0;
    std::string result = "skipped";
};

[[maybe_unused]] DirectSsoActivationProbe activate_direct_sso_session(IHttpClient &http_client, const CloudLoginCredentials *credentials) {
    DirectSsoActivationProbe probe;
    if (!credentials || credentials->username.empty() || credentials->password.empty()) return probe;
    probe.attempted = true;
    probe.result = "start";

    HttpRequest page;
    page.method = HttpMethod::Get;
    page.url = kSsoLoginUrl;
    page.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
    page.headers["User-Agent"] = kUserAgent;
    disable_redirects(page);
    auto page_response = http_client.send(page);
    if (!page_response) {
        probe.result = "page-network";
        return probe;
    }
    probe.page_status = page_response->status_code;
    if (is_redirect(page_response->status_code)) {
        probe.success = true;
        probe.result = "page-redirect";
        return probe;
    }

    const auto execution = Protocol::extract_execution(page_response->body);
    if (execution.empty()) {
        probe.success = page_response->status_code >= 200 && page_response->status_code < 400;
        probe.result = probe.success ? "page-no-form" : "page-no-execution";
        return probe;
    }

    HttpRequest login;
    login.method = HttpMethod::Post;
    login.url = kSsoLoginUrl;
    login.headers["Content-Type"] = "application/x-www-form-urlencoded";
    login.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
    login.headers["User-Agent"] = kUserAgent;
    login.headers["Referer"] = kSsoLoginUrl;
    login.body = Protocol::build_login_form(page_response->body, credentials->username, credentials->password, execution, "");
    disable_redirects(login);
    auto login_response = http_client.send(login);
    if (!login_response) {
        probe.result = "post-network";
        return probe;
    }
    probe.post_status = login_response->status_code;
    if (is_redirect(login_response->status_code)) {
        probe.success = true;
        probe.result = "post-redirect";
        return probe;
    }
    probe.success = login_response->status_code >= 200 && login_response->status_code < 400 && Protocol::extract_execution(login_response->body).empty();
    probe.result = probe.success ? "post-no-form" : "post-form";
    return probe;
}

std::string direct_sso_activation_summary(const DirectSsoActivationProbe &probe) {
    std::string summary = probe.attempted ? (probe.success ? "ok" : "failed") : "skipped";
    summary += "/" + probe.result;
    if (probe.page_status != 0) summary += ",page=" + std::to_string(probe.page_status);
    if (probe.post_status != 0) summary += ",post=" + std::to_string(probe.post_status);
    return summary;
}

HttpRequest make_html_get_request(const std::string &logical_url,
                                  ConnectionMode mode,
                                  ICookieStore *cookie_store,
                                  const std::string &login_challenge = {},
                                  const std::string &referer = {}) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(logical_url, mode);
    request.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";
    request.headers["User-Agent"] = kUserAgent;
    if (!referer.empty()) request.headers["Referer"] = resolve_for_mode(referer, mode);
    apply_cookie_header(request, html_cookie_header(cookie_store, logical_url, mode, login_challenge));
    disable_redirects(request);
    return request;
}

HttpRequest make_cloud_refresh_request(const std::string &logical_url,
                                       ConnectionMode mode,
                                       ICookieStore *cookie_store,
                                       const std::string &referer) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(logical_url, mode);
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8";
    request.headers["Cache-Control"] = "no-cache";
    request.headers["Pragma"] = "no-cache";
    request.headers["User-Agent"] = kUserAgent;
    request.headers["X-Requested-With"] = "XMLHttpRequest";
    if (!referer.empty()) request.headers["Referer"] = resolve_for_mode(referer, mode);
    apply_cookie_header(request, html_cookie_header(cookie_store, logical_url, mode));
    disable_redirects(request);
    return request;
}

std::string resolve_redirect_target(const std::string &actual_url, const std::string &logical_url, const std::string &location) {
    if (location.rfind("https://d.buaa.edu.cn/", 0) == 0) return location;
    if (location.rfind("http://", 0) == 0 || location.rfind("https://", 0) == 0) return VpnCipher::from_vpn_url(location);
    if (location.rfind("/http/", 0) == 0 || location.rfind("/https/", 0) == 0) {
        auto actual_host = extract_host(actual_url);
        if (!actual_host.empty()) return "https://" + actual_host + location;
    }
    return VpnCipher::from_vpn_url(resolve_location(logical_url, location));
}

Result<std::optional<std::string>> read_cloud_token_cookie(ICookieStore *cookie_store, bool load_if_missing, ConnectionMode mode) {
    if (!cookie_store) return std::optional<std::string>{};
    if (auto token = token_from_cookie_jar(cookie_store->current(), mode)) return token;
    if (!load_if_missing) return std::optional<std::string>{};

    auto loaded = cookie_store->load();
    if (!loaded) return make_error(loaded.error().code, "北航云盘 Cookie 读取失败: " + redacted_error_message(loaded.error().message));
    if (auto token = token_from_cookie_jar(&*loaded, mode)) return token;
    if (auto token = token_from_cookie_jar(cookie_store->current(), mode)) return token;
    return std::optional<std::string>{};
}

struct DirectSsoBridgeProbe {
    bool attempted = false;
    bool success = false;
    int service_status = 0;
    int result_status = 0;
    std::string result = "skipped";
    std::string path;
};

struct DirectSsoBridgeResult {
    DirectSsoBridgeProbe probe;
    bool has_response = false;
    std::string current_url;
    HttpResponse response;
};

std::string direct_sso_bridge_summary(const DirectSsoBridgeProbe &probe) {
    std::string summary = probe.attempted ? (probe.success ? "ok" : "failed") : "skipped";
    summary += "/" + probe.result;
    if (probe.service_status != 0) summary += ",service=" + std::to_string(probe.service_status);
    if (probe.result_status != 0) summary += ",result=" + std::to_string(probe.result_status);
    if (!probe.path.empty()) summary += ",path=" + probe.path;
    return summary;
}

std::string cloud_cookie_presence_summary(ICookieStore *cookie_store) {
    const auto *cookies = cookie_store ? cookie_store->current() : nullptr;
    if (!cookies) return "none";
    auto has = [cookies](const std::string &host, const std::string &name) {
        auto value = cookies->get_cookie(host, name);
        return value && !value->empty();
    };
    auto count_host = [cookies](const std::string &host) {
        std::size_t count = 0;
        const auto prefix = host + "\t";
        for (const auto &line : cookies->serialize()) {
            if (line.rfind(prefix, 0) == 0) ++count;
        }
        return count;
    };
    auto host_names = [cookies](const std::string &host) {
        std::vector<std::string> names;
        const auto prefix = host + "\t";
        for (const auto &line : cookies->serialize()) {
            if (line.rfind(prefix, 0) != 0) continue;
            const auto path_end = line.find('\t', prefix.size());
            if (path_end == std::string::npos) continue;
            const auto name_end = line.find('\t', path_end + 1);
            if (name_end == std::string::npos || name_end == path_end + 1) continue;
            names.push_back(line.substr(path_end + 1, name_end - path_end - 1));
        }
        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());
        std::string joined;
        for (const auto &name : names) {
            if (!joined.empty()) joined += "|";
            joined += name;
        }
        return joined;
    };
    std::string summary = "vpnToken=";
    summary += has(kVpnHost, kCloudTokenCookie) ? "1" : "0";
    summary += ",cloudToken=";
    summary += has(kCloudHost, kCloudTokenCookie) ? "1" : "0";
    summary += ",vpnRefresh=";
    summary += has(kVpnHost, "client.oauth2_refresh_token") ? "1" : "0";
    summary += ",cloudRefresh=";
    summary += has(kCloudHost, "client.oauth2_refresh_token") ? "1" : "0";
    summary += ",vpnCookies=" + std::to_string(count_host(kVpnHost));
    summary += ",cloudCookies=" + std::to_string(count_host(kCloudHost));
    summary += ",ssoCookies=" + std::to_string(count_host(kSsoHost));
    const auto vpn_names = host_names(kVpnHost);
    if (!vpn_names.empty()) summary += ",vpnNames=" + vpn_names;
    const auto cloud_names = host_names(kCloudHost);
    if (!cloud_names.empty()) summary += ",cloudNames=" + cloud_names;
    return summary;
}

[[maybe_unused]] DirectSsoBridgeResult activate_cloud_via_direct_sso_bridge(IHttpClient &http_client,
                                                                            ICookieStore *cookie_store,
                                                                            ConnectionMode mode,
                                                                            const std::string &login_challenge) {
    DirectSsoBridgeResult result;
    if (mode != ConnectionMode::WebVPN || login_challenge.empty()) return result;
    result.probe.attempted = true;
    result.probe.result = "start";

    const auto service_url = cloud_sso_url(mode, login_challenge);
    HttpRequest service;
    service.method = HttpMethod::Get;
    service.url = service_url;
    service.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";
    service.headers["User-Agent"] = kUserAgent;
    disable_redirects(service);
    auto response = http_client.send(service);
    if (!response) {
        result.probe.result = "service-network";
        return result;
    }
    result.probe.service_status = response->status_code;
    if (!is_redirect(response->status_code)) {
        result.probe.result = "service-no-redirect";
        return result;
    }

    auto location = header_value(*response, "Location");
    if (location.empty()) {
        result.probe.result = "service-no-location";
        return result;
    }
    auto current_url = resolve_redirect_target(service.url, service_url, location);
    if (!allowed_cloud_redirect(current_url)) {
        result.probe.result = "service-unsafe";
        return result;
    }

    HttpRequest request;
    for (int redirects = 0; redirects < 12; ++redirects) {
        request = make_html_get_request(current_url, mode, cookie_store, login_challenge, service_url);
        response = http_client.send(request);
        if (!response) {
            result.probe.result = "follow-network";
            return result;
        }
        if (!is_redirect(response->status_code)) break;
        location = header_value(*response, "Location");
        if (location.empty()) {
            result.probe.result = "follow-no-location";
            return result;
        }
        auto next_url = resolve_redirect_target(request.url, current_url, location);
        if (!allowed_cloud_redirect(next_url)) {
            result.probe.result = "follow-unsafe";
            return result;
        }
        current_url = std::move(next_url);
    }

    result.has_response = true;
    result.current_url = std::move(current_url);
    result.response = *response;
    result.probe.result_status = response->status_code;
    result.probe.path = url_path(result.current_url);
    const bool still_signin = result.probe.path.rfind("/oauth2/signin", 0) == 0;
    const bool has_token = token_from_cookie_jar(cookie_store ? cookie_store->current() : nullptr, mode).has_value();
    result.probe.success = has_token || (!still_signin && response->status_code >= 200 && response->status_code < 400);
    result.probe.result = result.probe.success ? "followed" : "follow-failed";
    return result;
}

bool json_code_is_success(const nlohmann::json &value) {
    if (!value.is_object()) return false;
    auto code = value.find("code");
    if (code == value.end()) return false;
    if (code->is_number_integer()) return code->get<long long>() == 200 || code->get<long long>() == 0;
    if (code->is_number_unsigned()) return code->get<unsigned long long>() == 200 || code->get<unsigned long long>() == 0;
    if (code->is_string()) return *code == "200" || *code == "0";
    return false;
}

bool looks_like_cloud_oauth_token(std::string_view value) {
    if (value.size() < 64 || value.size() > 8192) return false;
    bool has_alpha_or_digit = false;
    bool has_separator = false;
    for (const auto ch : value) {
        const auto uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch)) {
            has_alpha_or_digit = true;
            continue;
        }
        if (ch == '.' || ch == '_' || ch == '-' || ch == '~' || ch == '+' || ch == '/' || ch == '=') {
            if (ch == '.') has_separator = true;
            continue;
        }
        return false;
    }
    return has_alpha_or_digit && (has_separator || value.size() >= 128);
}

std::string json_message_shape(const nlohmann::json &value) {
    if (!value.is_string()) return value.type_name();
    const auto text = value.get<std::string>();
    std::string shape;
    if (text.empty()) {
        shape = "empty";
    } else if (text.size() < 16) {
        shape = "short";
    } else if (text.size() < 64) {
        shape = "medium";
    } else if (text.size() < 128) {
        shape = "long";
    } else {
        shape = "very-long";
    }
    shape += looks_like_cloud_oauth_token(text) ? ":candidate" : ":text";
    return shape;
}

std::optional<std::string> token_from_json_value(const nlohmann::json &value) {
    if (value.is_object()) {
        static constexpr std::array<std::string_view, 5> keys{
            "client.oauth2_token", "oauth2_token", "access_token", "accessToken", "token",
        };
        for (auto key : keys) {
            auto it = value.find(std::string(key));
            if (it != value.end() && it->is_string() && !it->get<std::string>().empty()) return it->get<std::string>();
        }
        auto message = value.find("message");
        if (message != value.end() && message->is_string() && json_code_is_success(value)) {
            auto candidate = message->get<std::string>();
            if (looks_like_cloud_oauth_token(candidate)) return candidate;
        }
        for (const auto &item : value.items()) {
            if (auto token = token_from_json_value(item.value())) return token;
        }
    } else if (value.is_array()) {
        for (const auto &item : value) {
            if (auto token = token_from_json_value(item)) return token;
        }
    }
    return std::nullopt;
}

std::optional<std::string> token_from_json_text(const std::string &body) {
    auto json = nlohmann::json::parse(body, nullptr, false);
    if (json.is_discarded()) return std::nullopt;
    return token_from_json_value(json);
}

std::string json_shape_summary(const std::string &body) {
    auto json = nlohmann::json::parse(body, nullptr, false);
    if (json.is_discarded()) {
        return std::string{"non-json,len="} + (body.size() < 8192 ? "lt8k" : "gte8k");
    }
    std::vector<std::string> parts;
    auto push_keys = [&parts](const char *prefix, const nlohmann::json &value) {
        if (!value.is_object()) return;
        std::vector<std::string> keys;
        for (const auto &item : value.items()) keys.push_back(item.key());
        std::sort(keys.begin(), keys.end());
        std::string joined;
        for (const auto &key : keys) {
            if (!joined.empty()) joined += "|";
            joined += key;
        }
        if (!joined.empty()) parts.push_back(std::string{prefix} + joined);
    };
    push_keys("keys=", json);
    if (json.is_object()) {
        auto success = json.find("success");
        if (success != json.end() && success->is_boolean()) parts.push_back(std::string{"success="} + (*success ? "true" : "false"));
        auto code = json.find("code");
        if (code != json.end()) {
            if (code->is_number_integer()) {
                parts.push_back("code=" + std::to_string(code->get<long long>()));
            } else if (code->is_number_unsigned()) {
                parts.push_back("code=" + std::to_string(code->get<unsigned long long>()));
            } else if (code->is_string()) {
                auto value = code->get<std::string>();
                bool safe = !value.empty() && value.size() <= 64;
                for (const auto ch : value) {
                    const auto uch = static_cast<unsigned char>(ch);
                    if (!std::isalnum(uch) && ch != '_' && ch != '-' && ch != '.') {
                        safe = false;
                        break;
                    }
                }
                parts.push_back(safe ? "code=" + value : "code=string");
            } else {
                parts.push_back("code=" + std::string{code->type_name()});
            }
        }
        auto data = json.find("data");
        if (data != json.end()) push_keys("dataKeys=", *data);
        auto message = json.find("message");
        if (message != json.end()) parts.push_back("message=" + json_message_shape(*message));
    }
    if (parts.empty()) return json.is_array() ? "array" : "json";
    std::string summary;
    for (const auto &part : parts) {
        if (!summary.empty()) summary += ",";
        summary += part;
    }
    return summary;
}

std::string set_cookie_name_summary(const HttpResponse &response) {
    std::vector<std::string> names;
    for (const auto &[key, value] : response.headers) {
        if (key.size() != 10) continue;
        bool same = true;
        static constexpr std::string_view target = "set-cookie";
        for (std::size_t i = 0; i < target.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(key[i])) != target[i]) {
                same = false;
                break;
            }
        }
        if (!same) continue;
        for (std::size_t pos = 0; pos < value.size();) {
            auto next = value.find('\n', pos);
            auto line = std::string_view{value}.substr(pos, next == std::string::npos ? std::string_view::npos : next - pos);
            while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) line.remove_prefix(1);
            const auto eq = line.find('=');
            if (eq != std::string_view::npos) {
                auto name = line.substr(0, eq);
                while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.remove_suffix(1);
                if (!name.empty()) names.emplace_back(name);
            }
            if (next == std::string::npos) break;
            pos = next + 1;
        }
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    if (names.empty()) return "none";
    std::string summary;
    for (const auto &name : names) {
        if (!summary.empty()) summary += "|";
        summary += name;
    }
    return summary;
}

std::string set_cookie_shape_summary(const HttpResponse &response) {
    const auto names_text = set_cookie_name_summary(response);
    if (names_text == "none") return names_text;
    std::size_t total = 0;
    std::size_t ctk = 0;
    std::size_t rfk = 0;
    std::size_t hydra = 0;
    std::size_t idt = 0;
    std::size_t state = 0;
    std::size_t csrf = 0;
    std::size_t ori = 0;
    std::size_t xfp = 0;
    std::size_t xfb = 0;
    std::size_t upstream = 0;
    std::size_t vpn = 0;
    for (std::size_t pos = 0; pos < names_text.size();) {
        auto next = names_text.find('|', pos);
        auto name = names_text.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        auto lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        ++total;
        if (lower == "client.oauth2_token") {
            ++ctk;
        } else if (lower == "client.oauth2_refresh_token") {
            ++rfk;
        } else if (lower.find("hydra") != std::string::npos) {
            ++hydra;
        } else if (lower == "id_token") {
            ++idt;
        } else if (lower == "state") {
            ++state;
        } else if (lower.find("csrf") != std::string::npos) {
            ++csrf;
        } else if (lower == "client.origin_uri") {
            ++ori;
        } else if (lower == "x-forwarded-prefix") {
            ++xfp;
        } else if (lower == "x-forwarded-web-client-basepath") {
            ++xfb;
        } else if (lower == "wrdvpn_upstream_ip") {
            ++upstream;
        } else if (lower == "heartbeat" || lower == "refresh" || lower == "route" || lower == "show_faq" ||
                   lower == "show_vpn" || lower.rfind("wengine_", 0) == 0) {
            ++vpn;
        }
        if (next == std::string::npos) break;
        pos = next + 1;
    }
    const auto known = ctk + rfk + hydra + idt + state + csrf + ori + xfp + xfb + upstream + vpn;
    return "total=" + std::to_string(total) +
           ",ctk=" + std::to_string(ctk) +
           ",rfk=" + std::to_string(rfk) +
           ",hydra=" + std::to_string(hydra) +
           ",idt=" + std::to_string(idt) +
           ",state=" + std::to_string(state) +
           ",csrf=" + std::to_string(csrf) +
           ",ori=" + std::to_string(ori) +
           ",xfp=" + std::to_string(xfp) +
           ",xfb=" + std::to_string(xfb) +
           ",upstream=" + std::to_string(upstream) +
           ",vpn=" + std::to_string(vpn) +
           ",other=" + std::to_string(total >= known ? total - known : 0);
}

bool html_url_delimiter(char ch) {
    const auto uch = static_cast<unsigned char>(ch);
    return std::isspace(uch) || ch == '"' || ch == '\'' || ch == '<' || ch == '>' || ch == ')' || ch == '}' || ch == ']';
}

void replace_all(std::string &text, std::string_view from, std::string_view to) {
    if (from.empty()) return;
    for (std::size_t pos = text.find(from); pos != std::string::npos; pos = text.find(from, pos + to.size())) {
        text.replace(pos, from.size(), to);
    }
}

std::string normalize_html_url_text(std::string text) {
    replace_all(text, "\\\\/", "/");
    replace_all(text, "\\/", "/");
    replace_all(text, "\\u002F", "/");
    replace_all(text, "\\u002f", "/");
    replace_all(text, "\\u003A", ":");
    replace_all(text, "\\u003a", ":");
    replace_all(text, "\\u003F", "?");
    replace_all(text, "\\u003f", "?");
    replace_all(text, "\\u003D", "=");
    replace_all(text, "\\u003d", "=");
    replace_all(text, "\\u0026", "&");
    replace_all(text, "&amp;", "&");
    replace_all(text, "&#38;", "&");
    return text;
}

std::string percent_decode_url_text(std::string_view encoded) {
    std::string decoded;
    decoded.reserve(encoded.size());
    for (std::size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            auto hex = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return -1;
            };
            const auto hi = hex(encoded[i + 1]);
            const auto lo = hex(encoded[i + 2]);
            if (hi >= 0 && lo >= 0) {
                decoded.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        decoded.push_back(encoded[i]);
    }
    return decoded;
}

std::optional<std::string> callback_url_from_text(std::string_view text, const std::string &base_url) {
    static constexpr std::array<std::string_view, 2> markers{
        "https://bhpan.buaa.edu.cn/anyshare/oauth2/login/callback",
        "/anyshare/oauth2/login/callback",
    };
    for (auto marker : markers) {
        for (std::size_t pos = text.find(marker); pos != std::string_view::npos; pos = text.find(marker, pos + marker.size())) {
            auto end = pos + marker.size();
            while (end < text.size() && !html_url_delimiter(text[end])) ++end;
            auto candidate = std::string{text.substr(pos, end - pos)};
            while (!candidate.empty() && (candidate.back() == ',' || candidate.back() == ';' || candidate.back() == '.')) {
                candidate.pop_back();
            }
            auto resolved = candidate.rfind("/", 0) == 0 ? "https://" + std::string{kCloudHost} + candidate
                                                        : VpnCipher::from_vpn_url(resolve_location(base_url, candidate));
            if (allowed_cloud_redirect(resolved) && url_path(resolved).rfind("/anyshare/oauth2/login/callback", 0) == 0) {
                return resolved;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> callback_url_from_html(const std::string &html, const std::string &base_url) {
    auto normalized = normalize_html_url_text(html);
    if (auto callback = callback_url_from_text(normalized, base_url)) return callback;
    auto decoded = normalize_html_url_text(percent_decode_url_text(normalized));
    return callback_url_from_text(decoded, base_url);
}

std::size_t count_case_insensitive(std::string text, std::string_view marker) {
    if (marker.empty()) return 0;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::string needle{marker};
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::size_t count = 0;
    for (std::size_t pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + needle.size())) {
        ++count;
    }
    return count;
}

bool contains_case_insensitive(std::string text, std::string_view marker) {
    return count_case_insensitive(std::move(text), marker) > 0;
}

std::string lower_copy_local(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string safe_control_type(std::string type) {
    type = lower_copy_local(std::move(type));
    if (type.empty()) return "text";
    for (char ch : type) {
        const auto uch = static_cast<unsigned char>(ch);
        if (!std::isalnum(uch) && ch != '-' && ch != '_') return "other";
    }
    if (type == "password") return "credential";
    if (type == "file") return "upload";

    static constexpr std::array<std::string_view, 22> kSafeTypes{
        "text", "hidden", "checkbox", "radio", "submit", "button", "reset", "email", "number", "search",
        "tel", "url", "date", "datetime-local", "month", "time", "week", "color", "range", "image",
        "select", "textarea",
    };
    for (auto safe_type : kSafeTypes) {
        if (type == safe_type) return type;
    }
    return "other";
}

std::string control_name_category(const std::map<std::string, std::string> &attrs) {
    auto name_it = attrs.find("name");
    if (name_it == attrs.end() || name_it->second.empty()) return "none";
    const auto name = lower_copy_local(name_it->second);
    if (name.find("login") != std::string::npos && name.find("challenge") != std::string::npos) return "loginChallenge";
    if (name.find("csrf") != std::string::npos || name.find("xsrf") != std::string::npos) return "csrf";
    if (name.find("scope") != std::string::npos) return "scope";
    if (name.find("redirect") != std::string::npos || name.find("return") != std::string::npos) return "redirect";
    if (name.find("client") != std::string::npos) return "client";
    if (name.find("state") != std::string::npos) return "state";
    if (name.find("consent") != std::string::npos || name.find("grant") != std::string::npos) return "consent";
    if (name.find("submit") != std::string::npos || name.find("approve") != std::string::npos || name.find("authorize") != std::string::npos) return "submit";
    return "other";
}

std::string html_marker_summary(const std::string &html) {
    const auto bucket = html.size() < 1024 ? "lt1k" : (html.size() < 8192 ? "lt8k" : "ge8k");
    std::ostringstream out;
    out << "len=" << bucket
        << ",form=" << count_case_insensitive(html, "<form")
        << ",input=" << count_case_insensitive(html, "<input")
        << ",button=" << count_case_insensitive(html, "<button")
        << ",action=" << count_case_insensitive(html, "action=")
        << ",method=" << count_case_insensitive(html, "method=")
        << ",hiddenType=" << count_case_insensitive(html, "type=\"hidden") + count_case_insensitive(html, "type='hidden") + count_case_insensitive(html, "type=hidden")
        << ",checkboxType=" << count_case_insensitive(html, "type=\"checkbox") + count_case_insensitive(html, "type='checkbox") + count_case_insensitive(html, "type=checkbox")
        << ",radioType=" << count_case_insensitive(html, "type=\"radio") + count_case_insensitive(html, "type='radio") + count_case_insensitive(html, "type=radio")
        << ",submitType=" << count_case_insensitive(html, "type=\"submit") + count_case_insensitive(html, "type='submit") + count_case_insensitive(html, "type=submit")
        << ",nameAttr=" << count_case_insensitive(html, "name=")
        << ",valueAttr=" << count_case_insensitive(html, "value=")
        << ",checked=" << count_case_insensitive(html, "checked")
        << ",disabled=" << count_case_insensitive(html, "disabled")
        << ",scriptTag=" << count_case_insensitive(html, "<script")
        << ",csrf=" << (contains_case_insensitive(html, "csrf") ? "1" : "0")
        << ",scope=" << (contains_case_insensitive(html, "scope") ? "1" : "0")
        << ",authorize=" << (contains_case_insensitive(html, "authorize") || contains_case_insensitive(html, "授权") ? "1" : "0")
        << ",approve=" << (contains_case_insensitive(html, "approve") || contains_case_insensitive(html, "同意") || contains_case_insensitive(html, "允许") ? "1" : "0")
        << ",deny=" << (contains_case_insensitive(html, "deny") || contains_case_insensitive(html, "拒绝") ? "1" : "0")
        << ",formaction=" << count_case_insensitive(html, "formaction")
        << ",formmethod=" << count_case_insensitive(html, "formmethod")
        << ",loginChallenge=" << (contains_case_insensitive(html, "login_challenge") ? "1" : "0")
        << ",callback=" << (contains_case_insensitive(html, "callback") ? "1" : "0")
        << ",windowLocation=" << count_case_insensitive(html, "window.location")
        << ",locationHref=" << count_case_insensitive(html, "location.href")
        << ",locationAssign=" << count_case_insensitive(html, "location.assign")
        << ",locationReplace=" << count_case_insensitive(html, "location.replace")
        << ",decode=" << (contains_case_insensitive(html, "decodeURI") || contains_case_insensitive(html, "decodeURIComponent") || contains_case_insensitive(html, "unescape(") ? "1" : "0")
        << ",ajax=" << (contains_case_insensitive(html, "XMLHttpRequest") || contains_case_insensitive(html, "fetch(") || contains_case_insensitive(html, "axios") || contains_case_insensitive(html, "$.ajax") ? "1" : "0")
        << ",submitJs=" << (contains_case_insensitive(html, ".submit") || contains_case_insensitive(html, "submit(") || contains_case_insensitive(html, "onsubmit") ? "1" : "0")
        << ",clickJs=" << (contains_case_insensitive(html, "onclick") || contains_case_insensitive(html, "addEventListener") || contains_case_insensitive(html, "click(") ? "1" : "0")
        << ",required=" << (contains_case_insensitive(html, "required") ? "1" : "0")
        << ",metaRefresh=" << (contains_case_insensitive(html, "refresh") ? "1" : "0")
        << ",hydra=" << (contains_case_insensitive(html, "hydra") ? "1" : "0")
        << ",consent=" << (contains_case_insensitive(html, "consent") ? "1" : "0")
        << ",oauth2=" << count_case_insensitive(html, "oauth2")
        << ",ctkCount=" << count_case_insensitive(html, "client.oauth2_token")
        << ",otkCount=" << count_case_insensitive(html, "oauth2_token")
        << ",atkCount=" << count_case_insensitive(html, "access_token") + count_case_insensitive(html, "accessToken")
        << ",rfkCount=" << count_case_insensitive(html, "refreshToken") + count_case_insensitive(html, "refresh_token")
        << ",docCk=" << count_case_insensitive(html, "document.cookie")
        << ",lsC=" << count_case_insensitive(html, "localStorage")
        << ",ssC=" << count_case_insensitive(html, "sessionStorage")
        << ",setIt=" << count_case_insensitive(html, "setItem");
    return out.str();
}

struct HtmlForm {
    std::string method = "get";
    std::string action;
    std::vector<std::pair<std::string, std::string>> fields;
    std::vector<std::string> field_kinds;
    std::vector<std::string> control_markers;
    std::size_t hidden_count = 0;
    std::size_t submit_count = 0;
    std::optional<std::string> csrf_header_value;
    bool has_text_input = false;
    bool has_credential_input = false;
};

void add_control_marker(HtmlForm &form, std::string_view tag, const std::string &type, const std::map<std::string, std::string> &attrs, std::string_view result) {
    if (form.control_markers.size() >= 12) return;
    const auto safe_type = safe_control_type(type);
    std::ostringstream marker;
    marker << tag << ":type=" << safe_type
           << ":name=" << control_name_category(attrs)
           << ":value=" << (attrs.find("value") == attrs.end() ? "none" : (attrs.at("value").empty() ? "empty" : "set"))
           << ":checked=" << (attrs.find("checked") == attrs.end() ? "0" : "1")
           << ":disabled=" << (attrs.find("disabled") == attrs.end() ? "0" : "1")
           << ":" << result;
    form.control_markers.push_back(marker.str());
}

bool looks_like_credential_field(const std::string &type) {
    return safe_control_type(type) == "credential";
}

bool looks_like_user_field(const std::string &type, const std::map<std::string, std::string> &attrs) {
    if (safe_control_type(type) != "text") return false;
    auto text_for = [](const std::map<std::string, std::string> &values, std::string_view key) -> std::string {
        auto it = values.find(std::string{key});
        if (it == values.end()) return {};
        return lower_copy_local(it->second);
    };
    const auto name = text_for(attrs, "name");
    const auto id = text_for(attrs, "id");
    const auto autocomplete = text_for(attrs, "autocomplete");
    const auto placeholder = text_for(attrs, "placeholder");
    const auto combined = name + " " + id + " " + autocomplete + " " + placeholder;
    return combined.find("user") != std::string::npos ||
           combined.find("account") != std::string::npos ||
           combined.find("login") != std::string::npos ||
           combined.find("email") != std::string::npos ||
           combined.find("name") != std::string::npos ||
           combined.find("student") != std::string::npos ||
           combined.find("school") != std::string::npos ||
           combined.find("uid") != std::string::npos ||
           combined.find("uname") != std::string::npos;
}

bool fill_first_credential_fields(HtmlForm &form, const CloudLoginCredentials *credentials) {
    if (!credentials || credentials->username.empty() || credentials->password.empty()) return false;
    std::size_t user_index = form.fields.size();
    std::size_t credential_index = form.fields.size();
    auto field_kind = [&form](std::size_t index) -> std::string {
        if (index < form.field_kinds.size()) return form.field_kinds[index];
        return {};
    };
    for (std::size_t i = 0; i < form.fields.size(); ++i) {
        if (!form.fields[i].second.empty()) continue;
        const auto kind = field_kind(i);
        const auto lower_name = lower_copy_local(form.fields[i].first);
        const auto name_looks_credential = lower_name.find("pass") != std::string::npos || lower_name.find("pwd") != std::string::npos;
        const auto name_looks_user = lower_name.find("user") != std::string::npos || lower_name.find("account") != std::string::npos ||
                                     lower_name.find("login") != std::string::npos || lower_name.find("email") != std::string::npos ||
                                     lower_name.find("student") != std::string::npos || lower_name.find("school") != std::string::npos ||
                                     lower_name.find("uid") != std::string::npos || lower_name.find("uname") != std::string::npos;
        if (credential_index == form.fields.size() && (kind == "credential" || name_looks_credential)) {
            credential_index = i;
            continue;
        }
        if (user_index == form.fields.size() && !name_looks_credential && (kind == "text" || kind == "email" || name_looks_user)) {
            user_index = i;
        }
    }
    if (user_index == form.fields.size() && form.has_credential_input) {
        for (std::size_t i = 0; i < form.fields.size(); ++i) {
            if (i != credential_index && form.fields[i].second.empty()) {
                const auto kind = field_kind(i);
                if (kind != "hidden" && kind != "checkbox" && kind != "radio" && kind != "submit" && kind != "button" && kind != "image") {
                    user_index = i;
                    break;
                }
            }
        }
    }
    if (credential_index == form.fields.size() && form.has_credential_input) {
        for (std::size_t i = 0; i < form.fields.size(); ++i) {
            if (i != user_index && form.fields[i].second.empty()) {
                credential_index = i;
                break;
            }
        }
    }
    if (user_index == form.fields.size() || credential_index == form.fields.size() || user_index == credential_index) return false;
    form.fields[user_index].second = credentials->username;
    form.fields[credential_index].second = credentials->password;
    return true;
}

std::string join_control_markers(const HtmlForm &form) {
    std::string result;
    for (const auto &marker : form.control_markers) {
        if (!result.empty()) result += '|';
        result += marker;
    }
    return result;
}

std::string html_unescape(std::string text) {
    replace_all(text, "&amp;", "&");
    replace_all(text, "&#38;", "&");
    replace_all(text, "&quot;", "\"");
    replace_all(text, "&#34;", "\"");
    replace_all(text, "&#x22;", "\"");
    replace_all(text, "&#X22;", "\"");
    replace_all(text, "&#39;", "'");
    replace_all(text, "&#x27;", "'");
    replace_all(text, "&#X27;", "'");
    replace_all(text, "&apos;", "'");
    replace_all(text, "&lt;", "<");
    replace_all(text, "&gt;", ">");
    return text;
}

std::map<std::string, std::string> html_attrs(std::string_view attrs_text) {
    std::map<std::string, std::string> attrs;
    static const std::regex attr_re(R"ATTR(([a-zA-Z_:][-a-zA-Z0-9_:.]*)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s"'>]+)))?)ATTR", std::regex::icase);
    const std::string attrs_string{attrs_text};
    auto begin = std::sregex_iterator(attrs_string.begin(), attrs_string.end(), attr_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        auto name = (*it)[1].str();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        std::string value;
        if ((*it)[2].matched) {
            value = (*it)[2].str();
        } else if ((*it)[3].matched) {
            value = (*it)[3].str();
        } else if ((*it)[4].matched) {
            value = (*it)[4].str();
        }
        attrs[name] = html_unescape(std::move(value));
    }
    return attrs;
}

std::string html_script_src_summary(const std::string &html, const std::string &base_url) {
    static const std::regex script_re(R"(<script\b([^>]*)>)", std::regex::icase);
    std::vector<std::string> parts;
    auto begin = std::sregex_iterator(html.begin(), html.end(), script_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end && parts.size() < 8; ++it) {
        const auto attrs = html_attrs((*it)[1].str());
        auto src_it = attrs.find("src");
        if (src_it == attrs.end() || src_it->second.empty()) continue;
        auto resolved = VpnCipher::from_vpn_url(resolve_location(base_url, src_it->second));
        const auto host = extract_host(resolved);
        auto safe_path = path_from_url(resolved);
        if (safe_path.size() > 96) safe_path = safe_path.substr(0, 96) + "...";
        if (host == kCloudHost) {
            parts.push_back("cloud:" + safe_path);
        } else if (host == kVpnHost) {
            parts.push_back("vpn:" + safe_path);
        } else if (!host.empty()) {
            parts.push_back("other:" + safe_path);
        } else {
            parts.push_back("relative:" + safe_path);
        }
    }
    if (parts.empty()) return {};
    std::string summary;
    for (const auto &part : parts) {
        if (!summary.empty()) summary += '|';
        summary += part;
    }
    return summary;
}

std::optional<std::string> csrf_token_from_meta(const std::string &html) {
    static const std::regex meta_re(R"(<meta\b([^>]*)>)", std::regex::icase);
    auto begin = std::sregex_iterator(html.begin(), html.end(), meta_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        const auto attrs = html_attrs((*it)[1].str());
        auto content_it = attrs.find("content");
        if (content_it == attrs.end() || content_it->second.empty()) continue;
        std::string name;
        if (auto name_it = attrs.find("name"); name_it != attrs.end()) name += lower_copy_local(name_it->second);
        if (auto property_it = attrs.find("property"); property_it != attrs.end()) {
            if (!name.empty()) name.push_back(' ');
            name += lower_copy_local(property_it->second);
        }
        if (name.find("csrf") != std::string::npos && name.find("token") != std::string::npos) {
            return content_it->second;
        }
    }
    return std::nullopt;
}

std::optional<std::string> javascript_location_url_from_text(const std::string &text, const std::string &base_url) {
    static const std::regex location_re(R"JS((?:window\.)?location(?:\.(?:href|assign|replace))?\s*(?:=|\()\s*["']([^"']+)["'])JS", std::regex::icase);
    static const std::regex decoded_location_re(R"JS((?:window\.)?location(?:\.(?:href|assign|replace))?\s*(?:=|\()\s*(?:decodeURI|decodeURIComponent|unescape)\(\s*["']([^"']+)["'])JS", std::regex::icase);
    auto resolve_candidate = [&base_url](std::string candidate) -> std::optional<std::string> {
        candidate = html_unescape(std::move(candidate));
        if (candidate.empty() || candidate[0] == '#') return std::nullopt;
        std::string lower = candidate;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (lower.rfind("javascript:", 0) == 0 || lower.rfind("about:", 0) == 0) return std::nullopt;
        auto resolved = VpnCipher::from_vpn_url(resolve_location(base_url, candidate));
        if (allowed_cloud_redirect(resolved)) return resolved;
        return std::nullopt;
    };

    for (const auto *pattern : {&decoded_location_re, &location_re}) {
        auto begin = std::sregex_iterator(text.begin(), text.end(), *pattern);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            if (auto resolved = resolve_candidate((*it)[1].str())) return resolved;
        }
    }
    return std::nullopt;
}

std::optional<std::string> javascript_location_url_from_html(const std::string &html, const std::string &base_url) {
    auto normalized = normalize_html_url_text(html_unescape(html));
    if (auto target = javascript_location_url_from_text(normalized, base_url)) return target;
    auto decoded = normalize_html_url_text(percent_decode_url_text(normalized));
    return javascript_location_url_from_text(decoded, base_url);
}

bool is_oauth_form_action(const std::string &action) {
    const auto logical = VpnCipher::from_vpn_url(action);
    if (extract_host(logical) != kCloudHost) return false;
    const auto path = url_path(logical);
    return path == "/oauth2/signin" || path.rfind("/oauth2/", 0) == 0 || path.rfind("/anyshare/oauth2/", 0) == 0;
}

std::string normalized_form_method(std::string method) {
    std::transform(method.begin(), method.end(), method.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return method == "post" ? "post" : "get";
}

void apply_submit_overrides(HtmlForm &form, const std::map<std::string, std::string> &attrs, const std::string &base_url) {
    if (auto action_it = attrs.find("formaction"); action_it != attrs.end() && !action_it->second.empty()) {
        auto action = VpnCipher::from_vpn_url(resolve_location(base_url, html_unescape(action_it->second)));
        if (allowed_cloud_redirect(action) && is_oauth_form_action(action)) {
            form.action = std::move(action);
        }
    }
    if (auto method_it = attrs.find("formmethod"); method_it != attrs.end() && !method_it->second.empty()) {
        form.method = normalized_form_method(method_it->second);
    }
}

std::optional<HtmlForm> oauth_signin_form_from_html(const std::string &html, const std::string &base_url) {
    static const std::regex form_re(R"(<form\b([^>]*)>([\s\S]*?)</form>)", std::regex::icase);
    static const std::regex input_re(R"(<input\b([^>]*)>)", std::regex::icase);
    static const std::regex button_re(R"(<button\b([^>]*)>([\s\S]*?)</button>)", std::regex::icase);
    std::optional<HtmlForm> best_form;
    const auto csrf_header_value = csrf_token_from_meta(html);
    int best_score = -1;
    auto begin = std::sregex_iterator(html.begin(), html.end(), form_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        auto form_attrs = html_attrs((*it)[1].str());
        auto action_it = form_attrs.find("action");
        auto action = action_it == form_attrs.end() ? base_url : resolve_location(base_url, html_unescape(action_it->second));
        action = VpnCipher::from_vpn_url(action);
        if (!allowed_cloud_redirect(action) || !is_oauth_form_action(action)) {
            continue;
        }

        HtmlForm form;
        auto method_it = form_attrs.find("method");
        if (method_it != form_attrs.end() && !method_it->second.empty()) {
            form.method = normalized_form_method(method_it->second);
        }
        form.action = action;
        form.csrf_header_value = csrf_header_value;
        bool submit_added = false;
        bool submit_control_selected = false;
        const auto body = (*it)[2].str();
        auto input_begin = std::sregex_iterator(body.begin(), body.end(), input_re);
        for (auto input_it = input_begin; input_it != end; ++input_it) {
            const auto attrs = html_attrs((*input_it)[1].str());
            auto type_it = attrs.find("type");
            auto type = type_it == attrs.end() ? std::string{} : normalized_form_method(type_it->second);
            if (type_it != attrs.end()) {
                type = type_it->second;
                std::transform(type.begin(), type.end(), type.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            }
            if (type == "button" || type == "file") {
                add_control_marker(form, "input", type, attrs, "skip-unsupported");
                continue;
            }
            if (attrs.find("disabled") != attrs.end()) {
                add_control_marker(form, "input", type, attrs, "skip-disabled");
                continue;
            }
            auto name_it = attrs.find("name");
            if (type == "submit" || type == "image") {
                ++form.submit_count;
                if (!submit_control_selected) {
                    apply_submit_overrides(form, attrs, base_url);
                    submit_control_selected = true;
                }
                if (type == "submit" && !submit_added && name_it != attrs.end() && !name_it->second.empty()) {
                    auto value_it = attrs.find("value");
                    const auto value = value_it == attrs.end() ? std::string{} : value_it->second;
                    form.fields.push_back({name_it->second, value});
                    form.field_kinds.push_back(safe_control_type(type));
                    submit_added = true;
                    add_control_marker(form, "input", type, attrs, "submit-added");
                } else {
                    add_control_marker(form, "input", type, attrs, "submit-selected");
                }
                continue;
            }
            if (name_it == attrs.end() || name_it->second.empty()) {
                add_control_marker(form, "input", type, attrs, "skip-no-name");
                continue;
            }
            if (looks_like_credential_field(type)) form.has_credential_input = true;
            if (looks_like_user_field(type, attrs)) form.has_text_input = true;
            auto value_it = attrs.find("value");
            auto value = value_it == attrs.end() ? std::string{} : value_it->second;
            if (type == "hidden") ++form.hidden_count;
            if ((type == "checkbox" || type == "radio") && attrs.find("checked") == attrs.end()) {
                add_control_marker(form, "input", type, attrs, "skip-unchecked");
                continue;
            }
            if (type == "checkbox" && value.empty()) value = "on";
            form.fields.push_back({name_it->second, value});
            form.field_kinds.push_back(safe_control_type(type));
            add_control_marker(form, "input", type, attrs, "field-added");
        }
        auto button_begin = std::sregex_iterator(body.begin(), body.end(), button_re);
        for (auto button_it = button_begin; button_it != end; ++button_it) {
            const auto attrs = html_attrs((*button_it)[1].str());
            auto type_it = attrs.find("type");
            auto type = type_it == attrs.end() ? std::string{"submit"} : type_it->second;
            std::transform(type.begin(), type.end(), type.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (attrs.find("disabled") != attrs.end()) {
                add_control_marker(form, "button", type, attrs, "skip-disabled");
                continue;
            }
            if (type != "submit") {
                add_control_marker(form, "button", type, attrs, "skip-nonsubmit");
                continue;
            }
            ++form.submit_count;
            if (!submit_control_selected) {
                apply_submit_overrides(form, attrs, base_url);
                submit_control_selected = true;
            }
            auto name_it = attrs.find("name");
            if (name_it == attrs.end() || name_it->second.empty() || submit_added) {
                add_control_marker(form, "button", type, attrs, "submit-selected");
                continue;
            }
            auto value_it = attrs.find("value");
            const auto value = value_it == attrs.end() ? std::string{} : value_it->second;
            form.fields.push_back({name_it->second, value});
            form.field_kinds.push_back(safe_control_type(type));
            submit_added = true;
            add_control_marker(form, "button", type, attrs, "submit-added");
        }

        int score = static_cast<int>(form.fields.size()) + static_cast<int>(form.hidden_count) * 2 + static_cast<int>(form.submit_count);
        if (form.method == "post") score += 100;
        if (url_path(form.action) != url_path(base_url)) score += 20;
        if (!best_form || score > best_score) {
            best_score = score;
            best_form = std::move(form);
        }
    }
    return best_form;
}

std::string form_body(const std::vector<std::pair<std::string, std::string>> &fields) {
    std::string body;
    for (const auto &[name, value] : fields) {
        if (!body.empty()) body += '&';
        body += Protocol::form_url_encode(name);
        body += '=';
        body += Protocol::form_url_encode(value);
    }
    return body;
}

std::vector<std::pair<std::string, std::string>> form_fields_with_login_challenge(
    std::vector<std::pair<std::string, std::string>> fields,
    const std::string &login_challenge,
    const std::string &action) {
    (void)login_challenge;
    (void)action;
    return fields;
}

struct AnyshareSigninPage {
    std::string csrf_token;
    std::string challenge;
    nlohmann::json device = nlohmann::json::object();
};

std::optional<std::string> next_data_json_from_html(const std::string &html) {
    static const std::regex script_re(R"(<script\b([^>]*)>([\s\S]*?)</script>)", std::regex::icase);
    auto begin = std::sregex_iterator(html.begin(), html.end(), script_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        const auto attrs = html_attrs((*it)[1].str());
        auto id_it = attrs.find("id");
        if (id_it == attrs.end() || id_it->second != "__NEXT_DATA__") continue;
        auto body = (*it)[2].str();
        while (!body.empty() && std::isspace(static_cast<unsigned char>(body.front()))) body.erase(body.begin());
        while (!body.empty() && std::isspace(static_cast<unsigned char>(body.back()))) body.pop_back();
        if (!body.empty()) return body;
    }
    return std::nullopt;
}

std::optional<AnyshareSigninPage> anyshare_signin_page_from_html(const std::string &html) {
    auto next_data = next_data_json_from_html(html);
    if (!next_data) return std::nullopt;
    auto root = nlohmann::json::parse(*next_data, nullptr, false);
    if (root.is_discarded()) root = nlohmann::json::parse(html_unescape(*next_data), nullptr, false);
    if (root.is_discarded() || !root.is_object()) return std::nullopt;
    auto props_it = root.find("props");
    if (props_it == root.end() || !props_it->is_object()) return std::nullopt;
    auto page_props_it = props_it->find("pageProps");
    if (page_props_it == props_it->end() || !page_props_it->is_object()) return std::nullopt;
    const auto &page_props = *page_props_it;
    auto string_field = [&page_props](const char *key) -> std::string {
        auto it = page_props.find(key);
        if (it == page_props.end() || !it->is_string()) return {};
        return it->get<std::string>();
    };

    AnyshareSigninPage page;
    page.csrf_token = string_field("csrftoken");
    if (page.csrf_token.empty()) page.csrf_token = string_field("csrfToken");
    page.challenge = string_field("challenge");
    auto device_it = page_props.find("device");
    if (device_it != page_props.end() && device_it->is_object()) page.device = *device_it;
    if (page.csrf_token.empty() || page.challenge.empty()) return std::nullopt;
    return page;
}

std::optional<std::string> redirect_from_json_value(const nlohmann::json &value) {
    if (value.is_object()) {
        static constexpr std::array<std::string_view, 3> keys{"redirect", "redirectUrl", "redirect_url"};
        for (auto key : keys) {
            auto it = value.find(std::string{key});
            if (it != value.end() && it->is_string() && !it->get<std::string>().empty()) return it->get<std::string>();
        }
        for (const auto &item : value.items()) {
            if (auto redirect = redirect_from_json_value(item.value())) return redirect;
        }
    } else if (value.is_array()) {
        for (const auto &item : value) {
            if (auto redirect = redirect_from_json_value(item)) return redirect;
        }
    }
    return std::nullopt;
}

std::optional<std::string> anyshare_signin_redirect_from_json(const std::string &body, const std::string &base_url) {
    auto root = nlohmann::json::parse(body, nullptr, false);
    if (root.is_discarded()) return std::nullopt;
    auto redirect = redirect_from_json_value(root);
    if (!redirect) return std::nullopt;
    auto candidate = html_unescape(*redirect);
    if (candidate.empty()) return std::nullopt;
    std::string resolved;
    if (candidate.rfind("/http/", 0) == 0 || candidate.rfind("/https/", 0) == 0) {
        resolved = VpnCipher::from_vpn_url("https://" + std::string{kVpnHost} + candidate);
    } else {
        resolved = VpnCipher::from_vpn_url(resolve_location(VpnCipher::from_vpn_url(base_url), candidate));
    }
    if (!allowed_cloud_redirect(resolved)) return std::nullopt;
    return resolved;
}

Result<HttpRequest> make_anyshare_signin_json_request(const AnyshareSigninPage &page,
                                                      ConnectionMode mode,
                                                      ICookieStore *cookie_store,
                                                      const std::string &login_challenge,
                                                      const std::string &referer,
                                                      ICryptoProvider &crypto,
                                                      const CloudLoginCredentials *credentials) {
    if (!credentials || credentials->username.empty() || credentials->password.empty()) {
        return make_error(ErrorCode::AuthFailed, "北航云盘 OAuth2 signin 需要已保存的登录凭据，请先重新登录");
    }
    const std::vector<unsigned char> password_bytes(credentials->password.begin(), credentials->password.end());
    auto encrypted_password = crypto.rsa_pkcs1_encrypt_base64(password_bytes, kAnyshareSigninPublicKeyDerBase64);
    if (!encrypted_password) {
        return make_error(encrypted_password.error().code, "北航云盘 OAuth2 密码加密失败: " + redacted_error_message(encrypted_password.error().message));
    }

    const std::string action = "https://bhpan.buaa.edu.cn/oauth2/signin";
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(action, mode);
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["Content-Type"] = "application/json";
    request.headers["User-Agent"] = kUserAgent;
    const auto actual_origin = origin_from_url(request.url);
    if (!actual_origin.empty()) request.headers["Origin"] = actual_origin;
    if (!referer.empty()) request.headers["Referer"] = resolve_for_mode(referer, mode);
    apply_cookie_header(request, html_cookie_header(cookie_store, action, mode, login_challenge));
    request.body = nlohmann::json{
        {"_csrf", page.csrf_token},
        {"challenge", page.challenge},
        {"account", credentials->username},
        {"password", *encrypted_password},
        {"vcode", nlohmann::json{{"id", ""}, {"content", ""}}},
        {"dualfactorauthinfo", nlohmann::json{{"validcode", nlohmann::json{{"vcode", ""}}}, {"OTP", nlohmann::json{{"OTP", ""}}}}},
        {"remember", false},
        {"device", page.device.is_object() ? page.device : nlohmann::json::object()},
    }.dump();
    disable_redirects(request);
    return request;
}

HttpRequest make_html_form_request(const HtmlForm &form,
                                   ConnectionMode mode,
                                   ICookieStore *cookie_store,
                                   const std::string &login_challenge,
                                   const std::string &referer,
                                   const CloudLoginCredentials *credentials = nullptr) {
    HttpRequest request;
    auto submit_form = form;
    const bool credentials_filled = fill_first_credential_fields(submit_form, credentials);
    auto fields = form_fields_with_login_challenge(submit_form.fields, login_challenge, submit_form.action);
    if (form.method == "post" || credentials_filled) {
        request.method = HttpMethod::Post;
        request.url = resolve_for_mode(form.action, mode);
        request.body = form_body(fields);
        request.headers["Content-Type"] = "application/x-www-form-urlencoded";
        if (submit_form.csrf_header_value && !submit_form.csrf_header_value->empty()) {
            request.headers["X-CSRF-Token"] = *submit_form.csrf_header_value;
        }
        const auto logical_origin = origin_from_url(form.action);
        if (!logical_origin.empty()) request.headers["Origin"] = logical_origin;
    } else {
        request.method = HttpMethod::Get;
        request.url = resolve_for_mode(append_query(form.action, fields), mode);
    }
    request.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";
    request.headers["User-Agent"] = kUserAgent;
    if (!referer.empty()) request.headers["Referer"] = resolve_for_mode(referer, mode);
    apply_cookie_header(request, html_cookie_header(cookie_store, form.action, mode, login_challenge));
    disable_redirects(request);
    return request;
}

std::string url_encode_component(const std::string &value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());
    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded.push_back(static_cast<char>(c));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[c >> 4]);
            encoded.push_back(hex[c & 0x0F]);
        }
    }
    return encoded;
}

std::string append_raw_query_parameter(const std::string &url, std::string_view key, const std::string &raw_value) {
    std::string result = url;
    result.push_back(url.find('?') == std::string::npos ? '?' : '&');
    result += url_encode_component(std::string{key});
    result.push_back('=');
    result += raw_value;
    return result;
}

std::optional<std::string> raw_query_parameter_from_url(const std::string &url, std::string_view name) {
    const auto logical = VpnCipher::from_vpn_url(url);
    const auto query_start = logical.find('?');
    if (query_start == std::string::npos) return std::nullopt;
    const auto query_end = logical.find('#', query_start + 1);
    const auto query = std::string_view{logical}.substr(query_start + 1, query_end == std::string::npos ? std::string_view::npos : query_end - query_start - 1);
    for (std::size_t pos = 0; pos < query.size();) {
        auto next = query.find('&', pos);
        const auto part = query.substr(pos, next == std::string_view::npos ? std::string_view::npos : next - pos);
        const auto eq = part.find('=');
        const auto key = part.substr(0, eq == std::string_view::npos ? part.size() : eq);
        if (key == name) {
            if (eq == std::string_view::npos) return std::string{};
            return std::string{part.substr(eq + 1)};
        }
        if (next == std::string_view::npos) break;
        pos = next + 1;
    }
    return std::nullopt;
}

std::string login_challenge_from_url(const std::string &url) {
    if (auto raw = raw_query_parameter_from_url(url, kCloudLoginChallengeCookie)) return *raw;
    return Protocol::extract_query_parameter_anywhere(url, kCloudLoginChallengeCookie);
}

std::string cloud_sso_url(ConnectionMode mode, const std::string &login_challenge) {
    auto service = std::string{"https://bhpan.buaa.edu.cn/oauth2/signin"};
    if (!login_challenge.empty()) {
        service = append_raw_query_parameter(service, kCloudLoginChallengeCookie, login_challenge);
    }
    (void)mode;
    return "https://sso.buaa.edu.cn/login?service=" + url_encode_component(service);
}

std::string append_query(const std::string &url, const std::vector<std::pair<std::string, std::string>> &query) {
    if (query.empty()) return url;
    std::string result = url;
    result.push_back(url.find('?') == std::string::npos ? '?' : '&');
    bool first = true;
    for (const auto &[key, value] : query) {
        if (!first) result.push_back('&');
        first = false;
        result += url_encode_component(key);
        result.push_back('=');
        result += url_encode_component(value);
    }
    return result;
}

std::vector<std::pair<std::string, std::string>> root_query(CloudRootKind kind) {
    std::vector<std::pair<std::string, std::string>> query{{"sort", "doc_lib_name"}, {"direction", "asc"}};
    switch (kind) {
    case CloudRootKind::All: break;
    case CloudRootKind::User: query.push_back({"type", "user_doc_lib"}); break;
    case CloudRootKind::Shared: query.push_back({"type", "shared_user_doc_lib"}); break;
    case CloudRootKind::Department: query.push_back({"type", "department_doc_lib"}); break;
    case CloudRootKind::Group: query.push_back({"type", "custom_doc_lib"}); break;
    }
    return query;
}

std::string root_kind_string(CloudRootKind kind) {
    switch (kind) {
    case CloudRootKind::All: return "all";
    case CloudRootKind::User: return "user";
    case CloudRootKind::Shared: return "shared";
    case CloudRootKind::Department: return "department";
    case CloudRootKind::Group: return "group";
    }
    return "all";
}

void apply_cloud_headers(HttpRequest &request, ConnectionMode mode, const std::string &referer = "https://bhpan.buaa.edu.cn/anyshare/zh-cn/portal") {
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["User-Agent"] = kUserAgent;
    request.headers["Referer"] = resolve_for_mode(referer, mode);
}

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.is_object() || !json.contains(key) || json[key].is_null()) return {};
    const auto &value = json[key];
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<long long>());
    if (value.is_number_unsigned()) return std::to_string(value.get<unsigned long long>());
    if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
    return {};
}

std::string first_json_string(const nlohmann::json &json, std::initializer_list<const char *> keys) {
    for (const auto *key : keys) {
        auto value = json_string(json, key);
        if (!value.empty()) return value;
    }
    return {};
}

bool json_bool(const nlohmann::json &json, const char *key, bool fallback = false) {
    if (!json.is_object() || !json.contains(key) || json[key].is_null()) return fallback;
    const auto &value = json[key];
    if (value.is_boolean()) return value.get<bool>();
    if (value.is_string()) {
        const auto text = value.get<std::string>();
        return text == "true" || text == "1";
    }
    if (value.is_number_integer()) return value.get<long long>() != 0;
    return fallback;
}

nlohmann::json cloud_share_payload(const CloudShareRequest &share) {
    nlohmann::json allow = nlohmann::json::array();
    if (share.permission.create) allow.push_back("create");
    if (share.permission.modify) allow.push_back("modify");
    if (share.permission.download) allow.push_back("download");
    if (share.permission.preview) allow.push_back("preview");
    if (share.permission.display) allow.push_back("display");
    return nlohmann::json{
        {"item", {{"id", share.item_id}, {"type", share.is_dir ? "folder" : "file"}, {"allow", allow}}},
        {"title", share.title},
        {"expires_at", share.expires_at.empty() ? "1970-01-01T08:00:00+08:00" : share.expires_at},
        {"password", share.password},
        {"limited_times", share.limit},
    };
}

std::string share_permissions_to_string(const Model::CloudSharePermission &permission) {
    std::vector<std::string> values;
    if (permission.create) values.push_back("create");
    if (permission.modify) values.push_back("modify");
    if (permission.download) values.push_back("download");
    if (permission.preview) values.push_back("preview");
    if (permission.display) values.push_back("display");
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << ',';
        out << values[i];
    }
    return out.str();
}

Model::MutationResult cloud_mutation(std::string id,
                                     std::string message,
                                     std::string status,
                                     std::map<std::string, std::string> fields = {}) {
    Model::MutationResult result;
    result.accepted = true;
    result.message = std::move(message);
    result.summary.id = std::move(id);
    result.summary.title = result.message;
    result.summary.status = std::move(status);
    result.summary.fields = std::move(fields);
    return result;
}

std::string extract_id_from_json(const nlohmann::json &json) {
    if (json.is_object()) {
        auto value = first_json_string(json, {"docid", "id", "gnsId", "gns_id"});
        if (!value.empty()) return value;
        for (const auto *key : {"data", "result", "item"}) {
            if (json.contains(key)) {
                value = extract_id_from_json(json[key]);
                if (!value.empty()) return value;
            }
        }
    }
    return {};
}

std::string unescape_json_url(std::string value) {
    value.erase(std::remove(value.begin(), value.end(), '\\'), value.end());
    return value;
}

std::vector<std::string> parse_authrequest(const nlohmann::json &json, const char *key = "authrequest") {
    const auto *value = &json;
    if (json.is_object() && json.contains(key)) value = &json[key];
    std::vector<std::string> auth;
    if (!value->is_array()) return auth;
    auth.reserve(value->size());
    for (const auto &item : *value) {
        if (item.is_string()) auth.push_back(item.get<std::string>());
    }
    return auth;
}

Result<HttpResponse> send_authrequest(IHttpClient &http_client,
                                      const std::vector<std::string> &authrequest,
                                      HttpMethod fallback_method,
                                      const std::string &body,
                                      const std::string &context) {
    if (authrequest.size() < 2) return make_error(ErrorCode::ParseError, context + "缺少上传授权请求");
    HttpRequest request;
    const auto method = authrequest[0];
    if (method == "POST" || method == "post") {
        request.method = HttpMethod::Post;
    } else if (method == "PUT" || method == "put") {
        request.method = HttpMethod::Put;
    } else if (method == "GET" || method == "get") {
        request.method = HttpMethod::Get;
    } else {
        request.method = fallback_method;
    }
    request.url = authrequest[1];
    request.body = body;
    for (std::size_t i = 2; i < authrequest.size(); ++i) {
        const auto &header = authrequest[i];
        const auto sep = header.find(": ");
        if (sep == std::string::npos) continue;
        request.headers[header.substr(0, sep)] = header.substr(sep + 2);
    }
    auto response = http_client.send(request);
    if (!response) return make_error(response.error().code, context + "请求失败: " + redacted_error_message(response.error().message));
    if (response->status_code < 200 || response->status_code >= 300) {
        return make_error(ErrorCode::NetworkError, context + "请求返回: " + std::to_string(response->status_code));
    }
    return *response;
}

std::string extract_etag(const HttpResponse &response) {
    auto etag = header_value(response, "ETag");
    if (etag.size() >= 2 && etag.front() == '"' && etag.back() == '"') etag = etag.substr(1, etag.size() - 2);
    return etag;
}

std::string share_id_from_url(std::string id_or_url) {
    const auto link_marker = id_or_url.find("/link/");
    if (link_marker != std::string::npos) id_or_url = id_or_url.substr(link_marker + 6);
    const auto stop = id_or_url.find_first_of("?#/");
    if (stop != std::string::npos) id_or_url = id_or_url.substr(0, stop);
    return id_or_url;
}

std::optional<std::string> cookie_from_store(ICookieStore *store, const std::string &host, const std::string &name) {
    if (!store) return std::nullopt;
    if (const auto *current = store->current()) {
        auto value = current->get_cookie(host, name);
        if (value && !value->empty()) return value;
    }
    auto loaded = store->load();
    if (!loaded) return std::nullopt;
    auto value = loaded->get_cookie(host, name);
    if (value && !value->empty()) return value;
    return std::nullopt;
}

std::optional<std::string> cookie_from_set_cookie_header(const HttpResponse &response, const std::string &name) {
    auto header = header_value(response, "Set-Cookie");
    if (header.empty()) return std::nullopt;
    const auto prefix = name + "=";
    auto pos = header.find(prefix);
    if (pos == std::string::npos) return std::nullopt;
    pos += prefix.size();
    auto end = header.find(';', pos);
    auto value = header.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    if (value.empty()) return std::nullopt;
    return value;
}

Model::FeatureRecord download_url_record_for(const Model::CloudDownloadUrl &download) {
    Model::FeatureRecord record;
    record.id = download.name;
    record.title = download.name.empty() ? "Cloud download URL" : download.name;
    record.status = "url";
    record.fields = {
        {"url", download.url},
        {"name", download.name},
        {"zipped", download.zipped ? "true" : "false"},
    };
    return record;
}

std::string extract_download_url(const nlohmann::json &json) {
    if (json.is_object()) {
        auto auth = parse_authrequest(json);
        if (auth.size() > 1) return unescape_json_url(auth[1]);
        auto value = first_json_string(json, {"url", "download_url", "href"});
        if (!value.empty()) return unescape_json_url(value);
        for (const auto *key : {"data", "result"}) {
            if (json.contains(key)) {
                value = extract_download_url(json[key]);
                if (!value.empty()) return value;
            }
        }
    }
    if (json.is_array() && json.size() > 1 && json[1].is_string()) return unescape_json_url(json[1].get<std::string>());
    return {};
}

struct CloudUploadInit {
    std::string docid;
    std::string rev;
    std::string uploadid;
    std::string parts;
};

CloudUploadInit parse_upload_init(const nlohmann::json &json, std::uint64_t length) {
    CloudUploadInit init;
    init.docid = first_json_string(json, {"docid", "id"});
    init.rev = first_json_string(json, {"rev", "revision"});
    init.uploadid = first_json_string(json, {"uploadid", "upload_id"});
    const auto part_count = (length + kCloudUploadPartSize - 1) / kCloudUploadPartSize;
    init.parts = "1-" + std::to_string(part_count);
    return init;
}

nlohmann::json upload_init_payload(const CloudUploadInit &init) {
    return nlohmann::json{{"docid", init.docid}, {"rev", init.rev}, {"uploadid", init.uploadid}, {"parts", init.parts}};
}

std::vector<std::pair<int, std::vector<std::string>>> parse_part_authrequests(const nlohmann::json &json) {
    std::vector<std::pair<int, std::vector<std::string>>> parts;
    if (!json.is_object() || !json.contains("authrequests") || !json["authrequests"].is_object()) return parts;
    for (const auto &[key, value] : json["authrequests"].items()) {
        int index = 0;
        try {
            index = std::stoi(key);
        } catch (...) {
            continue;
        }
        auto auth = parse_authrequest(value, "authrequest");
        if (auth.empty() && value.is_array()) auth = parse_authrequest(value);
        if (auth.empty()) continue;
        parts.push_back({index, std::move(auth)});
    }
    std::sort(parts.begin(), parts.end(), [](const auto &left, const auto &right) { return left.first < right.first; });
    return parts;
}

struct CloudCompleteUploadRequest {
    std::vector<std::string> authrequest;
    std::string body;
};

Result<CloudCompleteUploadRequest> parse_complete_upload_response(const std::string &body) {
    const auto xml_start = body.find('<');
    const auto json_start = body.find('{');
    if (xml_start == std::string::npos || json_start == std::string::npos) {
        return make_error(ErrorCode::ParseError, "北航云盘分片完成响应格式无效");
    }
    auto xml_end = body.find("--", xml_start);
    if (xml_end == std::string::npos || xml_end <= xml_start) return make_error(ErrorCode::ParseError, "北航云盘分片完成 XML 响应缺失");
    auto xml = body.substr(xml_start, xml_end - xml_start);
    while (!xml.empty() && (xml.back() == '\r' || xml.back() == '\n')) xml.pop_back();

    auto json_end = body.find("--", json_start);
    if (json_end == std::string::npos || json_end <= json_start) json_end = body.size();
    auto json_text = body.substr(json_start, json_end - json_start);
    while (!json_text.empty() && (json_text.back() == '\r' || json_text.back() == '\n')) json_text.pop_back();
    auto parsed = nlohmann::json::parse(json_text, nullptr, false);
    if (parsed.is_discarded()) return make_error(ErrorCode::ParseError, "解析北航云盘分片完成授权失败");
    auto auth = parse_authrequest(parsed);
    if (auth.size() < 2) return make_error(ErrorCode::ParseError, "北航云盘分片完成授权缺失");
    return CloudCompleteUploadRequest{std::move(auth), std::move(xml)};
}

Result<std::string> read_upload_bytes(IUploadSource &source, std::uint64_t expected_size) {
    if (expected_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return make_error(ErrorCode::InvalidArgument, "上传分片超过当前平台可分配大小");
    }
    std::string body;
    body.resize(static_cast<std::size_t>(expected_size));
    std::uint64_t offset = 0;
    while (offset < expected_size) {
        auto read = source.read(reinterpret_cast<unsigned char *>(&body[static_cast<std::size_t>(offset)]),
                                static_cast<std::size_t>(expected_size - offset));
        if (!read) return make_error(read.error().code, read.error().message);
        if (*read == 0) return make_error(ErrorCode::InvalidArgument, "上传文件读取提前结束");
        offset += *read;
    }
    return body;
}

std::string json_message(const nlohmann::json &json, const std::string &fallback) {
    auto message = ServiceResponse::envelope_message(json, fallback);
    if (message == fallback && json.contains("cause")) message = json_string(json, "cause");
    if (message.empty()) message = fallback;
    return Security::redact_sensitive_text(message);
}

Result<nlohmann::json> parse_cloud_json(const HttpResponse &response, const std::string &context) {
    if (Protocol::is_session_expired_response(response, {}, true)) return make_error(ErrorCode::SessionExpired, context + "会话已过期，请重新登录");
    if (response.status_code < 200 || response.status_code >= 300) return make_error(ErrorCode::NetworkError, context + "请求返回: " + std::to_string(response.status_code));
    auto root = nlohmann::json::parse(response.body, nullptr, false);
    if (root.is_discarded()) return make_error(ErrorCode::ParseError, "解析" + context + "JSON 失败");
    if (root.is_object()) {
        if (root.contains("success") && root["success"].is_boolean() && !root["success"].get<bool>()) {
            return make_error(ErrorCode::NetworkError, json_message(root, context + "请求失败"));
        }
        if (!ServiceResponse::envelope_ok(root)) {
            return make_error(ErrorCode::NetworkError, json_message(root, context + "请求失败"));
        }
    }
    return ServiceResponse::envelope_data(root);
}

std::string trim_ascii(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

Result<void> accept_cloud_mutation_response(const HttpResponse &response, const std::string &context) {
    if (Protocol::is_session_expired_response(response, {}, true)) return make_error(ErrorCode::SessionExpired, context + "会话已过期，请重新登录");
    if (response.status_code < 200 || response.status_code >= 300) return make_error(ErrorCode::NetworkError, context + "请求返回: " + std::to_string(response.status_code));
    auto body = trim_ascii(response.body);
    if (body.empty() || body == "ok" || body == "OK" || body == "success" || body == "true") return {};
    auto parsed = parse_cloud_json(response, context);
    if (!parsed) return make_error(parsed.error().code, parsed.error().message);
    return {};
}

Model::FeatureRecord item_record(const Model::CloudItem &item, const std::string &status) {
    Model::FeatureRecord record;
    record.id = item.id;
    record.title = item.name.empty() ? item.doc_lib_name : item.name;
    record.status = status;
    record.fields = {
        {"type", item.type},
        {"size", item.size},
        {"parentId", item.parent_id},
        {"docLibId", item.doc_lib_id},
        {"docLibName", item.doc_lib_name},
        {"creator", item.creator},
        {"modifier", item.modifier},
        {"createdAt", item.created_at},
        {"updatedAt", item.updated_at},
        {"revision", item.revision},
        {"hasToken", item.token.empty() ? "false" : "true"},
    };
    return record;
}

Model::FeatureRecord size_record_for(const Model::CloudSize &size, const std::string &doc_id) {
    Model::FeatureRecord record;
    record.id = doc_id;
    record.title = "Cloud item size";
    record.status = "ok";
    record.fields = {
        {"bytes", size.bytes},
        {"fileCount", size.file_count},
        {"dirCount", size.dir_count},
    };
    return record;
}

Model::FeatureRecord share_record_for(const Model::CloudShare &share) {
    Model::FeatureRecord record;
    record.id = share.id;
    record.title = share.name;
    record.status = "shared";
    record.fields = {
        {"url", share.url},
        {"itemId", share.item_id},
        {"isDir", share.is_dir ? "true" : "false"},
        {"expiresAt", share.expires_at},
        {"permissions", share.permissions},
        {"hasPassword", share.password.empty() ? "false" : "true"},
        {"limitedTimes", std::to_string(share.limit)},
    };
    return record;
}

} // namespace

CloudService::CloudService(IHttpClient &http_client, ICookieStore *cookie_store, ICacheStore &cache, ConnectionMode mode,
                           std::optional<CloudLoginCredentials> login_credentials)
    : CloudService(http_client, cookie_store, cache, mode, default_crypto_provider(), std::move(login_credentials)) {}

CloudService::CloudService(IHttpClient &http_client, ICookieStore *cookie_store, ICacheStore &cache, ConnectionMode mode,
                           ICryptoProvider &crypto, std::optional<CloudLoginCredentials> login_credentials)
    : m_http_client(http_client), m_cookie_store(cookie_store), m_cache(cache), m_mode(mode), m_crypto(crypto),
      m_login_credentials(std::move(login_credentials)) {}

Result<std::string> CloudService::token(bool force_refresh) {
    (void)m_cache;
    if (!force_refresh && m_token && !m_token->empty()) return *m_token;
    if (!m_cookie_store) return make_error(ErrorCode::UnsupportedCookiePersistence, "北航云盘需要 CookieStore 读取 OAuth2 token");

    if (!force_refresh) {
        auto cached = read_cloud_token_cookie(m_cookie_store, true, m_mode);
        if (!cached) return make_error(cached.error().code, cached.error().message);
        if (*cached) {
            m_token = **cached;
            return *m_token;
        }
    }

    std::string login_challenge;
    std::string current_url = kCloudLoginUrl;
    auto request = make_html_get_request(current_url, m_mode, m_cookie_store);
    auto response = m_http_client.send(request);
    if (!response) return make_error(response.error().code, "北航云盘登录入口访问失败: " + redacted_error_message(response.error().message));

    for (int redirects = 0; redirects < 12 && is_redirect(response->status_code); ++redirects) {
        auto location = header_value(*response, "Location");
        if (location.empty()) return make_error(ErrorCode::NetworkError, "北航云盘登录重定向缺少 Location");
        auto next_url = resolve_redirect_target(request.url, current_url, location);
        if (!allowed_cloud_redirect(next_url)) return make_error(ErrorCode::NetworkError, "拒绝不安全的云盘重定向地址");
        const auto next_login_challenge = login_challenge_from_url(next_url);
        const auto next_path = url_path(next_url);
        const auto cookie_challenge = next_path.rfind("/oauth2/signin", 0) == 0 ? std::string{} : (login_challenge.empty() ? next_login_challenge : login_challenge);

        current_url = std::move(next_url);
        request = make_html_get_request(current_url, m_mode, m_cookie_store, cookie_challenge);
        if (login_challenge.empty()) login_challenge = next_login_challenge;
        response = m_http_client.send(request);
        if (!response) return make_error(response.error().code, "北航云盘登录重定向失败: " + redacted_error_message(response.error().message));
    }

    if (is_redirect(response->status_code)) return make_error(ErrorCode::NetworkError, "北航云盘登录重定向次数过多");

    auto final_path = url_path(current_url);
    bool callback_from_html = false;
    bool script_redirect_from_html = false;
    std::string script_redirect_path;
    std::string script_redirect_result_path;
    int script_redirect_status = 0;
    std::string refresh_shape;
    std::string refresh_set_cookie_names;
    std::string signin_page_set_cookie_names;
    std::string signin_json_shape;
    std::string signin_json_set_cookie_names;
    std::string signin_json_follow_set_cookie_names;
    std::string oauth_redirect_set_cookie_names;
    std::string script_hop_set_cookie_names;
    bool signin_form_submitted = false;
    std::string signin_form_method;
    std::string signin_form_submit_method;
    std::string signin_form_action_path;
    std::string signin_form_result_path;
    std::string signin_form_markers;
    std::string signin_form_control_markers;
    std::size_t signin_form_field_count = 0;
    std::size_t signin_form_hidden_count = 0;
    std::size_t signin_form_submit_count = 0;
    int signin_form_status = 0;
    std::string activation_markers;
    bool oauth_signin_completed = false;
    DirectSsoActivationProbe direct_sso_activation;
    DirectSsoBridgeProbe direct_sso_bridge;
    if (final_path.rfind("/oauth2/signin", 0) == 0) {
        if (login_challenge.empty()) login_challenge = login_challenge_from_url(current_url);
        bool sso_bridge_handled = false;

        if (!sso_bridge_handled) {
            const auto sso_url = cloud_sso_url(m_mode, login_challenge);
        current_url = sso_url;
        request = make_html_get_request(current_url, m_mode, m_cookie_store);
        response = m_http_client.send(request);
        if (!response) return make_error(response.error().code, "北航云盘 SSO 激活失败: " + redacted_error_message(response.error().message));

        for (int redirects = 0; redirects < 16 && is_redirect(response->status_code); ++redirects) {
            auto location = header_value(*response, "Location");
            if (location.empty()) return make_error(ErrorCode::NetworkError, "北航云盘 SSO 重定向缺少 Location");
            auto next_url = resolve_redirect_target(request.url, current_url, location);
            if (!allowed_cloud_redirect(next_url)) return make_error(ErrorCode::NetworkError, "拒绝不安全的云盘 SSO 重定向地址");
            if (login_challenge.empty()) login_challenge = login_challenge_from_url(next_url);

            current_url = std::move(next_url);
            request = make_html_get_request(current_url, m_mode, m_cookie_store, login_challenge, sso_url);
            response = m_http_client.send(request);
            if (!response) return make_error(response.error().code, "北航云盘 SSO 重定向失败: " + redacted_error_message(response.error().message));
        }

        if (is_redirect(response->status_code)) return make_error(ErrorCode::NetworkError, "北航云盘 SSO 重定向次数过多");
        if (response->status_code == 200 && !Protocol::extract_execution(response->body).empty()) {
            return make_error(ErrorCode::SessionExpired, "北航云盘会话未激活，请先登录或重新登录");
        }
        final_path = url_path(current_url);
        if (response->status_code == 200) {
            activation_markers = html_marker_summary(response->body);
            if (auto script_sources = html_script_src_summary(response->body, current_url); !script_sources.empty()) activation_markers += ",scripts=" + script_sources;
            signin_page_set_cookie_names = set_cookie_shape_summary(*response);
            if (auto signin_page = anyshare_signin_page_from_html(response->body)) {
                signin_form_submitted = true;
                signin_form_method = "json";
                signin_form_submit_method = "post-json";
                signin_form_action_path = "/oauth2/signin";
                signin_form_field_count = 8;
                const auto referer_url = current_url;
                current_url = "https://bhpan.buaa.edu.cn/oauth2/signin";
                const auto credentials = m_login_credentials ? &*m_login_credentials : nullptr;
                auto json_request = make_anyshare_signin_json_request(*signin_page, m_mode, m_cookie_store, login_challenge, referer_url, m_crypto, credentials);
                if (!json_request) return make_error(json_request.error().code, json_request.error().message);
                request = *json_request;
                response = m_http_client.send(request);
                if (!response) return make_error(response.error().code, "北航云盘 OAuth2 signin JSON 提交失败: " + redacted_error_message(response.error().message));
                signin_json_shape = json_shape_summary(response->body);
                signin_json_set_cookie_names = set_cookie_shape_summary(*response);
                if (!is_redirect(response->status_code)) {
                    if (auto redirect_url = anyshare_signin_redirect_from_json(response->body, current_url)) {
                        current_url = *redirect_url;
                        request = make_html_get_request(current_url, m_mode, m_cookie_store, login_challenge, referer_url);
                        response = m_http_client.send(request);
                        if (!response) return make_error(response.error().code, "北航云盘 OAuth2 signin JSON 跳转失败: " + redacted_error_message(response.error().message));
                        signin_json_follow_set_cookie_names = set_cookie_shape_summary(*response);
                    }
                }
                for (int redirects = 0; redirects < 12 && is_redirect(response->status_code); ++redirects) {
                    auto location = header_value(*response, "Location");
                    if (location.empty()) return make_error(ErrorCode::NetworkError, "北航云盘 OAuth2 signin JSON 重定向缺少 Location");
                    auto next_url = resolve_redirect_target(request.url, current_url, location);
                    if (!allowed_cloud_redirect(next_url)) return make_error(ErrorCode::NetworkError, "拒绝不安全的云盘 OAuth2 signin JSON 重定向地址");
                    current_url = std::move(next_url);
                    request = make_html_get_request(current_url, m_mode, m_cookie_store, login_challenge, referer_url);
                    response = m_http_client.send(request);
                    if (!response) return make_error(response.error().code, "北航云盘 OAuth2 signin JSON 重定向失败: " + redacted_error_message(response.error().message));
                    oauth_redirect_set_cookie_names = set_cookie_shape_summary(*response);
                }
                if (is_redirect(response->status_code)) return make_error(ErrorCode::NetworkError, "北航云盘 OAuth2 signin JSON 重定向次数过多");
                final_path = url_path(current_url);
                signin_form_status = response->status_code;
                signin_form_result_path = final_path;
                if (final_path.rfind("/anyshare/oauth2/login/callback", 0) == 0) oauth_signin_completed = true;
                if (response->status_code == 200) {
                    signin_form_markers = html_marker_summary(response->body);
                    if (auto script_sources = html_script_src_summary(response->body, current_url); !script_sources.empty()) signin_form_markers += ",scripts=" + script_sources;
                }
            } else if (auto callback_url = callback_url_from_html(response->body, current_url)) {
                callback_from_html = true;
                const auto referer_url = current_url;
                current_url = *callback_url;
                request = make_html_get_request(current_url, m_mode, m_cookie_store, login_challenge, referer_url);
                response = m_http_client.send(request);
                if (!response) return make_error(response.error().code, "北航云盘 OAuth2 callback 激活失败: " + redacted_error_message(response.error().message));
                oauth_redirect_set_cookie_names = set_cookie_shape_summary(*response);
                for (int redirects = 0; redirects < 8 && is_redirect(response->status_code); ++redirects) {
                    auto location = header_value(*response, "Location");
                    if (location.empty()) return make_error(ErrorCode::NetworkError, "北航云盘 OAuth2 callback 重定向缺少 Location");
                    auto next_url = resolve_redirect_target(request.url, current_url, location);
                    if (!allowed_cloud_redirect(next_url)) return make_error(ErrorCode::NetworkError, "拒绝不安全的云盘 OAuth2 callback 重定向地址");
                    current_url = std::move(next_url);
                    request = make_html_get_request(current_url, m_mode, m_cookie_store, login_challenge, referer_url);
                    response = m_http_client.send(request);
                    if (!response) return make_error(response.error().code, "北航云盘 OAuth2 callback 重定向失败: " + redacted_error_message(response.error().message));
                    oauth_redirect_set_cookie_names = set_cookie_shape_summary(*response);
                }
                if (is_redirect(response->status_code)) return make_error(ErrorCode::NetworkError, "北航云盘 OAuth2 callback 重定向次数过多");
                final_path = url_path(current_url);
                if (final_path.rfind("/anyshare/oauth2/login/callback", 0) == 0) oauth_signin_completed = true;
            } else if (auto script_url = javascript_location_url_from_html(response->body, current_url)) {
                script_redirect_from_html = true;
                script_redirect_path = url_path(*script_url);
                const auto referer_url = current_url;
                current_url = *script_url;
                request = make_html_get_request(current_url, m_mode, m_cookie_store, login_challenge, referer_url);
                response = m_http_client.send(request);
                if (!response) return make_error(response.error().code, "北航云盘 OAuth2 页面跳转失败: " + redacted_error_message(response.error().message));
                script_hop_set_cookie_names = set_cookie_shape_summary(*response);
                for (int redirects = 0; redirects < 12 && is_redirect(response->status_code); ++redirects) {
                    auto location = header_value(*response, "Location");
                    if (location.empty()) return make_error(ErrorCode::NetworkError, "北航云盘 OAuth2 页面跳转重定向缺少 Location");
                    auto next_url = resolve_redirect_target(request.url, current_url, location);
                    if (!allowed_cloud_redirect(next_url)) return make_error(ErrorCode::NetworkError, "拒绝不安全的云盘 OAuth2 页面跳转地址");
                    current_url = std::move(next_url);
                    request = make_html_get_request(current_url, m_mode, m_cookie_store, login_challenge, referer_url);
                    response = m_http_client.send(request);
                    if (!response) return make_error(response.error().code, "北航云盘 OAuth2 页面跳转重定向失败: " + redacted_error_message(response.error().message));
                    script_hop_set_cookie_names = set_cookie_shape_summary(*response);
                }
                if (is_redirect(response->status_code)) return make_error(ErrorCode::NetworkError, "北航云盘 OAuth2 页面跳转重定向次数过多");
                final_path = url_path(current_url);
                if (final_path.rfind("/anyshare/oauth2/login/callback", 0) == 0) oauth_signin_completed = true;
                script_redirect_status = response->status_code;
                script_redirect_result_path = final_path;
                if (response->status_code == 200) {
                    activation_markers = html_marker_summary(response->body);
                    if (auto script_sources = html_script_src_summary(response->body, current_url); !script_sources.empty()) activation_markers += ",scripts=" + script_sources;
                }
            } else if (auto form = oauth_signin_form_from_html(response->body, current_url)) {
                signin_form_submitted = true;
                signin_form_method = form->method;
                signin_form_action_path = url_path(form->action);
                signin_form_field_count = form_fields_with_login_challenge(form->fields, login_challenge, form->action).size();
                signin_form_hidden_count = form->hidden_count;
                signin_form_submit_count = form->submit_count;
                signin_form_control_markers = join_control_markers(*form);
                const auto referer_url = current_url;
                current_url = form->action;
                const auto credentials = m_login_credentials ? &*m_login_credentials : nullptr;
                request = make_html_form_request(*form, m_mode, m_cookie_store, login_challenge, referer_url, credentials);
                signin_form_submit_method = request.method == HttpMethod::Post ? "post" : "get";
                response = m_http_client.send(request);
                if (!response) return make_error(response.error().code, "北航云盘 OAuth2 signin 表单提交失败: " + redacted_error_message(response.error().message));
                for (int redirects = 0; redirects < 12 && is_redirect(response->status_code); ++redirects) {
                    auto location = header_value(*response, "Location");
                    if (location.empty()) return make_error(ErrorCode::NetworkError, "北航云盘 OAuth2 signin 表单重定向缺少 Location");
                    auto next_url = resolve_redirect_target(request.url, current_url, location);
                    if (!allowed_cloud_redirect(next_url)) return make_error(ErrorCode::NetworkError, "拒绝不安全的云盘 OAuth2 signin 表单重定向地址");
                    current_url = std::move(next_url);
                    request = make_html_get_request(current_url, m_mode, m_cookie_store, login_challenge, referer_url);
                    response = m_http_client.send(request);
                    if (!response) return make_error(response.error().code, "北航云盘 OAuth2 signin 表单重定向失败: " + redacted_error_message(response.error().message));
                }
                if (is_redirect(response->status_code)) return make_error(ErrorCode::NetworkError, "北航云盘 OAuth2 signin 表单重定向次数过多");
                final_path = url_path(current_url);
                signin_form_status = response->status_code;
                signin_form_result_path = final_path;
                if (final_path.rfind("/anyshare/oauth2/login/callback", 0) == 0) oauth_signin_completed = true;
                if (response->status_code == 200) {
                    signin_form_markers = html_marker_summary(response->body);
                    if (auto script_sources = html_script_src_summary(response->body, current_url); !script_sources.empty()) signin_form_markers += ",scripts=" + script_sources;
                }
            }
        }
        }
    } else if (response->status_code == 200 && !Protocol::extract_execution(response->body).empty()) {
        return make_error(ErrorCode::SessionExpired, "北航云盘会话未激活，请先登录或重新登录");
    }

    for (int script_hops = 0; script_hops < 4 && response->status_code == 200; ++script_hops) {
        auto script_url = javascript_location_url_from_html(response->body, current_url);
        if (!script_url || *script_url == current_url) break;
        script_redirect_from_html = true;
        script_redirect_path = url_path(*script_url);
        const auto referer_url = current_url;
        current_url = *script_url;
        request = make_html_get_request(current_url, m_mode, m_cookie_store, login_challenge, referer_url);
        response = m_http_client.send(request);
        if (!response) return make_error(response.error().code, "北航云盘 OAuth2 页面脚本跳转失败: " + redacted_error_message(response.error().message));
        script_hop_set_cookie_names = set_cookie_shape_summary(*response);
        for (int redirects = 0; redirects < 12 && is_redirect(response->status_code); ++redirects) {
            auto location = header_value(*response, "Location");
            if (location.empty()) return make_error(ErrorCode::NetworkError, "北航云盘 OAuth2 页面脚本跳转重定向缺少 Location");
            auto next_url = resolve_redirect_target(request.url, current_url, location);
            if (!allowed_cloud_redirect(next_url)) return make_error(ErrorCode::NetworkError, "拒绝不安全的云盘 OAuth2 页面脚本跳转地址");
            current_url = std::move(next_url);
            request = make_html_get_request(current_url, m_mode, m_cookie_store, login_challenge, referer_url);
            response = m_http_client.send(request);
            if (!response) return make_error(response.error().code, "北航云盘 OAuth2 页面脚本跳转重定向失败: " + redacted_error_message(response.error().message));
            script_hop_set_cookie_names = set_cookie_shape_summary(*response);
        }
        if (is_redirect(response->status_code)) return make_error(ErrorCode::NetworkError, "北航云盘 OAuth2 页面脚本跳转重定向次数过多");
        final_path = url_path(current_url);
        if (final_path.rfind("/anyshare/oauth2/login/callback", 0) == 0) oauth_signin_completed = true;
        script_redirect_status = response->status_code;
        script_redirect_result_path = final_path;
        if (response->status_code == 200) {
            activation_markers = html_marker_summary(response->body);
            if (auto script_sources = html_script_src_summary(response->body, current_url); !script_sources.empty()) activation_markers += ",scripts=" + script_sources;
        }
    }

    const auto final_host = extract_host(VpnCipher::from_vpn_url(current_url));
    if (oauth_signin_completed && final_path.rfind("/anyshare/zh-cn/portal", 0) == 0) {
        // WebVPN 下前端会在 portal 页继续触发 refreshToken；保留 portal Referer，避免把请求伪装成 callback 页。
    }
    const auto activation_status = response->status_code;
    if (auto body_token = token_from_json_text(response->body)) {
        m_token = *body_token;
        return *m_token;
    }
    if (!force_refresh) {
        if (auto activated = read_cloud_token_cookie(m_cookie_store, false, m_mode); activated && *activated) {
            m_token = **activated;
            return *m_token;
        } else if (!activated) {
            return make_error(activated.error().code, activated.error().message);
        }
    }

    if (final_path.rfind("/anyshare/oauth2/login/callback", 0) == 0 || final_host == kCloudHost) {
        const auto refresh_referer = current_url.empty() ? std::string{kCloudLoginUrl} : current_url;
        const std::array<std::pair<const char *, const char *>, 2> refresh_variants{{
            {"base", kCloudRefreshUrl},
            {"isforced", "https://bhpan.buaa.edu.cn/anyshare/oauth2/login/refreshToken?isforced=false"},
        }};
        for (const auto &[refresh_label, refresh_url] : refresh_variants) {
            auto refresh = make_cloud_refresh_request(refresh_url, m_mode, m_cookie_store, refresh_referer);
            response = m_http_client.send(refresh);
            if (!response) return make_error(response.error().code, "北航云盘 token 刷新失败: " + redacted_error_message(response.error().message));
            const auto variant_shape = json_shape_summary(response->body);
            const auto variant_cookies = set_cookie_shape_summary(*response);
            if (!refresh_shape.empty()) refresh_shape += ";";
            refresh_shape += std::string(refresh_label) + "(" + variant_shape + ")";
            if (!refresh_set_cookie_names.empty()) refresh_set_cookie_names += ";";
            refresh_set_cookie_names += std::string(refresh_label) + "(" + variant_cookies + ")";
            if (Protocol::is_session_expired_response(*response, {}, false)) return make_error(ErrorCode::SessionExpired, "北航云盘会话已过期，请重新登录");
            if (response->status_code < 200 || response->status_code >= 300) {
                return make_error(ErrorCode::NetworkError,
                                  "北航云盘 token 刷新返回非成功状态: " + std::to_string(response->status_code) +
                                      "，激活状态: " + std::to_string(activation_status) +
                                      "，激活路径: " + final_path +
                                      "，direct SSO: " + direct_sso_activation_summary(direct_sso_activation) +
                                      "，direct SSO bridge: " + direct_sso_bridge_summary(direct_sso_bridge) +
                                      "，refresh shape: " + refresh_shape +
                                      "，refresh response cookies: " + refresh_set_cookie_names +
                                      "，signin page cookies: " + (signin_page_set_cookie_names.empty() ? "none" : signin_page_set_cookie_names) +
                                      "，signin JSON shape: " + (signin_json_shape.empty() ? "none" : signin_json_shape) +
                                      "，signin JSON cookies: " + (signin_json_set_cookie_names.empty() ? "none" : signin_json_set_cookie_names) +
                                      "，signin follow cookies: " + (signin_json_follow_set_cookie_names.empty() ? "none" : signin_json_follow_set_cookie_names) +
                                      "，oauth redirect cookies: " + (oauth_redirect_set_cookie_names.empty() ? "none" : oauth_redirect_set_cookie_names) +
                                      "，script hop cookies: " + (script_hop_set_cookie_names.empty() ? "none" : script_hop_set_cookie_names) +
                                      "，cookie presence: " + cloud_cookie_presence_summary(m_cookie_store) +
                                      "，HTML callback: " + (callback_from_html ? "yes" : "no") +
                                      "，HTML script: " + (script_redirect_from_html ? "followed" : "none") +
                                      (script_redirect_from_html ? ("，script path: " + script_redirect_path +
                                                                   "，script status: " + std::to_string(script_redirect_status) +
                                                                   "，script result: " + script_redirect_result_path)
                                                               : std::string{}) +
                                      "，HTML form: " + (signin_form_submitted ? "submitted" : "none") +
                                      (signin_form_submitted ? ("，form method: " + signin_form_method +
                                                                 "，form submit method: " + signin_form_submit_method +
                                                                 "，form action: " + signin_form_action_path +
                                                                 "，form fields: " + std::to_string(signin_form_field_count) +
                                                                 "，form hidden: " + std::to_string(signin_form_hidden_count) +
                                                                 "，form submit: " + std::to_string(signin_form_submit_count) +
                                                                 "，form status: " + std::to_string(signin_form_status) +
                                                                 "，form result: " + signin_form_result_path +
                                                                 (signin_form_control_markers.empty() ? std::string{} : "，form controls: " + signin_form_control_markers) +
                                                                 (signin_form_markers.empty() ? std::string{} : "，form markers: " + signin_form_markers))
                                                             : std::string{}) +
                                      (activation_markers.empty() ? std::string{} : "，HTML markers: " + activation_markers));
            }
            if (auto body_token = token_from_json_text(response->body)) {
                m_token = *body_token;
                return *m_token;
            }
            auto refreshed_cookie = read_cloud_token_cookie(m_cookie_store, false, m_mode);
            if (!refreshed_cookie) return make_error(refreshed_cookie.error().code, refreshed_cookie.error().message);
            if (*refreshed_cookie) {
                m_token = **refreshed_cookie;
                return *m_token;
            }
        }
    }

    auto refreshed = read_cloud_token_cookie(m_cookie_store, false, m_mode);
    if (!refreshed) return make_error(refreshed.error().code, refreshed.error().message);
    if (!*refreshed) {
        refreshed = read_cloud_token_cookie(m_cookie_store, true, m_mode);
        if (!refreshed) return make_error(refreshed.error().code, refreshed.error().message);
    }
    if (!*refreshed) {
        return make_error(ErrorCode::SessionExpired,
                          "北航云盘未返回 OAuth2 token，请重新登录" +
                              std::string{"，激活状态: "} + std::to_string(activation_status) +
                              "，激活路径: " + final_path +
                              "，direct SSO: " + direct_sso_activation_summary(direct_sso_activation) +
                              "，direct SSO bridge: " + direct_sso_bridge_summary(direct_sso_bridge) +
                              "，refresh shape: " + refresh_shape +
                              "，refresh response cookies: " + refresh_set_cookie_names +
                              "，signin page cookies: " + (signin_page_set_cookie_names.empty() ? "none" : signin_page_set_cookie_names) +
                              "，signin JSON shape: " + (signin_json_shape.empty() ? "none" : signin_json_shape) +
                              "，signin JSON cookies: " + (signin_json_set_cookie_names.empty() ? "none" : signin_json_set_cookie_names) +
                              "，signin follow cookies: " + (signin_json_follow_set_cookie_names.empty() ? "none" : signin_json_follow_set_cookie_names) +
                              "，oauth redirect cookies: " + (oauth_redirect_set_cookie_names.empty() ? "none" : oauth_redirect_set_cookie_names) +
                              "，script hop cookies: " + (script_hop_set_cookie_names.empty() ? "none" : script_hop_set_cookie_names) +
                              "，cookie presence: " + cloud_cookie_presence_summary(m_cookie_store) +
                              "，HTML callback: " + (callback_from_html ? "yes" : "no") +
                              "，HTML script: " + (script_redirect_from_html ? "followed" : "none") +
                              "，HTML form: " + (signin_form_submitted ? "submitted" : "none") +
                              (signin_form_submitted ? ("，form method: " + signin_form_method +
                                                         "，form submit method: " + signin_form_submit_method +
                                                         "，form action: " + signin_form_action_path +
                                                         "，form fields: " + std::to_string(signin_form_field_count) +
                                                         "，form status: " + std::to_string(signin_form_status) +
                                                         "，form result: " + signin_form_result_path +
                                                         (signin_form_markers.empty() ? std::string{} : "，form markers: " + signin_form_markers))
                                                     : std::string{}) +
                              (activation_markers.empty() ? std::string{} : "，HTML markers: " + activation_markers));
    }

    m_token = **refreshed;
    return *m_token;
}

Result<HttpResponse> CloudService::send_json_request(HttpRequest request, const std::string &context, const std::string &extra_token) {
    auto ready = token();
    if (!ready) return make_error(ready.error().code, ready.error().message);
    request.headers["Authorization"] = bearer_header_value(*ready);
    if (!extra_token.empty()) request.headers["x-as-authorization"] = bearer_header_value(extra_token);

    auto response = m_http_client.send(request);
    if (!response) return make_error(response.error().code, context + "请求失败: " + redacted_error_message(response.error().message));
    if (response->status_code == 401 || response->status_code == 403 || Protocol::is_session_expired_response(*response, {}, true)) {
        m_token.reset();
        auto refreshed = token(true);
        if (refreshed) {
            request.headers["Authorization"] = bearer_header_value(*refreshed);
            response = m_http_client.send(request);
            if (!response) return make_error(response.error().code, context + "请求失败: " + redacted_error_message(response.error().message));
            if (response->status_code != 401 && response->status_code != 403 && !Protocol::is_session_expired_response(*response, {}, true)) return *response;
        }
        return make_error(ErrorCode::SessionExpired, context + "会话已过期，请重新登录");
    }
    return *response;
}

Result<std::vector<Model::CloudItem>> CloudService::roots(CloudRootKind kind) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(append_query(kCloudRootUrl, root_query(kind)), m_mode);
    apply_cloud_headers(request, m_mode);
    auto response = send_json_request(std::move(request), "北航云盘根目录");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘根目录");
    if (!json) return make_error(json.error().code, json.error().message);
    return Parser::parse_cloud_roots(*json);
}

Result<Model::CloudItem> CloudService::user_root() {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(kCloudUserRootUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    auto response = send_json_request(std::move(request), "北航云盘用户根目录");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘用户根目录");
    if (!json) return make_error(json.error().code, json.error().message);
    auto roots = Parser::parse_cloud_roots(*json);
    if (roots.empty()) return make_error(ErrorCode::ParseError, "北航云盘用户根目录为空");
    return roots.front();
}

Result<Model::CloudDir> CloudService::list_dir(const CloudListQuery &query) {
    if (query.doc_id.empty()) return make_error(ErrorCode::InvalidArgument, "file list 需要 --id <docid>");
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudListUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"by", "name"}, {"docid", query.doc_id}, {"sort", "asc"}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘目录列表", query.token);
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘目录列表");
    if (!json) return make_error(json.error().code, json.error().message);
    return Parser::parse_cloud_dir(*json);
}

Result<Model::CloudSize> CloudService::item_size(const CloudListQuery &query) {
    if (query.doc_id.empty()) return make_error(ErrorCode::InvalidArgument, "file size 需要 --id <docid>");
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudSizeUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"docid", query.doc_id}, {"onlyrecycle", false}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘容量", query.token);
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘容量");
    if (!json) return make_error(json.error().code, json.error().message);
    return Parser::parse_cloud_size(*json);
}

Result<Model::CloudDir> CloudService::recycle_bin() {
    auto root = user_root();
    if (!root) return make_error(root.error().code, root.error().message);
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudRecycleUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"by", "time"}, {"docid", root->id}, {"limit", 100}, {"sort", "desc"}, {"start", 0}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘回收站");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘回收站");
    if (!json) return make_error(json.error().code, json.error().message);
    return Parser::parse_cloud_dir(*json);
}

Result<std::vector<Model::CloudShare>> CloudService::share_history() {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(kCloudShareHistoryUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    auto response = send_json_request(std::move(request), "北航云盘分享记录");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘分享记录");
    if (!json) return make_error(json.error().code, json.error().message);
    return Parser::parse_cloud_shares(*json);
}

Result<std::string> CloudService::suggest_name(const std::string &parent_id, const std::string &name) {
    if (parent_id.empty()) return make_error(ErrorCode::InvalidArgument, "file suggest-name 需要 --parent-id <docid>");
    if (name.empty()) return make_error(ErrorCode::InvalidArgument, "file suggest-name 需要 --name <name>");
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudSuggestNameUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"docid", parent_id}, {"name", name}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘建议名称");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘建议名称");
    if (!json) return make_error(json.error().code, json.error().message);
    auto suggested = first_json_string(*json, {"name"});
    if (suggested.empty()) return make_error(ErrorCode::ParseError, "北航云盘建议名称响应缺少 name");
    return suggested;
}

Result<std::vector<Model::CloudShare>> CloudService::share_record(const std::string &item_id) {
    if (item_id.empty()) return make_error(ErrorCode::InvalidArgument, "file share-record 需要 --id <docid>");
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode("https://bhpan.buaa.edu.cn/api/shared-link/v1/document/folder/" +
                                       url_encode_component(item_id) + "?type=anonymous",
                                   m_mode);
    apply_cloud_headers(request, m_mode);
    auto response = send_json_request(std::move(request), "北航云盘单项分享记录");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘单项分享记录");
    if (!json) return make_error(json.error().code, json.error().message);
    return Parser::parse_cloud_shares(*json);
}

Result<Model::CloudItem> CloudService::share_parse(const std::string &id_or_url, const std::string &password) {
    auto share_id = share_id_from_url(id_or_url);
    if (share_id.empty()) return make_error(ErrorCode::InvalidArgument, "file share-parse 需要 --id <share-id|url>");

    HttpRequest info_request;
    info_request.method = HttpMethod::Get;
    info_request.url = resolve_for_mode(std::string{kCloudShareLinkInfoUrl} + url_encode_component(share_id), m_mode);
    apply_cloud_headers(info_request, m_mode);
    auto info_response = m_http_client.send(info_request);
    if (!info_response) return make_error(info_response.error().code, "北航云盘分享信息请求失败: " + redacted_error_message(info_response.error().message));
    if (info_response->status_code < 200 || info_response->status_code >= 300) {
        return make_error(ErrorCode::NetworkError, "北航云盘分享信息请求返回: " + std::to_string(info_response->status_code));
    }
    auto info_json = nlohmann::json::parse(info_response->body, nullptr, false);
    if (info_json.is_discarded()) return make_error(ErrorCode::ParseError, "解析北航云盘分享信息 JSON 失败");
    const auto password_required = json_bool(info_json, "password_required", false);

    HttpResponse link_response;
    if (password_required) {
        if (password.empty()) return make_error(ErrorCode::InvalidArgument, "北航云盘分享链接需要 --password");
        HttpRequest link_request;
        link_request.method = HttpMethod::Post;
        link_request.url = resolve_for_mode("https://bhpan.buaa.edu.cn/link", m_mode);
        apply_cloud_headers(link_request, m_mode);
        link_request.headers["Content-Type"] = "application/json";
        link_request.body = nlohmann::json{
            {"id", share_id},
            {"type", "anonymous"},
            {"password_required", true},
            {"password", password},
        }.dump();
        auto response = m_http_client.send(link_request);
        if (!response) return make_error(response.error().code, "北航云盘分享密码提交失败: " + redacted_error_message(response.error().message));
        link_response = *response;
    } else {
        HttpRequest link_request;
        link_request.method = HttpMethod::Get;
        link_request.url = resolve_for_mode("https://bhpan.buaa.edu.cn/link/" + url_encode_component(share_id), m_mode);
        apply_cloud_headers(link_request, m_mode);
        auto response = m_http_client.send(link_request);
        if (!response) return make_error(response.error().code, "北航云盘分享链接访问失败: " + redacted_error_message(response.error().message));
        link_response = *response;
    }
    if (link_response.status_code < 200 || link_response.status_code >= 400) {
        return make_error(ErrorCode::NetworkError, "北航云盘分享链接访问返回: " + std::to_string(link_response.status_code));
    }

    const auto cookie_name = "link_token:" + share_id;
    auto link_token = cookie_from_store(m_cookie_store, kCloudHost, cookie_name);
    if (!link_token) link_token = cookie_from_set_cookie_header(link_response, cookie_name);
    if (!link_token || link_token->empty()) return make_error(ErrorCode::SessionExpired, "北航云盘分享链接未返回临时授权 token");

    HttpRequest entry_request;
    entry_request.method = HttpMethod::Get;
    entry_request.url = resolve_for_mode(kCloudEntryItemUrl, m_mode);
    apply_cloud_headers(entry_request, m_mode);
    entry_request.headers["Authorization"] = bearer_header_value(*link_token);
    auto entry_response = m_http_client.send(entry_request);
    if (!entry_response) return make_error(entry_response.error().code, "北航云盘分享入口请求失败: " + redacted_error_message(entry_response.error().message));
    auto entry_json = parse_cloud_json(*entry_response, "北航云盘分享入口");
    if (!entry_json) return make_error(entry_json.error().code, entry_json.error().message);
    auto items = Parser::parse_cloud_roots(*entry_json);
    if (items.empty()) return make_error(ErrorCode::ParseError, "北航云盘分享入口为空");
    items.front().token = *link_token;
    return items.front();
}

Result<Model::CloudDownloadUrl> CloudService::download_url(const CloudItemRef &item) {
    if (item.doc_id.empty()) return make_error(ErrorCode::InvalidArgument, "file download-url 需要 --id <docid>");
    if (item.is_dir) {
        auto result = batch_download_url({item}, item.name.empty() ? "download" : item.name);
        if (!result) return make_error(result.error().code, result.error().message);
        return *result;
    }

    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudSingleDownloadUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"docid", item.doc_id}, {"authtype", "QUERY_STRING"}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘下载链接", item.token);
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘下载链接");
    if (!json) return make_error(json.error().code, json.error().message);
    auto url = extract_download_url(*json);
    if (url.empty()) return make_error(ErrorCode::ParseError, "北航云盘下载链接响应缺少 URL");
    return Model::CloudDownloadUrl{url, item.name, false};
}

Result<Model::CloudDownloadChunk> CloudService::download_range(const CloudItemRef &item, std::uint64_t offset, std::uint64_t length) {
    if (item.is_dir) return make_error(ErrorCode::InvalidArgument, "北航云盘目录不能按文件读取");
    if (length == 0) return Model::CloudDownloadChunk{{}, offset, true};

    auto url = download_url(item);
    if (!url) return make_error(url.error().code, url.error().message);
    if (url->zipped) return make_error(ErrorCode::InvalidArgument, "北航云盘压缩下载链接不能作为文件 range 读取");
    if (offset > std::numeric_limits<std::uint64_t>::max() - (length - 1)) {
        return make_error(ErrorCode::InvalidArgument, "北航云盘文件读取 range 超出范围");
    }

    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = url->url;
    request.transport.redact_url_query_in_errors = true;
    const auto end = offset + length - 1;
    request.headers["Range"] = "bytes=" + std::to_string(offset) + "-" + std::to_string(end);

    auto response = m_http_client.send(request);
    if (!response) return make_error(response.error().code, "北航云盘文件读取失败: " + redacted_error_message(response.error().message));
    if (response->status_code != 200 && response->status_code != 206) {
        return make_error(ErrorCode::NetworkError, "北航云盘文件读取失败: HTTP " + std::to_string(response->status_code));
    }

    Model::CloudDownloadChunk chunk;
    chunk.offset = offset;
    chunk.partial = response->status_code == 206;
    chunk.bytes.assign(response->body.begin(), response->body.end());
    return chunk;
}

Result<Model::CloudDownloadUrl> CloudService::batch_download_url(const std::vector<CloudItemRef> &items, const std::string &name) {
    if (items.empty()) return make_error(ErrorCode::InvalidArgument, "file batch-download-url 需要至少一个 --id");
    if (items.size() == 1 && !items.front().is_dir) return download_url(items.front());

    nlohmann::json dirs = nlohmann::json::array();
    nlohmann::json files = nlohmann::json::array();
    for (const auto &item : items) {
        if (item.doc_id.empty()) return make_error(ErrorCode::InvalidArgument, "file batch-download-url 包含空 docid");
        if (item.is_dir) dirs.push_back(item.doc_id);
        else files.push_back(item.doc_id);
    }

    auto archive_name = name.empty() ? "download" : name;
    if (archive_name.size() < 4 || archive_name.substr(archive_name.size() - 4) != ".zip") archive_name += ".zip";
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudBatchDownloadUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"name", archive_name}, {"reqhost", kCloudHost}, {"dirs", dirs}, {"files", files}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘批量下载链接", items.front().token);
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘批量下载链接");
    if (!json) return make_error(json.error().code, json.error().message);
    auto url = extract_download_url(*json);
    if (url.empty()) return make_error(ErrorCode::ParseError, "北航云盘批量下载链接响应缺少 URL");
    return Model::CloudDownloadUrl{url, archive_name, true};
}

void CloudService::set_write_operation_gate(WriteOperationGate gate) {
    m_write_gate = std::move(gate);
}

Result<Model::MutationResult> CloudService::create_dir(const std::string &parent_id, const std::string &name) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (parent_id.empty()) return make_error(ErrorCode::InvalidArgument, "file mkdir 需要 --parent-id <docid>");
    if (name.empty()) return make_error(ErrorCode::InvalidArgument, "file mkdir 需要 --name <name>");
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudCreateDirUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"docid", parent_id}, {"name", name}, {"ondup", 1}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘创建目录");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘创建目录");
    if (!json) return make_error(json.error().code, json.error().message);
    auto id = extract_id_from_json(*json);
    if (id.empty()) return make_error(ErrorCode::ParseError, "北航云盘创建目录响应缺少 docid");
    return cloud_mutation(id, "Cloud directory created", "created", {{"parentId", parent_id}, {"name", name}});
}

Result<Model::MutationResult> CloudService::rename_item(const std::string &item_id, const std::string &name) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (item_id.empty()) return make_error(ErrorCode::InvalidArgument, "file rename 需要 --id <docid>");
    if (name.empty()) return make_error(ErrorCode::InvalidArgument, "file rename 需要 --name <name>");
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudRenameUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"docid", item_id}, {"name", name}, {"ondup", 1}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘重命名");
    if (!response) return make_error(response.error().code, response.error().message);
    auto accepted = accept_cloud_mutation_response(*response, "北航云盘重命名");
    if (!accepted) return make_error(accepted.error().code, accepted.error().message);
    return cloud_mutation(item_id, "Cloud item renamed", "renamed", {{"name", name}});
}

Result<Model::MutationResult> CloudService::move_item(const std::string &item_id, const std::string &dest_parent_id) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (item_id.empty()) return make_error(ErrorCode::InvalidArgument, "file move 需要 --id <docid>");
    if (dest_parent_id.empty()) return make_error(ErrorCode::InvalidArgument, "file move 需要 --dest-id <docid>");
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudMoveUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"destparent", dest_parent_id}, {"docid", item_id}, {"ondup", 1}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘移动");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘移动");
    if (!json) return make_error(json.error().code, json.error().message);
    auto id = extract_id_from_json(*json);
    if (id.empty()) id = item_id;
    return cloud_mutation(id, "Cloud item moved", "moved", {{"sourceId", item_id}, {"destParentId", dest_parent_id}});
}

Result<Model::MutationResult> CloudService::copy_item(const CloudItemRef &item, const std::string &dest_parent_id) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (item.doc_id.empty()) return make_error(ErrorCode::InvalidArgument, "file copy 需要 --id <docid>");
    if (dest_parent_id.empty()) return make_error(ErrorCode::InvalidArgument, "file copy 需要 --dest-id <docid>");
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudCopyUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"destparent", dest_parent_id}, {"docid", item.doc_id}, {"ondup", 1}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘复制", item.token);
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘复制");
    if (!json) return make_error(json.error().code, json.error().message);
    auto id = extract_id_from_json(*json);
    if (id.empty()) return make_error(ErrorCode::ParseError, "北航云盘复制响应缺少 docid");
    return cloud_mutation(id, "Cloud item copied", "copied", {{"sourceId", item.doc_id}, {"destParentId", dest_parent_id}});
}

Result<Model::MutationResult> CloudService::delete_item(const std::string &item_id) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (item_id.empty()) return make_error(ErrorCode::InvalidArgument, "file delete 需要 --id <docid>");
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudDeleteUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"docid", item_id}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘删除");
    if (!response) return make_error(response.error().code, response.error().message);
    auto accepted = accept_cloud_mutation_response(*response, "北航云盘删除");
    if (!accepted) return make_error(accepted.error().code, accepted.error().message);
    return cloud_mutation(item_id, "Cloud item moved to recycle bin", "deleted");
}

Result<Model::MutationResult> CloudService::delete_recycle_item(const std::string &item_id) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (item_id.empty()) return make_error(ErrorCode::InvalidArgument, "file recycle-delete 需要 --id <docid>");
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudRecycleDeleteUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"docid", item_id}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘彻底删除回收站项目");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘彻底删除回收站项目");
    if (!json) return make_error(json.error().code, json.error().message);
    return cloud_mutation(item_id, "Cloud recycle item deleted", "recycle-deleted");
}

Result<Model::MutationResult> CloudService::restore_recycle_item(const std::string &item_id) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (item_id.empty()) return make_error(ErrorCode::InvalidArgument, "file recycle-restore 需要 --id <docid>");
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudRecycleRestoreUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = nlohmann::json{{"docid", item_id}, {"ondup", 1}}.dump();
    auto response = send_json_request(std::move(request), "北航云盘恢复回收站项目");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘恢复回收站项目");
    if (!json) return make_error(json.error().code, json.error().message);
    auto id = extract_id_from_json(*json);
    if (id.empty()) id = item_id;
    return cloud_mutation(id, "Cloud recycle item restored", "restored", {{"sourceId", item_id}});
}

Result<Model::MutationResult> CloudService::share_item(const CloudShareRequest &share) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (share.item_id.empty()) return make_error(ErrorCode::InvalidArgument, "file share-create 需要 --id <docid>");
    if (share.title.empty()) return make_error(ErrorCode::InvalidArgument, "file share-create 需要 --name <title>");
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kCloudShareCreateUrl, m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = cloud_share_payload(share).dump();
    auto response = send_json_request(std::move(request), "北航云盘创建分享");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘创建分享");
    if (!json) return make_error(json.error().code, json.error().message);
    auto id = extract_id_from_json(*json);
    if (id.empty()) return make_error(ErrorCode::ParseError, "北航云盘创建分享响应缺少 id");
    return cloud_mutation(id, "Cloud share created", "shared",
                          {{"itemId", share.item_id},
                           {"url", "https://bhpan.buaa.edu.cn/link/" + id},
                           {"permissions", share_permissions_to_string(share.permission)},
                           {"expiresAt", share.expires_at},
                           {"limitedTimes", std::to_string(share.limit)}});
}

Result<Model::MutationResult> CloudService::share_update(const std::string &share_id, const CloudShareRequest &share) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (share_id.empty()) return make_error(ErrorCode::InvalidArgument, "file share-update 需要 --share-id <id>");
    HttpRequest request;
    request.method = HttpMethod::Put;
    request.url = resolve_for_mode(std::string{kCloudShareCreateUrl} + "/" + url_encode_component(share_id), m_mode);
    apply_cloud_headers(request, m_mode);
    request.headers["Content-Type"] = "application/json";
    request.body = cloud_share_payload(share).dump();
    auto response = send_json_request(std::move(request), "北航云盘更新分享");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_cloud_json(*response, "北航云盘更新分享");
    if (!json) return make_error(json.error().code, json.error().message);
    return cloud_mutation(share_id, "Cloud share updated", "share-updated",
                          {{"itemId", share.item_id}, {"permissions", share_permissions_to_string(share.permission)}});
}

Result<Model::MutationResult> CloudService::share_delete(const std::string &share_id) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (share_id.empty()) return make_error(ErrorCode::InvalidArgument, "file share-delete 需要 --share-id <id>");
    HttpRequest request;
    request.method = HttpMethod::Delete;
    request.url = resolve_for_mode(std::string{kCloudShareCreateUrl} + "/" + url_encode_component(share_id), m_mode);
    apply_cloud_headers(request, m_mode);
    auto response = send_json_request(std::move(request), "北航云盘删除分享");
    if (!response) return make_error(response.error().code, response.error().message);
    auto accepted = accept_cloud_mutation_response(*response, "北航云盘删除分享");
    if (!accepted) return make_error(accepted.error().code, accepted.error().message);
    return cloud_mutation(share_id, "Cloud share deleted", "share-deleted");
}

Result<Model::MutationResult> CloudService::upload_file(const CloudUploadRequest &upload, IUploadSource &source) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (upload.parent_id.empty()) return make_error(ErrorCode::InvalidArgument, "file upload 需要 --parent-id <docid>");
    auto name = upload.name.empty() ? source.name() : upload.name;
    if (name.empty()) return make_error(ErrorCode::InvalidArgument, "file upload 需要文件名或 --name <name>");

    auto hashes = compute_upload_hashes(source);
    if (!hashes) return make_error(hashes.error().code, hashes.error().message);

    if (upload.token.empty()) {
        HttpRequest pre_request;
        pre_request.method = HttpMethod::Post;
        pre_request.url = resolve_for_mode(kCloudPreuploadUrl, m_mode);
        apply_cloud_headers(pre_request, m_mode);
        pre_request.headers["Content-Type"] = "application/json";
        pre_request.body = nlohmann::json{{"slice_md5", hashes->slice_md5}, {"length", hashes->length}}.dump();
        auto pre_response = send_json_request(std::move(pre_request), "北航云盘秒传检查");
        if (!pre_response) return make_error(pre_response.error().code, pre_response.error().message);
        auto pre_json = parse_cloud_json(*pre_response, "北航云盘秒传检查");
        if (!pre_json) return make_error(pre_json.error().code, pre_json.error().message);
        if (json_bool(*pre_json, "match", false)) {
            HttpRequest fast_request;
            fast_request.method = HttpMethod::Post;
            fast_request.url = resolve_for_mode(kCloudFastUploadUrl, m_mode);
            apply_cloud_headers(fast_request, m_mode);
            fast_request.headers["Content-Type"] = "application/json";
            fast_request.body = nlohmann::json{
                {"client_mtime", static_cast<std::int64_t>(0)},
                {"crc32", hashes->crc32},
                {"docid", upload.parent_id},
                {"length", hashes->length},
                {"md5", hashes->md5},
                {"name", name},
                {"ondup", 1},
            }.dump();
            auto fast_response = send_json_request(std::move(fast_request), "北航云盘秒传");
            if (!fast_response) return make_error(fast_response.error().code, fast_response.error().message);
            auto fast_json = parse_cloud_json(*fast_response, "北航云盘秒传");
            if (!fast_json) return make_error(fast_json.error().code, fast_json.error().message);
            return cloud_mutation(upload.parent_id, "Cloud file uploaded by fast path", "uploaded",
                                  {{"name", name}, {"parentId", upload.parent_id}, {"fastUpload", "true"}});
        }
    }

    if (hashes->length <= kCloudUploadPartSize) {
        HttpRequest begin_request;
        begin_request.method = HttpMethod::Post;
        begin_request.url = resolve_for_mode(kCloudBeginUploadUrl, m_mode);
        apply_cloud_headers(begin_request, m_mode);
        begin_request.headers["Content-Type"] = "application/json";
        begin_request.body = nlohmann::json{
            {"client_mtime", static_cast<std::int64_t>(0)},
            {"docid", upload.parent_id},
            {"length", hashes->length},
            {"name", name},
            {"ondup", 1},
        }.dump();
        auto begin_response = send_json_request(std::move(begin_request), "北航云盘开始上传", upload.token);
        if (!begin_response) return make_error(begin_response.error().code, begin_response.error().message);
        auto begin_json = parse_cloud_json(*begin_response, "北航云盘开始上传");
        if (!begin_json) return make_error(begin_json.error().code, begin_json.error().message);
        auto auth = parse_authrequest(*begin_json);
        auto docid = first_json_string(*begin_json, {"docid", "id"});
        auto rev = first_json_string(*begin_json, {"rev", "revision"});
        auto body = read_upload_bytes(source, hashes->length);
        if (!body) return make_error(body.error().code, body.error().message);
        auto put_response = send_authrequest(m_http_client, auth, HttpMethod::Put, *body, "北航云盘上传文件内容");
        if (!put_response) return make_error(put_response.error().code, put_response.error().message);

        HttpRequest end_request;
        end_request.method = HttpMethod::Post;
        end_request.url = resolve_for_mode(kCloudEndUploadUrl, m_mode);
        apply_cloud_headers(end_request, m_mode);
        end_request.headers["Content-Type"] = "application/json";
        end_request.body = nlohmann::json{{"docid", docid}, {"rev", rev}}.dump();
        auto end_response = send_json_request(std::move(end_request), "北航云盘结束上传", upload.token);
        if (!end_response) return make_error(end_response.error().code, end_response.error().message);
        auto end_json = parse_cloud_json(*end_response, "北航云盘结束上传");
        if (!end_json) return make_error(end_json.error().code, end_json.error().message);
        return cloud_mutation(docid.empty() ? upload.parent_id : docid, "Cloud file uploaded", "uploaded",
                              {{"name", name}, {"parentId", upload.parent_id}, {"rev", rev}, {"fastUpload", "false"}});
    }

    HttpRequest init_request;
    init_request.method = HttpMethod::Post;
    init_request.url = resolve_for_mode(kCloudInitMultiUploadUrl, m_mode);
    apply_cloud_headers(init_request, m_mode);
    init_request.headers["Content-Type"] = "application/json";
    init_request.body = nlohmann::json{{"docid", upload.parent_id}, {"length", hashes->length}, {"name", name}, {"ondup", 1}}.dump();
    auto init_response = send_json_request(std::move(init_request), "北航云盘初始化分片上传", upload.token);
    if (!init_response) return make_error(init_response.error().code, init_response.error().message);
    auto init_json = parse_cloud_json(*init_response, "北航云盘初始化分片上传");
    if (!init_json) return make_error(init_json.error().code, init_json.error().message);
    auto init = parse_upload_init(*init_json, hashes->length);
    if (init.docid.empty() || init.rev.empty() || init.uploadid.empty()) return make_error(ErrorCode::ParseError, "北航云盘分片上传初始化响应缺少必要字段");

    HttpRequest part_request;
    part_request.method = HttpMethod::Post;
    part_request.url = resolve_for_mode(kCloudUploadPartUrl, m_mode);
    apply_cloud_headers(part_request, m_mode);
    part_request.headers["Content-Type"] = "application/json";
    part_request.body = upload_init_payload(init).dump();
    auto part_response = send_json_request(std::move(part_request), "北航云盘获取分片上传授权", upload.token);
    if (!part_response) return make_error(part_response.error().code, part_response.error().message);
    auto part_json = parse_cloud_json(*part_response, "北航云盘获取分片上传授权");
    if (!part_json) return make_error(part_json.error().code, part_json.error().message);
    auto parts = parse_part_authrequests(*part_json);
    if (parts.empty()) return make_error(ErrorCode::ParseError, "北航云盘分片上传授权为空");

    nlohmann::json part_info = nlohmann::json::object();
    std::uint64_t remaining = hashes->length;
    for (const auto &[index, auth] : parts) {
        const auto to_read = std::min<std::uint64_t>(kCloudUploadPartSize, remaining);
        auto body = read_upload_bytes(source, to_read);
        if (!body) return make_error(body.error().code, body.error().message);
        auto put_response = send_authrequest(m_http_client, auth, HttpMethod::Put, *body, "北航云盘上传分片");
        if (!put_response) return make_error(put_response.error().code, put_response.error().message);
        auto etag = extract_etag(*put_response);
        if (etag.empty()) return make_error(ErrorCode::ParseError, "北航云盘上传分片响应缺少 ETag");
        part_info[std::to_string(index)] = nlohmann::json::array({etag, to_read});
        remaining -= to_read;
        if (remaining == 0) break;
    }
    if (remaining != 0) return make_error(ErrorCode::ParseError, "北航云盘分片授权数量不足");

    HttpRequest complete_request;
    complete_request.method = HttpMethod::Post;
    complete_request.url = resolve_for_mode(kCloudCompleteUploadUrl, m_mode);
    apply_cloud_headers(complete_request, m_mode);
    complete_request.headers["Content-Type"] = "application/json";
    complete_request.body = nlohmann::json{{"docid", init.docid}, {"rev", init.rev}, {"uploadid", init.uploadid}, {"partinfo", part_info}}.dump();
    auto complete_response = send_json_request(std::move(complete_request), "北航云盘完成分片上传", upload.token);
    if (!complete_response) return make_error(complete_response.error().code, complete_response.error().message);
    if (complete_response->status_code < 200 || complete_response->status_code >= 300) {
        return make_error(ErrorCode::NetworkError, "北航云盘完成分片上传请求返回: " + std::to_string(complete_response->status_code));
    }
    auto complete = parse_complete_upload_response(complete_response->body);
    if (!complete) return make_error(complete.error().code, complete.error().message);
    auto complete_store_response = send_authrequest(m_http_client, complete->authrequest, HttpMethod::Post, complete->body, "北航云盘提交分片完成");
    if (!complete_store_response) return make_error(complete_store_response.error().code, complete_store_response.error().message);

    HttpRequest end_request;
    end_request.method = HttpMethod::Post;
    end_request.url = resolve_for_mode(kCloudEndUploadUrl, m_mode);
    apply_cloud_headers(end_request, m_mode);
    end_request.headers["Content-Type"] = "application/json";
    end_request.body = nlohmann::json{{"docid", init.docid}, {"rev", init.rev}}.dump();
    auto end_response = send_json_request(std::move(end_request), "北航云盘结束分片上传", upload.token);
    if (!end_response) return make_error(end_response.error().code, end_response.error().message);
    auto end_json = parse_cloud_json(*end_response, "北航云盘结束分片上传");
    if (!end_json) return make_error(end_json.error().code, end_json.error().message);
    return cloud_mutation(init.docid, "Cloud file uploaded", "uploaded",
                          {{"name", name}, {"parentId", upload.parent_id}, {"rev", init.rev}, {"fastUpload", "false"}});
}

Result<std::vector<Model::FeatureRecord>> CloudService::root_records(CloudRootKind kind) {
    auto result = roots(kind);
    if (!result) return make_error(result.error().code, result.error().message);
    std::vector<Model::FeatureRecord> records;
    records.reserve(result->size());
    for (const auto &item : *result) {
        auto record = item_record(item, "root");
        record.fields["root"] = root_kind_string(kind);
        records.push_back(std::move(record));
    }
    return records;
}

Result<Model::FeatureRecord> CloudService::user_root_record() {
    auto result = user_root();
    if (!result) return make_error(result.error().code, result.error().message);
    return item_record(*result, "root");
}

Result<std::vector<Model::FeatureRecord>> CloudService::list_records(const CloudListQuery &query) {
    auto result = list_dir(query);
    if (!result) return make_error(result.error().code, result.error().message);
    std::vector<Model::FeatureRecord> records;
    records.reserve(result->dirs.size() + result->files.size());
    for (const auto &item : result->dirs) records.push_back(item_record(item, "dir"));
    for (const auto &item : result->files) records.push_back(item_record(item, "file"));
    return records;
}

Result<Model::FeatureRecord> CloudService::size_record(const CloudListQuery &query) {
    auto result = item_size(query);
    if (!result) return make_error(result.error().code, result.error().message);
    return size_record_for(*result, query.doc_id);
}

Result<std::vector<Model::FeatureRecord>> CloudService::recycle_records() {
    auto result = recycle_bin();
    if (!result) return make_error(result.error().code, result.error().message);
    std::vector<Model::FeatureRecord> records;
    records.reserve(result->dirs.size() + result->files.size());
    for (const auto &item : result->dirs) records.push_back(item_record(item, "recycle-dir"));
    for (const auto &item : result->files) records.push_back(item_record(item, "recycle-file"));
    return records;
}

Result<std::vector<Model::FeatureRecord>> CloudService::share_records() {
    auto result = share_history();
    if (!result) return make_error(result.error().code, result.error().message);
    std::vector<Model::FeatureRecord> records;
    records.reserve(result->size());
    for (const auto &share : *result) records.push_back(share_record_for(share));
    return records;
}

Result<std::vector<Model::FeatureRecord>> CloudService::share_record_records(const std::string &item_id) {
    auto result = share_record(item_id);
    if (!result) return make_error(result.error().code, result.error().message);
    std::vector<Model::FeatureRecord> records;
    records.reserve(result->size());
    for (const auto &share : *result) records.push_back(share_record_for(share));
    return records;
}

Result<Model::FeatureRecord> CloudService::parsed_share_record(const std::string &id_or_url, const std::string &password) {
    auto result = share_parse(id_or_url, password);
    if (!result) return make_error(result.error().code, result.error().message);
    return item_record(*result, result->is_dir() ? "share-dir" : "share-file");
}

Result<Model::FeatureRecord> CloudService::download_url_record(const CloudItemRef &item) {
    auto result = download_url(item);
    if (!result) return make_error(result.error().code, result.error().message);
    return download_url_record_for(*result);
}

Result<Model::FeatureRecord> CloudService::batch_download_url_record(const std::vector<CloudItemRef> &items, const std::string &name) {
    auto result = batch_download_url(items, name);
    if (!result) return make_error(result.error().code, result.error().message);
    return download_url_record_for(*result);
}

} // namespace UBAANext
