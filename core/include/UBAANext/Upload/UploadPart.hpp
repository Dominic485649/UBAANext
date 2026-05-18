#pragma once

#include <string>
#include <vector>

namespace UBAANext {

struct UploadPart {
    std::string field_name;
    std::string filename;
    std::string content_type;
    std::vector<unsigned char> bytes;
};

} // namespace UBAANext
