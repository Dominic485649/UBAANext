#include <UBAANext/Service/BykcService.hpp>

#include <UBAANext/Net/VpnCipher.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <map>
#include <memory>
#include <random>
#include <regex>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#endif

namespace UBAANext {

namespace {

#ifdef _WIN32
constexpr const char *rsa_public_key_base64 = "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDlHMQ3B5GsWnCe7Nlo1YiG/YmHdlOiKOST5aRm4iaqYSvhvWmwcigoyWTM+8bv2+sf6nQBRDWTY4KmNV7DBk1eDnTIQo6ENA31k5/tYCLEXgjPbEjCK9spiyB62fCT6cqOhbamJB0lcDJRO6Vo1m3dy+fD0jbxfDVBBNtyltIsDQIDAQAB";
#endif
constexpr const char *key_chars = "ABCDEFGHJKMNPQRSTWXYZabcdefhijkmnprstwxyz2345678";

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    return mode == ConnectionMode::WebVPN ? VpnCipher::to_vpn_url(url) : url;
}

std::string header_value(const HttpResponse &response, const std::string &name) {
    for (const auto &[key, value] : response.headers) {
        if (key.size() != name.size()) continue;
        bool same = true;
        for (size_t i = 0; i < key.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(key[i])) != std::tolower(static_cast<unsigned char>(name[i]))) {
                same = false;
                break;
            }
        }
        if (same) {
            auto newline = value.find('\n');
            return newline == std::string::npos ? value : value.substr(0, newline);
        }
    }
    return {};
}

bool response_is_login(const HttpResponse &response) {
    return response.status_code == 401 || response.status_code == 403 ||
           response.body.find("name=\"execution\"") != std::string::npos ||
           response.body.find("统一身份认证") != std::string::npos;
}

std::string base64_encode(const std::vector<unsigned char> &data) {
    static constexpr char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0;
    int valb = -6;
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::vector<unsigned char> base64_decode(const std::string &input) {
    std::array<int, 256> table{};
    table.fill(-1);
    for (int i = 0; i < 64; ++i) table[static_cast<unsigned char>("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i])] = i;
    std::vector<unsigned char> out;
    int val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (std::isspace(c) || c == '=') continue;
        if (table[c] == -1) break;
        val = (val << 6) + table[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string bytes_to_hex(const std::vector<unsigned char> &bytes) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto byte : bytes) out << std::setw(2) << static_cast<int>(byte);
    return out.str();
}

std::vector<unsigned char> string_bytes(const std::string &text) {
    return {text.begin(), text.end()};
}

#ifdef _WIN32

struct AlgHandle {
    BCRYPT_ALG_HANDLE handle = nullptr;
    ~AlgHandle() { if (handle) BCryptCloseAlgorithmProvider(handle, 0); }
};

struct KeyHandle {
    BCRYPT_KEY_HANDLE handle = nullptr;
    ~KeyHandle() { if (handle) BCryptDestroyKey(handle); }
};

struct CryptoKeyHandle {
    HCRYPTKEY handle = 0;
    ~CryptoKeyHandle() { if (handle) CryptDestroyKey(handle); }
};

struct CryptoProviderHandle {
    HCRYPTPROV handle = 0;
    ~CryptoProviderHandle() { if (handle) CryptReleaseContext(handle, 0); }
};

Result<std::vector<unsigned char>> sha1_digest(const std::vector<unsigned char> &data) {
    AlgHandle alg;
    if (BCryptOpenAlgorithmProvider(&alg.handle, BCRYPT_SHA1_ALGORITHM, nullptr, 0) != 0) return make_error(ErrorCode::NetworkError, "打开 SHA-1 算法失败");
    DWORD hash_len = 0;
    DWORD cb = 0;
    if (BCryptGetProperty(alg.handle, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_len), sizeof(hash_len), &cb, 0) != 0) return make_error(ErrorCode::NetworkError, "获取 SHA-1 长度失败");
    std::vector<unsigned char> hash(hash_len);
    if (BCryptHash(alg.handle, nullptr, 0, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), hash.data(), static_cast<ULONG>(hash.size())) != 0) return make_error(ErrorCode::NetworkError, "计算 SHA-1 失败");
    return hash;
}

Result<std::vector<unsigned char>> aes_ecb_pkcs5_encrypt(const std::vector<unsigned char> &data, const std::string &key) {
    AlgHandle alg;
    if (BCryptOpenAlgorithmProvider(&alg.handle, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return make_error(ErrorCode::NetworkError, "打开 AES 算法失败");
    if (BCryptSetProperty(alg.handle, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_ECB)), static_cast<ULONG>((wcslen(BCRYPT_CHAIN_MODE_ECB) + 1) * sizeof(wchar_t)), 0) != 0) return make_error(ErrorCode::NetworkError, "设置 AES ECB 模式失败");
    KeyHandle key_handle;
    auto key_bytes = string_bytes(key);
    if (BCryptGenerateSymmetricKey(alg.handle, &key_handle.handle, nullptr, 0, key_bytes.data(), static_cast<ULONG>(key_bytes.size()), 0) != 0) return make_error(ErrorCode::NetworkError, "生成 AES 密钥失败");
    ULONG out_len = 0;
    if (BCryptEncrypt(key_handle.handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr, nullptr, 0, nullptr, 0, &out_len, BCRYPT_BLOCK_PADDING) != 0) return make_error(ErrorCode::NetworkError, "计算 AES 输出长度失败");
    std::vector<unsigned char> out(out_len);
    if (BCryptEncrypt(key_handle.handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr, nullptr, 0, out.data(), static_cast<ULONG>(out.size()), &out_len, BCRYPT_BLOCK_PADDING) != 0) return make_error(ErrorCode::NetworkError, "AES 加密失败");
    out.resize(out_len);
    return out;
}

Result<std::vector<unsigned char>> aes_ecb_pkcs5_decrypt(const std::vector<unsigned char> &data, const std::string &key) {
    AlgHandle alg;
    if (BCryptOpenAlgorithmProvider(&alg.handle, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return make_error(ErrorCode::NetworkError, "打开 AES 算法失败");
    if (BCryptSetProperty(alg.handle, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_ECB)), static_cast<ULONG>((wcslen(BCRYPT_CHAIN_MODE_ECB) + 1) * sizeof(wchar_t)), 0) != 0) return make_error(ErrorCode::NetworkError, "设置 AES ECB 模式失败");
    KeyHandle key_handle;
    auto key_bytes = string_bytes(key);
    if (BCryptGenerateSymmetricKey(alg.handle, &key_handle.handle, nullptr, 0, key_bytes.data(), static_cast<ULONG>(key_bytes.size()), 0) != 0) return make_error(ErrorCode::NetworkError, "生成 AES 密钥失败");
    ULONG out_len = 0;
    if (BCryptDecrypt(key_handle.handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr, nullptr, 0, nullptr, 0, &out_len, BCRYPT_BLOCK_PADDING) != 0) return make_error(ErrorCode::NetworkError, "计算 AES 解密输出长度失败");
    std::vector<unsigned char> out(out_len);
    if (BCryptDecrypt(key_handle.handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr, nullptr, 0, out.data(), static_cast<ULONG>(out.size()), &out_len, BCRYPT_BLOCK_PADDING) != 0) return make_error(ErrorCode::NetworkError, "AES 解密失败");
    out.resize(out_len);
    return out;
}

Result<std::string> rsa_pkcs1_encrypt_base64(const std::vector<unsigned char> &data) {
    auto der = base64_decode(rsa_public_key_base64);
    CERT_PUBLIC_KEY_INFO *info = nullptr;
    DWORD info_len = 0;
    if (!CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, X509_PUBLIC_KEY_INFO, der.data(), static_cast<DWORD>(der.size()), CRYPT_DECODE_ALLOC_FLAG, nullptr, &info, &info_len)) return make_error(ErrorCode::NetworkError, "解析 BYKC RSA 公钥失败");
    std::unique_ptr<CERT_PUBLIC_KEY_INFO, decltype(&LocalFree)> info_guard(info, LocalFree);

    CryptoProviderHandle provider;
    if (!CryptAcquireContextW(&provider.handle, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return make_error(ErrorCode::NetworkError, "初始化 CryptoAPI 失败");
    CryptoKeyHandle key;
    if (!CryptImportPublicKeyInfo(provider.handle, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, info, &key.handle)) return make_error(ErrorCode::NetworkError, "导入 BYKC RSA 公钥失败");

    DWORD key_len_bits = 0;
    DWORD key_len_size = sizeof(key_len_bits);
    if (!CryptGetKeyParam(key.handle, KP_KEYLEN, reinterpret_cast<BYTE *>(&key_len_bits), &key_len_size, 0)) return make_error(ErrorCode::NetworkError, "获取 BYKC RSA 密钥长度失败");
    DWORD block_len = key_len_bits / 8;
    std::vector<unsigned char> buffer(block_len);
    if (data.size() > block_len - 11) return make_error(ErrorCode::InvalidArgument, "BYKC RSA 明文过长");
    std::copy(data.begin(), data.end(), buffer.begin());
    DWORD data_len = static_cast<DWORD>(data.size());
    if (!CryptEncrypt(key.handle, 0, TRUE, 0, buffer.data(), &data_len, static_cast<DWORD>(buffer.size()))) return make_error(ErrorCode::NetworkError, "BYKC RSA 加密失败");
    buffer.resize(data_len);
    std::reverse(buffer.begin(), buffer.end());
    return base64_encode(buffer);
}

#else
Result<std::vector<unsigned char>> sha1_digest(const std::vector<unsigned char> &) { return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 BYKC SHA-1 加密实现"); }
Result<std::vector<unsigned char>> aes_ecb_pkcs5_encrypt(const std::vector<unsigned char> &, const std::string &) { return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 BYKC AES 加密实现"); }
Result<std::vector<unsigned char>> aes_ecb_pkcs5_decrypt(const std::vector<unsigned char> &, const std::string &) { return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 BYKC AES 解密实现"); }
Result<std::string> rsa_pkcs1_encrypt_base64(const std::vector<unsigned char> &) { return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 BYKC RSA 加密实现"); }
#endif

std::string generate_aes_key() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::size_t> dist(0, std::char_traits<char>::length(key_chars) - 1);
    std::string key;
    key.reserve(16);
    for (int i = 0; i < 16; ++i) key.push_back(key_chars[dist(gen)]);
    return key;
}

Result<std::tuple<std::string, std::string, std::string, std::string>> encrypt_request(const std::string &json) {
    auto key = generate_aes_key();
    auto encrypted = aes_ecb_pkcs5_encrypt(string_bytes(json), key);
    if (!encrypted) return make_error(encrypted.error().code, encrypted.error().message);
    auto digest = sha1_digest(string_bytes(json));
    if (!digest) return make_error(digest.error().code, digest.error().message);
    auto ak = rsa_pkcs1_encrypt_base64(string_bytes(key));
    if (!ak) return make_error(ak.error().code, ak.error().message);
    auto sign_hex = bytes_to_hex(*digest);
    auto sk = rsa_pkcs1_encrypt_base64(string_bytes(sign_hex));
    if (!sk) return make_error(sk.error().code, sk.error().message);
    return std::make_tuple(base64_encode(*encrypted), *ak, *sk, key);
}

std::int64_t now_millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_string()) return json[key].get<std::string>();
    if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    if (json[key].is_number_float()) return std::to_string(json[key].get<double>());
    if (json[key].is_boolean()) return json[key].get<bool>() ? "true" : "false";
    return {};
}

std::string nested_string(const nlohmann::json &json, const char *object_key, const char *key) {
    if (!json.contains(object_key) || !json[object_key].is_object()) return {};
    return json_string(json[object_key], key);
}

Model::FeatureRecord make_record(std::string id, std::string title, std::string status, std::map<std::string, std::string> fields = {}) {
    Model::FeatureRecord record;
    record.id = std::move(id);
    record.title = std::move(title);
    record.status = std::move(status);
    record.fields = std::move(fields);
    return record;
}

std::string status_for_course(const nlohmann::json &course) {
    if (course.value("selected", false)) return "selected";
    auto max_count = json_string(course, "courseMaxCount");
    auto current = json_string(course, "courseCurrentCount");
    if (!max_count.empty() && !current.empty()) {
        try {
            if (std::stoi(current) >= std::stoi(max_count)) return "full";
        } catch (...) {}
    }
    return "available";
}

} // namespace

BykcService::BykcService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<void> BykcService::ensure_login(bool force_refresh) {
    (void)m_cache;
    if (!force_refresh && !m_token.empty()) return {};

    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode("https://bykc.buaa.edu.cn/sscv/cas/login", m_mode);
    request.headers["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
    request.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) UBAANext/0.4";
    auto response = m_http_client.send(request);
    if (!response) return make_error(ErrorCode::NetworkError, "博雅登录失败: " + response.error().message);
    if (response_is_login(*response)) return make_error(ErrorCode::SessionExpired, "博雅会话已过期，请重新登录");

    std::regex token_re(R"([?&]token=([^&#]+))");
    std::smatch match;
    auto location = header_value(*response, "Location");
    if (std::regex_search(location, match, token_re) && match.size() > 1) m_token = match[1].str();
    if (m_token.empty() && std::regex_search(request.url, match, token_re) && match.size() > 1) m_token = match[1].str();
    if (m_token.empty()) {
        HttpRequest fallback;
        fallback.method = HttpMethod::Get;
        fallback.url = resolve_for_mode("https://bykc.buaa.edu.cn/cas-login?token=", m_mode);
        fallback.headers["User-Agent"] = request.headers["User-Agent"];
        auto fallback_response = m_http_client.send(fallback);
        if (!fallback_response) return make_error(ErrorCode::NetworkError, "博雅 CAS 登录失败: " + fallback_response.error().message);
        if (response_is_login(*fallback_response)) return make_error(ErrorCode::SessionExpired, "博雅会话已过期，请重新登录");
        auto fallback_location = header_value(*fallback_response, "Location");
        if (std::regex_search(fallback_location, match, token_re) && match.size() > 1) m_token = match[1].str();
    }
    if (m_token.empty()) return make_error(ErrorCode::SessionExpired, "未获取到博雅 auth token");
    return {};
}

Result<std::string> BykcService::call_api_raw(const std::string &api_name, const nlohmann::json &payload, bool allow_retry) {
    auto login = ensure_login();
    if (!login) return make_error(login.error().code, login.error().message);
    auto plain = payload.dump();
    auto encrypted = encrypt_request(plain);
    if (!encrypted) return make_error(encrypted.error().code, encrypted.error().message);
    auto [body, ak, sk, aes_key] = *encrypted;

    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode("https://bykc.buaa.edu.cn/sscv/" + api_name, m_mode);
    request.headers["Content-Type"] = "application/json; charset=UTF-8";
    request.headers["Accept"] = "application/json";
    request.headers["Referer"] = resolve_for_mode("https://bykc.buaa.edu.cn/system/course-select", m_mode);
    request.headers["Origin"] = resolve_for_mode("https://bykc.buaa.edu.cn", m_mode);
    request.headers["auth_token"] = m_token;
    request.headers["authtoken"] = m_token;
    request.headers["ak"] = ak;
    request.headers["sk"] = sk;
    request.headers["ts"] = std::to_string(now_millis());
    request.body = body;

    auto response = m_http_client.send(request);
    if (!response) return make_error(ErrorCode::NetworkError, "请求博雅失败: " + response.error().message);
    if (response_is_login(*response)) {
        m_token.clear();
        if (allow_retry) return call_api_raw(api_name, payload, false);
        return make_error(ErrorCode::SessionExpired, "博雅会话已过期，请重新登录");
    }
    if (response->status_code != 200) return make_error(ErrorCode::NetworkError, "博雅请求返回: " + std::to_string(response->status_code));

    auto encoded_json = nlohmann::json::parse(response->body, nullptr, false);
    std::string encoded_payload = encoded_json.is_string() ? encoded_json.get<std::string>() : response->body;
    auto encrypted_response = base64_decode(encoded_payload);
    auto decrypted = aes_ecb_pkcs5_decrypt(encrypted_response, aes_key);
    std::string decoded = decrypted ? std::string(decrypted->begin(), decrypted->end()) : encoded_payload;
    if (decoded.find("会话已失效") != std::string::npos || decoded.find("未登录") != std::string::npos) {
        m_token.clear();
        if (allow_retry) return call_api_raw(api_name, payload, false);
        return make_error(ErrorCode::SessionExpired, "博雅会话已过期，请重新登录");
    }
    return decoded;
}

Result<nlohmann::json> BykcService::call_api_data(const std::string &api_name, const nlohmann::json &payload, const std::string &fallback_message) {
    auto raw = call_api_raw(api_name, payload);
    if (!raw) return make_error(raw.error().code, raw.error().message);
    auto json = nlohmann::json::parse(*raw, nullptr, false);
    if (json.is_discarded()) return make_error(ErrorCode::ParseError, "解析博雅 JSON 失败");
    bool success = json.value("success", json.value("isSuccess", false));
    if (!success || !json.contains("data") || json["data"].is_null()) return make_error(ErrorCode::NetworkError, json.value("errmsg", fallback_message));
    return json["data"];
}

Result<std::vector<Model::FeatureRecord>> BykcService::profile() {
    auto data = call_api_data("getUserProfile", nlohmann::json::object(), "博雅资料加载失败");
    if (!data) return make_error(data.error().code, data.error().message);
    return std::vector<Model::FeatureRecord>{make_record(json_string(*data, "id"), json_string(*data, "realName"), "active", {
        {"employeeId", json_string(*data, "employeeId")}, {"studentNo", json_string(*data, "studentNo")}, {"studentType", json_string(*data, "studentType")},
        {"classCode", json_string(*data, "classCode")}, {"collegeName", nested_string(*data, "college", "collegeName")}, {"termName", nested_string(*data, "term", "termName")}})};
}

Result<std::vector<Model::FeatureRecord>> BykcService::courses(int page, int size, bool all) {
    BykcCourseQuery query;
    query.page = page;
    query.size = size;
    query.all = all;
    return courses(query);
}

Result<std::vector<Model::FeatureRecord>> BykcService::courses(const BykcCourseQuery &query) {
    nlohmann::json payload{{"pageNumber", query.page < 1 ? 1 : query.page}, {"pageSize", query.size < 1 ? 100 : query.size}, {"all", query.all}};
    if (!query.status.empty()) payload["status"] = query.status;
    if (!query.category.empty()) payload["category"] = query.category;
    if (!query.sub_category.empty()) payload["subCategory"] = query.sub_category;
    if (!query.campus.empty()) payload["campus"] = query.campus;
    if (!query.keyword.empty()) payload["keyword"] = query.keyword;

    auto data = call_api_data("queryStudentSemesterCourseByPage", payload, "博雅课程列表加载失败");
    if (!data) return make_error(data.error().code, data.error().message);
    auto content = data->contains("content") && (*data)["content"].is_array() ? (*data)["content"] : nlohmann::json::array();
    std::vector<Model::FeatureRecord> records;
    for (const auto &course : content) {
        auto status = status_for_course(course);
        auto title = json_string(course, "courseName");
        auto category = nested_string(course, "courseNewKind1", "kindName");
        auto sub_category = nested_string(course, "courseNewKind2", "kindName");
        auto position = json_string(course, "coursePosition");
        if (!query.status.empty() && status != query.status) continue;
        if (!query.category.empty() && category.find(query.category) == std::string::npos) continue;
        if (!query.sub_category.empty() && sub_category.find(query.sub_category) == std::string::npos) continue;
        if (!query.campus.empty() && position.find(query.campus) == std::string::npos) continue;
        if (!query.keyword.empty() && title.find(query.keyword) == std::string::npos && json_string(course, "courseTeacher").find(query.keyword) == std::string::npos) continue;
        records.push_back(make_record(json_string(course, "id"), title, status, {
            {"teacher", json_string(course, "courseTeacher")}, {"position", position}, {"startDate", json_string(course, "courseStartDate")},
            {"endDate", json_string(course, "courseEndDate")}, {"selectStartDate", json_string(course, "courseSelectStartDate")}, {"selectEndDate", json_string(course, "courseSelectEndDate")},
            {"cancelEndDate", json_string(course, "courseCancelEndDate")}, {"maxCount", json_string(course, "courseMaxCount")}, {"currentCount", json_string(course, "courseCurrentCount")},
            {"category", category}, {"subCategory", sub_category}, {"selected", json_string(course, "selected")}}));
    }
    return records;
}

Result<Model::FeatureRecord> BykcService::show_course(const std::string &course_id) {
    if (course_id.empty()) return make_error(ErrorCode::InvalidArgument, "bykc course show 需要 --course-id");
    auto data = call_api_data("queryCourseById", nlohmann::json{{"id", std::stoll(course_id)}}, "博雅课程详情加载失败");
    if (!data) return make_error(data.error().code, data.error().message);
    return make_record(course_id, json_string(*data, "courseName"), status_for_course(*data), {
        {"teacher", json_string(*data, "courseTeacher")}, {"position", json_string(*data, "coursePosition")}, {"contact", json_string(*data, "courseContact")},
        {"mobile", json_string(*data, "courseContactMobile")}, {"desc", json_string(*data, "courseDesc")}, {"startDate", json_string(*data, "courseStartDate")},
        {"endDate", json_string(*data, "courseEndDate")}, {"selected", json_string(*data, "selected")}, {"signConfig", json_string(*data, "courseSignConfig")}});
}

Result<std::vector<Model::FeatureRecord>> BykcService::chosen() {
    auto config = call_api_data("getAllConfig", nlohmann::json::object(), "博雅配置加载失败");
    if (!config) return make_error(config.error().code, config.error().message);
    auto semesters = config->contains("semester") && (*config)["semester"].is_array() ? (*config)["semester"] : nlohmann::json::array();
    if (semesters.empty()) return make_error(ErrorCode::ParseError, "无法获取当前博雅学期信息");
    auto semester = semesters.back();
    auto start = json_string(semester, "semesterStartDate");
    auto end = json_string(semester, "semesterEndDate");
    auto data = call_api_data("queryChosenCourse", nlohmann::json{{"startDate", start}, {"endDate", end}}, "博雅已选课程加载失败");
    if (!data) return make_error(data.error().code, data.error().message);
    auto list = data->contains("courseList") && (*data)["courseList"].is_array() ? (*data)["courseList"] : nlohmann::json::array();
    std::vector<Model::FeatureRecord> records;
    for (const auto &chosen_course : list) {
        auto course = chosen_course.contains("courseInfo") && chosen_course["courseInfo"].is_object() ? chosen_course["courseInfo"] : nlohmann::json::object();
        records.push_back(make_record(json_string(chosen_course, "id"), json_string(course, "courseName"), "selected", {
            {"courseId", json_string(course, "id")}, {"teacher", json_string(course, "courseTeacher")}, {"position", json_string(course, "coursePosition")},
            {"selectDate", json_string(chosen_course, "selectDate")}, {"checkin", json_string(chosen_course, "checkin")}, {"pass", json_string(chosen_course, "pass")},
            {"score", json_string(chosen_course, "score")}, {"homework", json_string(chosen_course, "homework")}, {"signInfo", json_string(chosen_course, "signInfo")}}));
    }
    return records;
}

Result<std::vector<Model::FeatureRecord>> BykcService::stats() {
    auto data = call_api_data("queryStatisticByUserId", nlohmann::json::object(), "博雅统计加载失败");
    if (!data) return make_error(data.error().code, data.error().message);
    std::vector<Model::FeatureRecord> records;
    records.push_back(make_record("total", "累计有效修读", "ok", {{"validCount", json_string(*data, "validCount")}}));
    if (data->contains("statistical") && (*data)["statistical"].is_object()) {
        for (const auto &[category, sub_map] : (*data)["statistical"].items()) {
            if (!sub_map.is_object()) continue;
            for (const auto &[sub_category, entry] : sub_map.items()) {
                records.push_back(make_record(category + ":" + sub_category, sub_category, "ok", {{"category", category}, {"requiredCount", json_string(entry, "assessmentCount")}, {"passedCount", json_string(entry, "completeAssessmentCount")}}));
            }
        }
    }
    return records;
}

Result<Model::MutationResult> BykcService::select_course(const std::string &course_id) {
    if (course_id.empty()) return make_error(ErrorCode::InvalidArgument, "bykc select 需要 --course-id");
    auto data = call_api_data("choseCourse", nlohmann::json{{"courseId", std::stoll(course_id)}}, "选课失败");
    if (!data) return make_error(data.error().code, data.error().message);
    Model::MutationResult result;
    result.accepted = true;
    result.message = "选课成功";
    result.summary = make_record(course_id, "博雅选课", "selected", {{"raw", data->dump()}});
    return result;
}

Result<Model::MutationResult> BykcService::unselect_course(const std::string &course_id) {
    if (course_id.empty()) return make_error(ErrorCode::InvalidArgument, "bykc unselect 需要 --course-id 或已选记录 id");
    auto data = call_api_data("delChosenCourse", nlohmann::json{{"id", std::stoll(course_id)}}, "退选失败");
    if (!data) return make_error(data.error().code, data.error().message);
    Model::MutationResult result;
    result.accepted = true;
    result.message = "退选成功";
    result.summary = make_record(course_id, "博雅退选", "unselected", {{"raw", data->dump()}});
    return result;
}

Result<Model::MutationResult> BykcService::sign_course(const std::string &course_id, double lat, double lng, int sign_type) {
    if (course_id.empty()) return make_error(ErrorCode::InvalidArgument, "bykc sign 需要 --course-id");
    if (sign_type != 1 && sign_type != 2) return make_error(ErrorCode::InvalidArgument, "bykc sign 需要 --sign-type 1 或 2");
    auto data = call_api_data("signCourseByUser", nlohmann::json{{"courseId", std::stoll(course_id)}, {"signLat", lat}, {"signLng", lng}, {"signType", sign_type}}, sign_type == 1 ? "签到失败" : "签退失败");
    if (!data) return make_error(data.error().code, data.error().message);
    Model::MutationResult result;
    result.accepted = true;
    result.message = sign_type == 1 ? "签到成功" : "签退成功";
    result.summary = make_record(course_id, sign_type == 1 ? "博雅签到" : "博雅签退", sign_type == 1 ? "signed" : "signed-out", {{"raw", data->dump()}});
    return result;
}

} // namespace UBAANext
