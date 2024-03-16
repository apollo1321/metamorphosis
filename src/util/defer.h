#include <utility>

namespace mtf::internal {

template <class Callback>
struct OnScopeExit {
  explicit OnScopeExit(Callback callback) noexcept : callback(std::move(callback)) {
  }

  ~OnScopeExit() {
    callback();
  }

  struct MacroHelper {
    template <class T>
    auto operator|(T callback) {
      return OnScopeExit<T>(std::move(callback));
    }
  };

  Callback callback;
};

}  // namespace mtf::internal

#define DEFER auto defer_##__LINE__ = mtf::internal::OnScopeExit<int>::MacroHelper{} | [&]()
