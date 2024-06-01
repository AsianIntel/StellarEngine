module;

#include <utility>
#include <variant>
#include <type_traits>

export module stellar.core.result;

export template<typename T>
struct Ok {
	T value;

    constexpr explicit Ok(const T& val) noexcept : value(val) {}
    constexpr explicit Ok(T&& val) noexcept : value(std::forward<T>(val)) {}
};

export template<>
struct Ok<void> {
    constexpr explicit Ok() noexcept = default;
};

Ok() -> Ok<void>;

export template<typename E>
struct Err {
    E value;

    constexpr explicit Err(const E& err) noexcept : value(err) {}
    constexpr explicit Err(E&& err) noexcept : value(std::forward<E>(err)) {}
};

export template<typename T, typename E>
struct Result {
    using OkT = Ok<T>;
    using ErrT = Err<E>;

    std::variant<OkT, ErrT> value;

    Result() = delete;
    Result(const Result&) = default;
    Result& operator=(const Result&) = default;
    Result(Result&&) = default;
    Result& operator=(Result&&) = default;
    ~Result() = default;

    Result(const OkT& ok): value(ok) {}
    Result(OkT&& ok): value(std::move(ok)) {}

    Result(const ErrT& err): value(err) {}
    Result(ErrT&& err): value(std::move(err)) {}

    [[nodiscard]] constexpr bool is_ok() const noexcept { return std::holds_alternative<OkT>(value); }
    [[nodiscard]] constexpr bool is_err() const noexcept { return std::holds_alternative<ErrT>(value); }

    constexpr T unwrap() requires !std::is_void_v<T> {
        if (is_err()) throw std::bad_variant_access();
        return std::get<OkT>(std::move(value)).value;
    }

    constexpr void unwrap() requires std::is_void_v<T> {
        if (is_err()) throw std::bad_variant_access();
    }

    constexpr T unwrap() const requires !std::is_void_v<T> {
        if (is_err()) throw std::bad_variant_access();
        return std::get<OkT>(value).value;
    }

    constexpr void unwrap() const requires std::is_void_v<T> {
        if (is_err()) throw std::bad_variant_access();
    }

    constexpr E unwrap_err() {
        if (is_ok()) throw std::bad_variant_access();
        return std::get<ErrT>(value).value;
    }

    constexpr E unwrap_err() const {
        if (is_ok()) throw std::bad_variant_access();
        return std::get<ErrT>(value).value;
    }

    constexpr T unwrap_or_default() requires std::is_default_constructible_v<T> {
        if (is_ok()) {
            return std::get<OkT>(std::move(value)).value;
        }
        return T();
    }
};