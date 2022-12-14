#pragma once

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#define ENSURE_STATUS(STATUS)                       \
  do {                                              \
    auto _status = (STATUS);                        \
    if (!_status.ok()) {                            \
      throw std::runtime_error(_status.ToString()); \
    }                                               \
  } while (false)

#define ENSURE(CONDIDITION, MESSAGE)     \
  do {                                   \
    if (!(CONDIDITION)) {                \
      throw std::runtime_error(MESSAGE); \
    }                                    \
  } while (false)

#define VERIFY(CONDIDITION, MESSAGE)                            \
  do {                                                          \
    if (!(CONDIDITION)) {                                       \
      std::cerr << "verify failed: " << (MESSAGE) << std::endl; \
      std::abort();                                             \
    }                                                           \
  } while (false)
