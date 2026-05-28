#include <UBAANext/Service/WriteOperationGate.hpp>

namespace UBAANext {

Result<void> require_write_operation(const WriteOperationGate &gate) {
    if (!gate.confirmed) {
        return make_error(ErrorCode::InvalidArgument, gate.operation + " 是有副作用操作，必须显式确认");
    }
    if (!gate.allow_write_operations) {
        return make_error(ErrorCode::UnsupportedPlatform, gate.operation + " 当前平台未启用真实写操作");
    }
    return {};
}

WriteOperationGate disabled_write_operation(std::string operation) {
    WriteOperationGate gate;
    gate.operation = std::move(operation);
    return gate;
}

WriteOperationGate confirmed_write_operation(const PlatformCapabilities &capabilities,
                                             std::string operation,
                                             bool confirmed) {
    WriteOperationGate gate;
    gate.confirmed = confirmed;
    gate.allow_write_operations = capabilities.write_operations;
    gate.operation = std::move(operation);
    return gate;
}

} // namespace UBAANext
