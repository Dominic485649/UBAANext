#pragma once

#include <UBAANext/Base/Result.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace UBAANext {

/**
 * Platform-neutral upload stream.
 *
 * Core services consume bytes through this interface only. CLI/platform layers own
 * local path validation, file permissions, MIME inference, and actual file handles.
 */
class IUploadSource {
public:
    virtual ~IUploadSource() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::string content_type() const = 0;
    [[nodiscard]] virtual Result<std::uint64_t> size() = 0;
    [[nodiscard]] virtual Result<void> rewind() = 0;
    [[nodiscard]] virtual Result<std::size_t> read(unsigned char *buffer, std::size_t max_bytes) = 0;
};

} // namespace UBAANext
