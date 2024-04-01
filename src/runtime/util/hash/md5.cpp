#include "md5.h"

#include <openssl/base64.h>
#include <openssl/md5.h>

#include <iomanip>
#include <sstream>

namespace mtf::rt {

MD5Hash ComputeMd5Hash(std::string_view data) noexcept {
  MD5Hash result;
  MD5(reinterpret_cast<const uint8_t*>(data.data()), data.size(), result.digest.data());
  return result;
}

std::string ToString(const MD5Hash& hash) noexcept {
  std::stringstream ss;
  for (int i = 0; i < hash.digest.size(); ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash.digest[i]);
  }
  return ss.str();
}

}  // namespace mtf::rt
