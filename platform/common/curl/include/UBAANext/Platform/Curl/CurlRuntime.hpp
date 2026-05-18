#pragma once

namespace UBAANext::Platform::Curl {

class CurlRuntime {
public:
    CurlRuntime();
    ~CurlRuntime();

    CurlRuntime(const CurlRuntime &) = delete;
    CurlRuntime &operator=(const CurlRuntime &) = delete;
};

} // namespace UBAANext::Platform::Curl
