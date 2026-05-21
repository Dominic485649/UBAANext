#pragma once

#include <UBAANext/Base/Result.hpp>

namespace UBAANext::Platform::Curl {

class CurlRuntime {
public:
    CurlRuntime();
    ~CurlRuntime();

    CurlRuntime(const CurlRuntime &) = delete;
    CurlRuntime &operator=(const CurlRuntime &) = delete;

    [[nodiscard]] bool ok() const noexcept { return m_ok; }
    [[nodiscard]] Error error() const { return m_error; }

private:
    bool m_ok = false;
    Error m_error;
};

} // namespace UBAANext::Platform::Curl
