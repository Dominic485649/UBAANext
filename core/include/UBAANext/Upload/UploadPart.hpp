/**
 * @file UploadPart.hpp
 * @brief Preloaded upload bytes boundary.
 *
 * Placeholder/Upload boundary：该结构只承载已由上层读取的 bytes，本身不读取本地文件、不触发远端请求，也不代表业务上传已实现。
 */
#pragma once

#include <string>
#include <vector>

namespace UBAANext {

struct UploadPart {
    std::string field_name;
    /** Sensitive input: filename may expose local or business context and must be redacted in diagnostics. */
    std::string filename;
    std::string content_type;
    /** Sensitive input: upload bytes may contain private documents or photos and must not be logged. */
    std::vector<unsigned char> bytes;
};

} // namespace UBAANext
