#pragma once
#include <variant>
#include <string>
#include <stdexcept>
#include <iostream>

namespace common {

    struct Ok {};

    enum class ErrorCode {
        Success = 0,
        Cancelled,       // Clean stop (Expected)
        DeviceNotFound,  // Hardware issue
        PermissionDenied,
        EncoderError,
        Busy,
        Timeout,
        ExternalToolMissing, // Missing dependency (e.g. grim, scrot)
        CriticalError,    // Requires restart
        NotImplemented,
        Unknown
    };

    struct AppError {
        ErrorCode code;
        std::string message;
        std::string location; // __FILE__:__LINE__
    };

    template <typename T = Ok>
    class Result {
        std::variant<T, AppError> value;

    public:
        // Constructors
        Result(T v) : value(std::move(v)) {}
        Result(AppError e) : value(std::move(e)) {}

        // Static Builders
        static Result<T> ok(T v) { return Result(std::move(v)); }

        static Result<T> err(ErrorCode code, const std::string& msg, const std::string& loc = "") {
            return Result(AppError{code, msg, loc});
        }

        // Checkers
        bool is_ok() const { return std::holds_alternative<T>(value); }
        bool is_err() const { return std::holds_alternative<AppError>(value); }

        // Unwrappers
        const T& unwrap() const {
            if (is_err()) {
                const auto& e = std::get<AppError>(value);
                throw std::runtime_error("Result::unwrap failed: " + e.message);
            }
            return std::get<T>(value);
        }

        const AppError& error() const {
            if (is_ok()) {
                throw std::logic_error("Result::error called on success value");
            }
            return std::get<AppError>(value);
        }

        // For void-like results (Result<Ok>)
        static Result<Ok> success() { return Result<Ok>(Ok{}); }
    };

    using EmptyResult = Result<Ok>;

} // namespace common
