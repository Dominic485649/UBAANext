#include <UBAANext/Protocol/CasFormParser.hpp>

#include <cctype>
#include <iomanip>
#include <map>
#include <regex>
#include <set>
#include <sstream>

namespace UBAANext::Protocol {

namespace {

std::string lower_copy(std::string value) {
    for (auto &ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

} // namespace

std::string form_url_encode(const std::string &value) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex << std::setfill('0');
    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '*') {
            encoded << static_cast<char>(ch);
        } else if (ch == ' ') {
            encoded << '+';
        } else {
            encoded << '%' << std::setw(2) << static_cast<int>(ch);
        }
    }
    return encoded.str();
}

std::string extract_execution(const std::string &html) {
    std::regex re(R"(<input[^>]*name\s*=\s*["']execution["'][^>]*value\s*=\s*["']([^"']*)["'])", std::regex::icase);
    std::smatch m;
    if (std::regex_search(html, m, re) && m.size() > 1) {
        return m[1].str();
    }
    std::regex re2(R"(<input[^>]*value\s*=\s*["']([^"']*)["'][^>]*name\s*=\s*["']execution["'])", std::regex::icase);
    if (std::regex_search(html, m, re2) && m.size() > 1) {
        return m[1].str();
    }
    return {};
}

std::string build_login_form(const std::string &html,
                             const std::string &username,
                             const std::string &password,
                             const std::string &execution,
                             const std::string &captcha) {
    std::string form;
    std::set<std::string> present_names;
    auto add = [&](const std::string &key, const std::string &value) {
        if (!form.empty()) form += "&";
        form += form_url_encode(key) + "=" + form_url_encode(value);
    };

    std::regex input_re(R"(<input\b([^>]*)>)", std::regex::icase);
    std::regex attr_re(R"(([a-zA-Z_:][-a-zA-Z0-9_:.]*)\s*=\s*["']([^"']*)["'])");
    auto inputs_begin = std::sregex_iterator(html.begin(), html.end(), input_re);
    auto inputs_end = std::sregex_iterator();
    for (auto it = inputs_begin; it != inputs_end; ++it) {
        std::string attrs_text = (*it)[1].str();
        std::map<std::string, std::string> attrs;
        auto attrs_begin = std::sregex_iterator(attrs_text.begin(), attrs_text.end(), attr_re);
        for (auto attr_it = attrs_begin; attr_it != inputs_end; ++attr_it) {
            attrs[lower_copy((*attr_it)[1].str())] = (*attr_it)[2].str();
        }
        auto name_it = attrs.find("name");
        if (name_it == attrs.end() || name_it->second.empty()) {
            continue;
        }
        std::string name = name_it->second;
        auto type_it = attrs.find("type");
        std::string type = type_it != attrs.end() ? lower_copy(type_it->second) : "";
        present_names.insert(name);
        if (name == "username" || name == "password" || name == "captcha" || type == "submit" || type == "button" || type == "image") {
            continue;
        }
        if (type == "checkbox" && attrs_text.find("checked") == std::string::npos) {
            continue;
        }
        auto value_it = attrs.find("value");
        std::string input_value = value_it != attrs.end() ? value_it->second : "";
        if (type == "hidden" || type == "checkbox" || !input_value.empty()) {
            add(name, input_value.empty() && type == "checkbox" ? "on" : input_value);
        }
    }

    add("username", username);
    add("password", password);
    if (!captcha.empty()) add("captcha", captcha);
    add("submit", "登录");
    if (present_names.find("execution") == present_names.end()) add("execution", execution);
    if (present_names.find("_eventId") == present_names.end()) add("_eventId", "submit");
    if (present_names.find("type") == present_names.end()) add("type", "username_password");
    return form;
}

} // namespace UBAANext::Protocol
