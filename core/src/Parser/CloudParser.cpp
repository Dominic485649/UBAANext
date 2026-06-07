#include <UBAANext/Parser/CloudParser.hpp>

#include <sstream>

namespace UBAANext {
namespace Parser {
namespace {

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.is_object() || !json.contains(key) || json[key].is_null()) return {};
    const auto &value = json[key];
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<long long>());
    if (value.is_number_unsigned()) return std::to_string(value.get<unsigned long long>());
    if (value.is_number_float()) return std::to_string(value.get<double>());
    if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
    return {};
}

std::string first_string(const nlohmann::json &json, std::initializer_list<const char *> keys) {
    for (const auto *key : keys) {
        auto value = json_string(json, key);
        if (!value.empty()) return value;
    }
    return {};
}

const nlohmann::json &unwrap_array_or_object(const nlohmann::json &root) {
    if (root.is_object()) {
        for (const auto *key : {"result", "data", "list", "items"}) {
            if (root.contains(key)) return root[key];
        }
    }
    return root;
}

nlohmann::json array_from_keys(const nlohmann::json &root, std::initializer_list<const char *> keys) {
    if (!root.is_object()) return nlohmann::json::array();
    for (const auto *key : keys) {
        if (root.contains(key) && root[key].is_array()) return root[key];
    }
    return nlohmann::json::array();
}

Model::CloudItem parse_item(const nlohmann::json &item, const std::string &default_type) {
    Model::CloudItem parsed;
    parsed.id = first_string(item, {"docid", "id", "gnsId", "gns_id"});
    parsed.name = first_string(item, {"name", "title", "doc_lib_name"});
    parsed.type = first_string(item, {"type", "doc_lib_type", "entry_type"});
    parsed.parent_id = first_string(item, {"parent_docid", "parentId", "parent_id"});
    parsed.doc_lib_id = first_string(item, {"doc_lib_id", "docLibId", "library_id"});
    parsed.doc_lib_name = first_string(item, {"doc_lib_name", "docLibName", "library_name"});
    parsed.creator = first_string(item, {"creator", "created_by", "owner", "owner_name"});
    parsed.modifier = first_string(item, {"modifier", "modified_by", "editor"});
    parsed.created_at = first_string(item, {"create_time", "created_at", "create", "ctime"});
    parsed.updated_at = first_string(item, {"modified", "modified_at", "update_time", "mtime"});
    parsed.size = first_string(item, {"size", "totalsize"});
    parsed.revision = first_string(item, {"rev", "revision"});
    parsed.token = first_string(item, {"token", "link_token"});

    if (parsed.type.empty()) {
        if (parsed.size == "-1") {
            parsed.type = "dir";
        } else {
            parsed.type = default_type;
        }
    }
    return parsed;
}

std::string join_permissions(const nlohmann::json &permission) {
    if (permission.is_string()) return permission.get<std::string>();
    if (!permission.is_array()) return json_string(permission, "permission");
    std::ostringstream out;
    bool first = true;
    for (const auto &item : permission) {
        std::string value;
        if (item.is_string()) value = item.get<std::string>();
        else if (item.is_number_integer()) value = std::to_string(item.get<long long>());
        if (value.empty()) continue;
        if (!first) out << ',';
        first = false;
        out << value;
    }
    return out.str();
}

bool bool_from_share_type(const nlohmann::json &json) {
    const auto type = first_string(json, {"type", "doc_type"});
    return type == "folder" || type == "dir" || type == "doc_lib";
}

Model::CloudShare parse_share(const nlohmann::json &json) {
    Model::CloudShare share;
    share.id = first_string(json, {"id", "share_id", "link_id"});
    share.name = first_string(json, {"title", "name"});
    share.item_id = first_string(json, {"docid", "item_id", "document_id"});
    share.expires_at = first_string(json, {"expires_at", "expiration"});
    share.password = first_string(json, {"password", "pwd"});
    if (auto limit = first_string(json, {"limited_times", "limit"}); !limit.empty()) {
        try {
            share.limit = std::stoll(limit);
        } catch (...) {
            share.limit = -1;
        }
    }
    share.url = first_string(json, {"url", "link"});
    if (share.url.empty() && !share.id.empty()) share.url = "https://bhpan.buaa.edu.cn/link/" + share.id;
    if (json.contains("item") && json["item"].is_object()) {
        if (share.item_id.empty()) share.item_id = first_string(json["item"], {"id", "docid"});
        if (share.name.empty()) share.name = first_string(json["item"], {"name", "title"});
        share.is_dir = bool_from_share_type(json["item"]);
        if (json["item"].contains("permission")) share.permissions = join_permissions(json["item"]["permission"]);
        if (share.permissions.empty() && json["item"].contains("allow")) share.permissions = join_permissions(json["item"]["allow"]);
    }
    if (share.permissions.empty() && json.contains("permission")) share.permissions = join_permissions(json["permission"]);
    if (share.permissions.empty() && json.contains("allow")) share.permissions = join_permissions(json["allow"]);
    if (!share.is_dir) share.is_dir = bool_from_share_type(json);
    return share;
}

} // namespace

std::vector<Model::CloudItem> parse_cloud_roots(const nlohmann::json &root) {
    const auto &value = unwrap_array_or_object(root);
    std::vector<Model::CloudItem> roots;
    if (!value.is_array()) return roots;
    roots.reserve(value.size());
    for (const auto &item : value) {
        auto parsed = parse_item(item, "doc_lib");
        if (parsed.type.empty()) parsed.type = "doc_lib";
        if (!parsed.id.empty() || !parsed.name.empty()) roots.push_back(std::move(parsed));
    }
    return roots;
}

Model::CloudDir parse_cloud_dir(const nlohmann::json &root) {
    const auto &value = unwrap_array_or_object(root);
    Model::CloudDir dir;
    if (!value.is_object()) return dir;
    for (const auto &item : array_from_keys(value, {"dirs", "folders", "directories"})) {
        auto parsed = parse_item(item, "dir");
        if (parsed.type.empty()) parsed.type = "dir";
        if (!parsed.id.empty() || !parsed.name.empty()) dir.dirs.push_back(std::move(parsed));
    }
    for (const auto &item : array_from_keys(value, {"files", "documents"})) {
        auto parsed = parse_item(item, "file");
        if (parsed.type.empty()) parsed.type = "file";
        if (!parsed.id.empty() || !parsed.name.empty()) dir.files.push_back(std::move(parsed));
    }
    return dir;
}

Model::CloudSize parse_cloud_size(const nlohmann::json &root) {
    const auto &value = unwrap_array_or_object(root);
    Model::CloudSize size;
    if (!value.is_object()) return size;
    size.bytes = first_string(value, {"totalsize", "size", "bytes"});
    size.file_count = first_string(value, {"filenum", "file", "files"});
    size.dir_count = first_string(value, {"dirnum", "dir", "dirs"});
    return size;
}

std::vector<Model::CloudShare> parse_cloud_shares(const nlohmann::json &root) {
    const auto &value = unwrap_array_or_object(root);
    std::vector<Model::CloudShare> shares;
    if (!value.is_array()) return shares;
    shares.reserve(value.size());
    for (const auto &item : value) {
        auto share = parse_share(item);
        if (!share.id.empty() || !share.name.empty() || !share.url.empty()) shares.push_back(std::move(share));
    }
    return shares;
}

} // namespace Parser
} // namespace UBAANext
