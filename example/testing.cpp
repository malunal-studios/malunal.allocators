#include <iostream>
#include <vector>
#include <simular/allocators.hpp>

using namespace simular::allocators;

static arena_memory_resource*
arena_allocator() noexcept {
    static arena_memory_resource k_mem;
    return &k_mem;
}

template<typename T>
static std::pmr::vector<T>
make_vector() noexcept {
    return std::pmr::vector<T>(arena_allocator());
}

int
main() {
    auto vec = make_vector<int>();
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.push_back(4);

    for (const auto& num : vec)
        std::cout << num << ",";
}