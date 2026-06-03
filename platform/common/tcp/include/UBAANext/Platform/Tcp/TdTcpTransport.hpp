#pragma once

#include <UBAANext/Protocol/TdClient.hpp>

namespace UBAANext::Platform::Tcp {

class TdTcpTransport final : public Protocol::Td::ITdTransport {
public:
    [[nodiscard]] Result<Protocol::Td::ByteVector> exchange(const Protocol::Td::TdEndpoint &endpoint,
                                                            const Protocol::Td::ByteVector &request_frame) override;
};

} // namespace UBAANext::Platform::Tcp
