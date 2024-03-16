#include <gtest/gtest.h>

#include <system_error>

#include <util/result.h>

struct SimpleErr {
  int val{};

  explicit SimpleErr(int v) : val{v} {
  }
};

TEST(Result, SimpleOk) {
  mtf::Result<int, SimpleErr> result = mtf::Ok(1);
  EXPECT_TRUE(result.HasValue());
  EXPECT_FALSE(result.HasError());
  EXPECT_NO_THROW(result.ValueOrThrow());  // NOLINT
  EXPECT_NO_THROW(result.ThrowIfError());  // NOLINT
  EXPECT_EQ(result.GetValue(), 1);
}

TEST(Result, SimpleErr) {
  mtf::Result<int, SimpleErr> result = mtf::Err(1);
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

  mtf::Result<Val, SimpleErr> result1 = mtf::Ok(1);
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

  mtf::Result<Val, SimpleErr> result1 = mtf::Ok(1);
  auto result2 = result1;
  auto result3 = result1;
  EXPECT_EQ(result2.GetValue().val, 1);
  EXPECT_EQ(result3.GetValue().val, 1);
  EXPECT_NO_THROW(result3.ThrowIfError());  // NOLINT
}

TEST(Result, SameType) {
  EXPECT_EQ((mtf::Result<int, int>(mtf::Ok(1)).GetValue()), 1);
  EXPECT_EQ((mtf::Result<int, int>(mtf::Err(1)).GetError()), 1);
}

TEST(Result, ThrowsStdError) {
  mtf::Status<std::error_code> result = mtf::Err(std::make_error_code(std::errc::host_unreachable));

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
  mtf::Status<std::exception_ptr> result = mtf::Err(exc);
  try {
    result.ThrowIfError();
  } catch (std::runtime_error& e) {
    EXPECT_STREQ(e.what(), "test exception");
  } catch (...) {
    FAIL();
  }
}

TEST(Result, AndThen) {
  struct Val1 {
    int val1{};
  };

  struct Val2 {
    int val2{};
  };

  {
    mtf::Result<Val1, std::string> res1 = mtf::Ok(Val1{1});

    auto res2 = std::move(res1).AndThen([](Val1&& v) -> mtf::Result<Val2, std::string> {
      return mtf::Ok(Val2{v.val1 + 1});
    });

    EXPECT_EQ(res2.GetValue().val2, 2);
  }

  {
    mtf::Result<Val1, std::string> res1 = mtf::Ok(Val1{0});

    auto res2 = std::move(res1).AndThen([](Val1&& v) -> mtf::Result<Val2, std::string> {
      return mtf::Err("abc");
    });

    EXPECT_EQ(res2.GetError(), "abc");
  }

  {
    mtf::Result<Val1, std::string> res1 = mtf::Err("abc");

    auto res2 = std::move(res1).AndThen([](Val1&& v) -> mtf::Result<Val2, std::string> {
      ADD_FAILURE();
      return mtf::Ok(Val2{1});
    });

    EXPECT_EQ(res2.GetError(), "abc");
  }
}

TEST(Result, OrElse) {
  struct Err1 {
    int val1{};
  };

  struct Err2 {
    int val2{};
  };

  mtf::Result<std::string, Err1> res1 = mtf::Err(Err1{1});

  auto res2 = std::move(res1)
                  .AndThen([](std::string&&) -> mtf::Result<std::string, Err1> {
                    ADD_FAILURE();
                    return mtf::Ok("abc");
                  })
                  .OrElse([](Err1&& error) -> mtf::Result<std::string, Err2> {
                    return mtf::Err(Err2{error.val1 + 1});
                  });

  EXPECT_EQ(res2.GetError().val2, 2);
}

TEST(Result, Transform) {
  struct Val1 {
    int val1{};
  };

  struct Val2 {
    int val2{};
  };

  mtf::Result<Val1, std::string> res1 = mtf::Ok(Val1{1});

  auto res2 = std::move(res1)
                  .Transform([](Val1&& val) -> Val2 {
                    return Val2{val.val1 + 1};
                  })
                  .Transform([](Val2&& val) -> Val2 {
                    return Val2{val.val2 + 1};
                  })
                  .TransformError([](std::string&&) -> int {
                    ADD_FAILURE();
                    return 1;
                  });

  EXPECT_EQ(res2.GetValue().val2, 3);
}

TEST(Result, TransformError) {
  struct Val1 {
    int val1{};
  };

  struct Val2 {
    int val2{};
  };

  mtf::Result<std::string, Val1> res1 = mtf::Err(Val1{1});

  auto res2 = std::move(res1)
                  .TransformError([](Val1&& val) -> Val2 {
                    return Val2{val.val1 + 1};
                  })
                  .TransformError([](Val2&& val) -> Val2 {
                    return Val2{val.val2 + 1};
                  })
                  .Transform([](std::string&&) -> int {
                    ADD_FAILURE();
                    return 1;
                  });

  EXPECT_EQ(res2.GetError().val2, 3);
}
