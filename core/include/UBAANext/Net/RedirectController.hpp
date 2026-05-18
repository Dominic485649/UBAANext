#pragma once

namespace UBAANext {

enum class RedirectPostPolicy {
    PreserveMethod,
    AllowSwitchToGet
};

struct RedirectOptions {
    bool follow_redirects = true;
    int max_redirects = 10;
    bool expose_location_header = true;
    RedirectPostPolicy post_policy = RedirectPostPolicy::AllowSwitchToGet;
    bool restrict_to_http_https = true;
};

class IRedirectController {
public:
    virtual ~IRedirectController() = default;
    [[nodiscard]] virtual RedirectOptions defaults() const = 0;
};

} // namespace UBAANext
