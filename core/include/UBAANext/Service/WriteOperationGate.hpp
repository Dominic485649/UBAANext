#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Platform/PlatformCapabilities.hpp>

#include <string>

namespace UBAANext {

struct WriteOperationGate {
    bool confirmed = false;
    bool allow_write_operations = false;
    std::string operation;
};

/** WriteGated guard: returns InvalidArgument when not confirmed and UnsupportedPlatform when capability is disabled. */
[[nodiscard]] Result<void> require_write_operation(const WriteOperationGate &gate);

/** Produces a fail-closed gate for write operations that must not run on the current platform. */
[[nodiscard]] WriteOperationGate disabled_write_operation(std::string operation);

/** Combines CLI/API confirmation with PlatformCapabilities::write_operations for typed service writes. */
[[nodiscard]] WriteOperationGate confirmed_write_operation(const PlatformCapabilities &capabilities,
                                                           std::string operation,
                                                           bool confirmed = true);

} // namespace UBAANext
