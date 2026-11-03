#pragma once

#include <QString>
#include <QStringList>

#include <utility>

namespace beygram {

/// Machine-readable error classes shared by core, platform and Telegram code.
/// These are deliberately coarse: the UI decides how to phrase a failure.
enum class ErrorCode {
    None = 0,
    InvalidArgument,
    NotFound,
    Unavailable,
    Io,
    Authentication,
    Tdlib,
    Internal,
};

struct Error {
    ErrorCode code = ErrorCode::None;
    QString message;

    [[nodiscard]] bool isError() const noexcept { return code != ErrorCode::None; }
    [[nodiscard]] QString codeName() const;
};

/// A small non-throwing result type used across module boundaries.
/// C++20 does not ship std::expected, and a dependency for this would be noise.
template <typename T>
class Result {
public:
    Result(T value) : m_value(std::move(value)), m_ok(true) {}
    Result(Error error) : m_error(std::move(error)), m_ok(false) {}

    [[nodiscard]] bool hasValue() const noexcept { return m_ok; }
    explicit operator bool() const noexcept { return m_ok; }

    [[nodiscard]] const T& value() const& { return m_value; }
    [[nodiscard]] T& value() & { return m_value; }
    [[nodiscard]] T take() { return std::move(m_value); }
    [[nodiscard]] const Error& error() const& { return m_error; }

    static Result ok(T value) { return Result(std::move(value)); }
    static Result fail(ErrorCode code, QString message) {
        return Result(Error{code, std::move(message)});
    }

private:
    T m_value{};
    Error m_error{};
    bool m_ok = false;
};

/// Result for operations that only report success or failure.
class Status {
public:
    Status() = default;
    explicit Status(Error error) : m_error(std::move(error)) {}

    [[nodiscard]] bool ok() const noexcept { return !m_error.isError(); }
    explicit operator bool() const noexcept { return ok(); }
    [[nodiscard]] const Error& error() const noexcept { return m_error; }

    static Status success() { return Status(); }
    static Status failure(ErrorCode code, QString message) {
        return Status(Error{code, std::move(message)});
    }

private:
    Error m_error{};
};

inline QString Error::codeName() const
{
    switch (code) {
    case ErrorCode::None: return QStringLiteral("none");
    case ErrorCode::InvalidArgument: return QStringLiteral("invalid_argument");
    case ErrorCode::NotFound: return QStringLiteral("not_found");
    case ErrorCode::Unavailable: return QStringLiteral("unavailable");
    case ErrorCode::Io: return QStringLiteral("io");
    case ErrorCode::Authentication: return QStringLiteral("authentication");
    case ErrorCode::Tdlib: return QStringLiteral("tdlib");
    case ErrorCode::Internal: return QStringLiteral("internal");
    }
    return QStringLiteral("unknown");
}

} // namespace beygram
