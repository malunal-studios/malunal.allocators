/// @file   common.hpp
/// @brief  Provides common utilities for all of the allocators.
/// @author John Christman sorakatadzuma@gmail.com
/// @copyright 2024 Simular Technologies, LLC.
#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory_resource>


#if (defined(linux)     || \
     defined(__linux)   || \
     defined(__linux__) || \
     defined(__GNU__)   || \
     defined(__GLIBC__))
#  define SIMULAR_ALLOCATORS_PLATFORM_POSIX 1
#  define SIMULAR_ALLOCATORS_PLATFORM_WIN32 0
#elif defined(_WIN32)    || \
      defined(__WIN32__) || \
      defined(WIN32)
#  define SIMULAR_ALLOCATORS_PLATFORM_POSIX 0
#  define SIMULAR_ALLOCATORS_PLATFORM_WIN32 1
#endif /* Platform check */


namespace simular::allocators {
} // namespace simular::allocators
