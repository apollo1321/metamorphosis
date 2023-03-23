#pragma once

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "backtrace.h"

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
      std::cerr << "VERIFY failed: " << (MESSAGE) << std::endl; \
      PrintBackTrace(std::cerr);                                \
      std::abort();                                             \
    }                                                           \
  } while (false)
