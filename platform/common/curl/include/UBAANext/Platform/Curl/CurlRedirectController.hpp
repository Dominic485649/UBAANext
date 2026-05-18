#pragma once

#include <UBAANext/Net/RedirectController.hpp>

namespace UBAANext::Platform::Curl {

class CurlRedirectController : public UBAANext::IRedirectController {
public:
    [[nodiscard]] RedirectOptions defaults() const override;
};

} // namespace UBAANext::Platform::Curl
