/**
 * @file Result.hpp
 * @brief UBAA Next Result 类型与错误工厂
 */
#pragma once

#include <UBAANext/Base/Error.hpp>

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace UBAANext {

struct Unexpected {
    Error error;
};

class ResultError : public std::runtime_error {
public:
    explicit ResultError(Error error)
        : std::runtime_error(error.message), m_error(std::move(error)) {}

    [[nodiscard]] const Error &error() const noexcept { return m_error; }

private:
    Error m_error;
};

[[nodiscard]] inline Unexpected make_error(ErrorCode code, std::string message) {
    return Unexpected{Error(code, std::move(message))};
}

template <typename T>
class Result {
public:
    Result(const T &value) : m_storage(value) {}
    Result(T &&value) : m_storage(std::move(value)) {}
    Result(Unexpected unexpected) : m_storage(std::move(unexpected.error)) {}

    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(m_storage);
    }

    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] T &operator*() { return std::get<T>(m_storage); }
    [[nodiscard]] const T &operator*() const { return std::get<T>(m_storage); }
    [[nodiscard]] T *operator->() { return &std::get<T>(m_storage); }
    [[nodiscard]] const T *operator->() const { return &std::get<T>(m_storage); }

    [[nodiscard]] T &value() {
        if (!has_value()) throw ResultError(std::get<Error>(m_storage));
        return std::get<T>(m_storage);
    }
    [[nodiscard]] const T &value() const {
        if (!has_value()) throw ResultError(std::get<Error>(m_storage));
        return std::get<T>(m_storage);
    }

    [[nodiscard]] Error &error() { return std::get<Error>(m_storage); }
    [[nodiscard]] const Error &error() const { return std::get<Error>(m_storage); }

private:
    std::variant<T, Error> m_storage;
};

template <>
class Result<void> {
public:
    Result() = default;
    Result(Unexpected unexpected) : m_error(std::move(unexpected.error)), m_has_value(false) {}

    [[nodiscard]] bool has_value() const noexcept { return m_has_value; }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    void value() const {
        if (!m_has_value) {
            throw ResultError(m_error);
        }
    }

    [[nodiscard]] Error &error() { return m_error; }
    [[nodiscard]] const Error &error() const { return m_error; }

private:
    Error m_error;
    bool m_has_value = true;
};

} // namespace UBAANext
