#include "backtrace.h"

#define BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED

#include <boost/stacktrace.hpp>

namespace ceq {

void PrintBackTrace(std::ostream& os) {
  auto bt = boost::stacktrace::stacktrace();
  for (size_t ind = 0; ind < bt.size(); ++ind) {
    auto& frame = bt[ind];
    os << ind << "# in " << frame.name();
    if (!frame.source_file().empty()) {
      os << " " << frame.source_file();
    }
    if (frame.source_line()) {
      os << ":" << frame.source_line();
    }
    os << " at " << frame.address() << '\n';
  }
  os << std::flush;
}

}  // namespace ceq
