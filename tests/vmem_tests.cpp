#include <gtest/gtest.h>
#include <simular/allocators.hpp>

TEST(VirtualMemoryTests, can_initialize_memory) {
    try {
        simular::allocators::vmem mem;
    } catch (const std::bad_alloc&) {
        ASSERT_FALSE(false) <<
            "Memory could not be allocated!";
    }
}

TEST(VirtualMemoryTests, can_allocate_memory) {
    simular::allocators::vmem mem;
    auto res = mem.allocate(sizeof(int), alignof(int));
    ASSERT_TRUE(res != nullptr);

    auto ptr = reinterpret_cast<std::uintptr_t>(res);
    ASSERT_EQ(0, ptr % alignof(int));
}