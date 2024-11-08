# Malunal.Allocators

A header-only collection of allocators which can be used to serve whatever allocation strategies you may need. For our own purposes, it serves as a manner to facilitate the large swaths of allocations and memory managment that we need in our `malunal.cherry` project.

### Requirements

As with most of our libraries, you'll need a compiler that supports C++20 or higher. You'll also need CMake `>=3.16`, because we intend to support precompiled headers and that is the first version of CMake that supports it.

### Usage

The usage is pretty simple, you will need to use the `std::pmr` containers for this to work. And a word of warning, the `std::pmr::polymorphic_allocator`, which is used in all of the `std::pmr` containers, does not propagate on copy or move construction/assignment. So you will want to make sure all of your containers that use the objects in this library are consistent and used everywhere with a pattern similar to the following example which is the same as [here](./example/testing.cpp):

```cpp
#include <iostream>
#include <vector>
#include <malunal/allocators.hpp>

using namespace malunal::allocators;

template<typename T>
static std::pmr::vector<T>
make_vector() noexcept {
    using malunal::allocators::arena_allocator_instance;
    return std::pmr::vector<T>(arena_allocator_instance());
}

int
main() {
    auto vec = make_vector<int>();
    for (auto index = 0; index < 512; index++)
        vec.push_back(index);
}
```
