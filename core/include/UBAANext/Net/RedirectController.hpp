/**
 * @file RedirectController.hpp
 * @brief Redirect policy boundary for CAS/downstream session navigation.
 *
 * PartiallyMigrated: redirect handling must keep URL query redacted and does not prove downstream business semantics.
 */
#pragma once

namespace UBAANext {

enum class RedirectPostPolicy {
    PreserveMethod,
    AllowSwitchToGet
};

struct RedirectOptions {
    /** PartiallyMigrated: true may trigger additional remote requests during navigation. */
    bool follow_redirects = true;
    int max_redirects = 10;
    /** Sensitive output: Location headers can contain ticket/token query parameters. */
    bool expose_location_header = true;
    RedirectPostPolicy post_policy = RedirectPostPolicy::AllowSwitchToGet;
    /** Security boundary: redirects must remain constrained to http/https unless explicitly changed. */
    bool restrict_to_http_https = true;
};

class IRedirectController {
public:
    virtual ~IRedirectController() = default;
    /** PartiallyMigrated platform boundary: defaults control transport behavior, not UBAA semantic completion. */
    [[nodiscard]] virtual RedirectOptions defaults() const = 0;
};

} // namespace UBAANext
