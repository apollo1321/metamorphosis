#pragma once

#include <concepts>
#include <cstdint>
#include <span>
#include <vector>

namespace mtf::rt::serde {

using Data = std::vector<uint8_t>;
using DataView = std::span<const uint8_t>;

template <class T>
concept CDataOrView = std::same_as<Data, T> || std::same_as<DataView, T>;

template <class T>
concept CSerde =  //
    requires(T serde, DataView data) {
      { serde.Deserialize(data) } noexcept;
      { serde.Serialize(serde.Deserialize(data)) } noexcept -> CDataOrView;
    };

}  // namespace mtf::rt::serde
