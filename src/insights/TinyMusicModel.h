#pragma once
#include <array>
#include <string_view>
namespace tonal::muse {
inline constexpr int labelsCount=8;
std::array<float,labelsCount> analyze(std::string_view normalized);
const std::array<const char*,labelsCount>& labels();
}
