#include <iostream>
#include <vector>
#include <simular/allocators.hpp>

using namespace simular::allocators;

template<typename T>
static std::pmr::vector<T>
make_vector() noexcept {
    using simular::allocators::arena_allocator_instance;
    return std::pmr::vector<T>(arena_allocator_instance());
}

int
main() {
    auto vec = make_vector<int>();
    for (auto index = 0; index < 512; index++)
        vec.push_back(index);
}