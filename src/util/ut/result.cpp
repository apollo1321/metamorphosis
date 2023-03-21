#include <gtest/gtest.h>

#include <system_error>

#include <util/result.h>

struct SimpleErr {
  int val{};

  explicit SimpleErr(int v) : val{v} {
  }
};

TEST(Result, SimpleOk) {
  auto result = Result<int, SimpleErr>::Ok(1);
  EXPECT_TRUE(result.HasValue());
  EXPECT_FALSE(result.HasError());
  EXPECT_TRUE(result.IsOk());
  EXPECT_NO_THROW(result.ValueOrThrow());  // NOLINT
  EXPECT_NO_THROW(result.ThrowIfError());  // NOLINT
  EXPECT_EQ(result.ExpectValue(), 1);
}

TEST(Result, SimpleErr) {
  auto result = Result<int, SimpleErr>::Err(1);
  EXPECT_TRUE(result.HasError());
  EXPECT_FALSE(result.HasValue());
  EXPECT_FALSE(result.IsOk());
  EXPECT_THROW(result.ValueOrThrow(), std::exception);  // NOLINT
  EXPECT_THROW(result.ThrowIfError(), std::exception);  // NOLINT
  EXPECT_EQ(result.ExpectError().val, 1);
}

TEST(Result, OnlyMovable) {
  struct Val {
    explicit Val(int v) : val{v} {
    }

    Val(Val&& rhs) = default;
    Val& operator=(Val&) = delete;

    int val{};
  };

  auto result1 = Result<Val, SimpleErr>::Ok(1);
  auto result2 = std::move(result1);
  EXPECT_EQ(result2.ExpectValue().val, 1);
}

TEST(Result, CopyableAndAssignable) {
  struct Val {
    explicit Val(int v) : val{v} {
    }

    Val(const Val& rhs) = default;
    Val& operator=(Val&) = delete;

    int val{};
  };

  auto result1 = Result<Val, SimpleErr>::Ok(1);
  auto result2 = result1;
  auto result3 = result1;
  EXPECT_EQ(result2.ExpectValue().val, 1);
  EXPECT_EQ(result3.ExpectValue().val, 1);
  EXPECT_NO_THROW(result3.ThrowIfError());  // NOLINT
}

TEST(Result, SameType) {
  using Result = Result<int, int>;
  EXPECT_EQ(Result::Ok(1).ExpectValue(), 1);
  EXPECT_EQ(Result::Err(1).ExpectError(), 1);
}

TEST(Result, ThrowsStdError) {
  auto result = Status<std::error_code>::Err(std::make_error_code(std::errc::host_unreachable));
  try {
    result.ThrowIfError();
  } catch (std::system_error& e) {
    EXPECT_STREQ(e.what(), "No route to host");
  } catch (...) {
    FAIL();
  }
}

TEST(Result, RethowsError) {
  std::exception_ptr exc;
  try {
    throw std::runtime_error("test exception");
  } catch (std::exception&) {
    exc = std::current_exception();
  }
  auto result = Status<std::exception_ptr>::Err(exc);
  try {
    result.ThrowIfError();
  } catch (std::runtime_error& e) {
    EXPECT_STREQ(e.what(), "test exception");
  } catch (...) {
    FAIL();
  }
}
