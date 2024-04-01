#include "chrono.h"

#include <iomanip>
#include <sstream>
#include <string>

namespace mtf::rt {

std::string ToString(const Duration& duration) noexcept {
  auto us = duration.count() % 1000;
  auto ms = duration.count() / 1000 % 1000;
  auto sec = duration.count() / 1000 / 1000 % 60;
  auto min = duration.count() / 1000 / 1000 / 60 % 60;
  auto hours = duration.count() / 1000 / 1000 / 60 / 60;

  std::ostringstream os;

  os << '[' << std::setfill('0')      //
     << std::setw(2) << hours << ':'  //
     << std::setw(2) << min << ':'    //
     << std::setw(2) << sec << '.'    //
     << std::setw(3) << ms << '.'     //
     << std::setw(3) << us            //
     << ']';

  return os.str();
}

}  // namespace mtf::rt
