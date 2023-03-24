#pragma once

#include <exception>
#include <system_error>
#include <variant>

#include "condition_check.h"

namespace ceq {

// Simple wrapper over std::variant

template <class T, class E>
class [[nodiscard]] Result {
 public:
  using Value = T;
  using Error = E;

  static_assert(!std::is_reference_v<Value> && !std::is_reference_v<Error>,
                "Reference types are not supported");

  static_assert(!std::is_void<Error>::value, "void error type is now allowed");

 public:
  // Static constructors

  template <class... Args>
  static Result Ok(Args&&... arguments) {
    return Result(std::in_place_index<0>, std::forward<Args>(arguments)...);
  }

  template <class... Args>
  static Result Err(Args&&... arguments) {
    return Result(std::in_place_index<1>, std::forward<Args>(arguments)...);
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
    VERIFY(HasValue(), "Result does not hold value");
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

  [[nodiscard]] bool IsOk() const noexcept {
    return HasValue();
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

  Value& ExpectValue() & noexcept {
    ExpectOk();
    return std::get<0>(data_);
  }

  const Value& ExpectValue() const& noexcept {
    ExpectOk();
    return std::get<0>(data_);
  }

  Value&& ExpectValue() && noexcept {
    ExpectOk();
    return std::move(std::get<0>(data_));
  }

  // Error accessors

  void ThrowIfError() const {
    if (HasValue()) {
      return;
    }
    if constexpr (std::is_base_of_v<Error, std::exception>) {
      throw std::get<1>(data_);
    } else if constexpr (std::is_base_of_v<Error, std::error_code>) {
      throw std::system_error(std::get<1>(data_));
    } else if constexpr (std::is_same_v<Error, std::exception_ptr>) {
      std::rethrow_exception(std::get<1>(data_));
    } else {
      throw std::runtime_error("Result does not contain value");
    }
  }

  Error& ExpectError() & noexcept {
    ExpectFail();
    return std::get<1>(data_);
  }

  const Error& ExpectError() const& noexcept {
    ExpectFail();
    return std::get<1>(data_);
  }

  Error&& ExpectError() && noexcept {
    ExpectFail();
    return std::move(std::get<1>(data_));
  }

  // Ignoring result

  void Ignore() const noexcept {
    // Do nothing
  }

 protected:
  template <class Index, class... Args>
  explicit Result(Index index, Args&&... arguments)
      : data_(index, std::forward<Args>(arguments)...) {
  }

 private:
  std::variant<Value, Error> data_;
};

template <class E>
class [[nodiscard]] Result<void, E> : public Result<std::monostate, E> {
 public:
  // Static constructors

  static Result Ok() {
    return Result(std::in_place_index<0>);
  }

  template <class... TArgs>
  static Result Err(TArgs&&... arguments) {
    return Result(std::in_place_index<1>, std::forward<TArgs>(arguments)...);
  }

 private:
  using Base = Result<std::monostate, E>;

  using Base::Err;
  using Base::ExpectValue;
  using Base::HasValue;
  using Base::Ok;
  using Base::ValueOrThrow;

 private:
  template <class Index, class... Args>
  explicit Result(Index index, Args&&... arguments)
      : Base(index, std::forward<Args>(arguments)...) {
  }
};

template <class E>
using Status = Result<void, E>;

}  // namespace ceq
