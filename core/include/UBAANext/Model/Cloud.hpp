#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace UBAANext {
namespace Model {

struct CloudItem {
    std::string id;
    std::string name;
    std::string type;
    std::string parent_id;
    std::string doc_lib_id;
    std::string doc_lib_name;
    std::string creator;
    std::string modifier;
    std::string created_at;
    std::string updated_at;
    std::string size;
    std::string revision;
    std::string token;

    [[nodiscard]] bool is_dir() const { return size == "-1" || type == "dir" || type == "folder" || type == "doc_lib"; }
};

struct CloudDir {
    std::vector<CloudItem> dirs;
    std::vector<CloudItem> files;
};

struct CloudSize {
    std::string bytes;
    std::string file_count;
    std::string dir_count;
};

struct CloudShare {
    std::string id;
    std::string name;
    std::string url;
    std::string item_id;
    bool is_dir = false;
    std::string expires_at;
    std::string password;
    std::int64_t limit = -1;
    std::string permissions;
};

struct CloudSharePermission {
    bool create = false;
    bool modify = false;
    bool download = false;
    bool preview = false;
    bool display = true;
};

struct CloudDownloadUrl {
    std::string url;
    std::string name;
    bool zipped = false;
};

struct CloudDownloadChunk {
    std::vector<unsigned char> bytes;
    std::uint64_t offset = 0;
    bool partial = false;
};

struct CloudUploadResult {
    std::string id;
    std::string revision;
    std::string name;
    std::string parent_id;
    bool fast_upload = false;
};

} // namespace Model
} // namespace UBAANext
