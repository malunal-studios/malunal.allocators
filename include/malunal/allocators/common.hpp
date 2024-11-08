/// @file   common.hpp
/// @brief  Provides common utilities for all of the allocators.
/// @author John Christman sorakatadzuma@gmail.com
/// @copyright 2024 Malunal Studios, LLC.
#pragma once
#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <memory_resource>
#include <type_traits>


#if (defined(linux)     || \
     defined(__linux)   || \
     defined(__linux__) || \
     defined(__GNU__)   || \
     defined(__GLIBC__))
#  define MALUNAL_ALLOCATORS_PLATFORM_POSIX 1
#  define MALUNAL_ALLOCATORS_PLATFORM_WIN32 0
#elif defined(_WIN32)    || \
      defined(__WIN32__) || \
      defined(WIN32)
#  define MALUNAL_ALLOCATORS_PLATFORM_POSIX 0
#  define MALUNAL_ALLOCATORS_PLATFORM_WIN32 1
#endif /* Platform check */


namespace malunal::allocators::detail {

/// @brief   Calculates the forward adjustment for a given pointer and the given
///          alignment value.
/// @details The forward adjustment defines how much to add to a given pointer
///          to align that address on a byte boundery of alignment. Any adjusted
///          address should produce zero when taking the modulus of that address
///          and its proposed alignment.
/// @param   ptr The pointer to forward adjust.
/// @param   aligment The alignment to adjust to.
/// @returns The amount needed to adjust the address to the byte boundary of
///          alignment.
inline std::size_t
calc_fwd_adjust(uintptr_t ptr, size_t alignment) noexcept {
    const auto iptr    = reinterpret_cast<uintptr_t>(ptr);
    const auto alignm1 = alignment - 1u;
    const auto aligned = (iptr + alignm1) & ~alignm1;
    return aligned - iptr;
}

} // namespace malunal::allocators::detail
