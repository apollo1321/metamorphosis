#pragma once

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "backtrace.h"

#define ENSURE(CONDIDITION, MESSAGE)     \
  do {                                   \
    if (!(CONDIDITION)) {                \
      throw std::runtime_error(MESSAGE); \
    }                                    \
  } while (false)

#define VERIFY(CONDIDITION, MESSAGE)                                                             \
  do {                                                                                           \
    if (!(CONDIDITION)) {                                                                        \
      std::cerr << __FILE__ << ":" << __LINE__ << ": VERIFY failed: " << (MESSAGE) << std::endl; \
      ::mtf::PrintBackTrace(std::cerr);                                                            \
      std::abort();                                                                              \
    }                                                                                            \
  } while (false)
