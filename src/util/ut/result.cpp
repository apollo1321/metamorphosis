#include <gtest/gtest.h>

#include <system_error>

#include <util/result.h>

struct SimpleErr {
  int val{};

  explicit SimpleErr(int v) : val{v} {
  }
};

TEST(Result, SimpleOk) {
  ceq::Result<int, SimpleErr> result = ceq::Ok(1);
  EXPECT_TRUE(result.HasValue());
  EXPECT_FALSE(result.HasError());
  EXPECT_NO_THROW(result.ValueOrThrow());  // NOLINT
  EXPECT_NO_THROW(result.ThrowIfError());  // NOLINT
  EXPECT_EQ(result.GetValue(), 1);
}

TEST(Result, SimpleErr) {
  ceq::Result<int, SimpleErr> result = ceq::Err(1);
  EXPECT_TRUE(result.HasError());
  EXPECT_FALSE(result.HasValue());
  EXPECT_THROW(result.ValueOrThrow(), std::exception);  // NOLINT
  EXPECT_THROW(result.ThrowIfError(), std::exception);  // NOLINT
  EXPECT_EQ(result.GetError().val, 1);
}

TEST(Result, OnlyMovable) {
  struct Val {
    explicit Val(int v) : val{v} {
    }

    Val(Val&& rhs) = default;
    Val& operator=(Val&) = delete;

    int val{};
  };

  ceq::Result<Val, SimpleErr> result1 = ceq::Ok(1);
  auto result2 = std::move(result1);
  EXPECT_EQ(result2.GetValue().val, 1);
}

TEST(Result, CopyableAndAssignable) {
  struct Val {
    explicit Val(int v) : val{v} {
    }

    Val(const Val& rhs) = default;
    Val& operator=(Val&) = delete;

    int val{};
  };

  ceq::Result<Val, SimpleErr> result1 = ceq::Ok(1);
  auto result2 = result1;
  auto result3 = result1;
  EXPECT_EQ(result2.GetValue().val, 1);
  EXPECT_EQ(result3.GetValue().val, 1);
  EXPECT_NO_THROW(result3.ThrowIfError());  // NOLINT
}

TEST(Result, SameType) {
  EXPECT_EQ((ceq::Result<int, int>(ceq::Ok(1)).GetValue()), 1);
  EXPECT_EQ((ceq::Result<int, int>(ceq::Err(1)).GetError()), 1);
}

TEST(Result, ThrowsStdError) {
  ceq::Status<std::error_code> result = ceq::Err(std::make_error_code(std::errc::host_unreachable));

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
  ceq::Status<std::exception_ptr> result = ceq::Err(exc);
  try {
    result.ThrowIfError();
  } catch (std::runtime_error& e) {
    EXPECT_STREQ(e.what(), "test exception");
  } catch (...) {
    FAIL();
  }
}
