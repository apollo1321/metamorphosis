#pragma once

#include <exception>
#include <system_error>
#include <tuple>
#include <variant>

#include "condition_check.h"

namespace ceq {

namespace impl {

struct OkTag {};
struct ErrTag {};

}  // namespace impl

template <class... Args>
auto Ok(Args&&... args) {
  return std::tuple<impl::OkTag, Args&&...>(impl::OkTag{}, std::forward<Args>(args)...);
}

template <class... Args>
auto Err(Args&&... args) {
  return std::tuple<impl::ErrTag, Args&&...>(impl::ErrTag{}, std::forward<Args>(args)...);
}

template <class T, class E>
class [[nodiscard]] Result {
 public:
  using Value = T;
  using Error = E;

  static_assert(!std::is_reference_v<Value> && !std::is_reference_v<Error>,
                "Reference types are not supported");

  static_assert(!std::is_void<Error>::value, "void error type is now allowed");

 public:
  template <class... Args>
  Result(std::tuple<impl::OkTag, Args...>&& args)  // NOLINT
      : Result(std::in_place_index<0>, std::move(args),
               std::make_index_sequence<sizeof...(Args) + 1>{}) {
  }

  template <class... Args>
  Result(std::tuple<impl::ErrTag, Args...>&& args)  // NOLINT
      : Result(std::in_place_index<1>, std::move(args),
               std::make_index_sequence<sizeof...(Args) + 1>{}) {
  }

  // Moving

  Result(Result&& other) = default;
  Result& operator=(Result&& other) = default;

  // Copying

  Result(Result& other) = default;
  Result& operator=(Result& other) = default;

  // Testing

  // Panics on error, ignores result
  void ExpectOk() const noexcept {
    constexpr bool kHasErrorMessage =  //
        requires(const Error& error) {
          { error.Message() } -> std::convertible_to<std::string>;
        };

    if constexpr (kHasErrorMessage) {
      VERIFY(HasValue(), "Result does not hold value: " + std::get<1>(data_).Message());
    } else {
      VERIFY(HasValue(), "Result does not hold value");
    }
  }

  // Panics on value, ignores result
  void ExpectFail() const noexcept {
    VERIFY(HasError(), "Result does not hold error");
  }

  [[nodiscard]] bool HasValue() const noexcept {
    return data_.index() == 0;
  }

  [[nodiscard]] bool HasError() const noexcept {
    return !HasValue();
  }

  // Value accessors

  Value& ValueOrThrow() & {
    ThrowIfError();
    return std::get<0>(data_);
  }

  const Value& ValueOrThrow() const& {
    ThrowIfError();
    return std::get<0>(data_);
  }

  Value&& ValueOrThrow() && {
    ThrowIfError();
    return std::move(std::get<0>(data_));
  }

  Value& GetValue() & noexcept {
    ExpectOk();
    return std::get<0>(data_);
  }

  const Value& GetValue() const& noexcept {
    ExpectOk();
    return std::get<0>(data_);
  }

  Value&& GetValue() && noexcept {
    ExpectOk();
    return std::move(std::get<0>(data_));
  }

  // Error accessors

  void ThrowIfError() const {
    if (HasValue()) {
      return;
    }
    if constexpr (std::is_convertible_v<Error, std::string>) {
      throw std::runtime_error{std::string(std::get<1>(data_))};
    } else if constexpr (std::is_base_of_v<Error, std::exception>) {
      throw std::get<1>(data_);
    } else if constexpr (std::is_base_of_v<Error, std::error_code>) {
      throw std::system_error(std::get<1>(data_));
    } else if constexpr (std::is_same_v<Error, std::exception_ptr>) {
      std::rethrow_exception(std::get<1>(data_));
    } else {
      throw std::runtime_error("Result does not contain value");
    }
  }

  Error& GetError() & noexcept {
    ExpectFail();
    return std::get<1>(data_);
  }

  const Error& GetError() const& noexcept {
    ExpectFail();
    return std::get<1>(data_);
  }

  Error&& GetError() && noexcept {
    ExpectFail();
    return std::move(std::get<1>(data_));
  }

  // Ignoring result

  void Ignore() const noexcept {
    // Do nothing
  }

 protected:
  template <class Index, class... Args, size_t Ind1, size_t... Is>
  explicit Result(Index index, std::tuple<Args...>&& args, std::index_sequence<Ind1, Is...>)
      : data_(index, std::get<Is>(std::move(args))...) {
  }

 private:
  std::variant<Value, Error> data_;
};

template <class E>
class [[nodiscard]] Result<void, E> : public Result<std::monostate, E> {
 public:
  Result(std::tuple<impl::OkTag>&& args)  // NOLINT
      : Base::Result(std::in_place_index<0>, std::move(args), std::make_index_sequence<1>{}) {
  }

  template <class... Args>
  Result(std::tuple<impl::ErrTag, Args...>&& args)  // NOLINT
      : Base::Result(std::in_place_index<1>, std::move(args),
                     std::make_index_sequence<sizeof...(Args) + 1>{}) {
  }

 private:
  using Base = Result<std::monostate, E>;

  using Base::GetValue;
  using Base::HasValue;
  using Base::ValueOrThrow;
};

template <class E>
using Status = Result<void, E>;

}  // namespace ceq
