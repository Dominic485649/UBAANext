#pragma once

#include <ctime>

namespace UBAANext {

std::tm utc_time(std::time_t value);
std::tm local_time(std::time_t value);
std::time_t utc_time_t(std::tm value);

} // namespace UBAANext
