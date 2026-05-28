#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <UBAANext/Base/TimeUtils.hpp>

#include <mutex>

namespace UBAANext {
namespace {

std::mutex &ctime_mutex() {
    static std::mutex mutex;
    return mutex;
}

long long days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const auto year_of_era = static_cast<unsigned>(year - era * 400);
    const auto day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const auto day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return era * 146097LL + static_cast<long long>(day_of_era) - 719468;
}

} // namespace

std::tm utc_time(std::time_t value) {
    std::lock_guard<std::mutex> lock(ctime_mutex());
    auto *tm = std::gmtime(&value);
    return tm ? *tm : std::tm{};
}

std::tm local_time(std::time_t value) {
    std::lock_guard<std::mutex> lock(ctime_mutex());
    auto *tm = std::localtime(&value);
    return tm ? *tm : std::tm{};
}

std::time_t utc_time_t(std::tm value) {
    const int year = value.tm_year + 1900;
    const auto month = static_cast<unsigned>(value.tm_mon + 1);
    const auto day = static_cast<unsigned>(value.tm_mday);
    auto seconds = days_from_civil(year, month, day) * 24 * 60 * 60;
    seconds += value.tm_hour * 60 * 60 + value.tm_min * 60 + value.tm_sec;
    return static_cast<std::time_t>(seconds);
}

} // namespace UBAANext
