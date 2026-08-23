#pragma once

// =============================================================================
// devpiano Precompiled Header (PCH)
//
// Precompiles standard library and JUCE framework headers to drastically reduce
// redundant AST parsing across translation units in Debug and Release builds.
// =============================================================================

#ifdef __cplusplus
// IWYU pragma: begin_exports
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
// IWYU pragma: end_exports
#endif
