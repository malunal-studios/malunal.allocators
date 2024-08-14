#include <gtest/gtest.h>
#include <simular/allocators.hpp>

using namespace simular::allocators;


struct test_arena_memory_resource : arena_memory_resource {
    // Promote these to public members.
    using arena_memory_resource::first_region;
    using arena_memory_resource::free_list;
    using arena_memory_resource::freed;

    explicit
    test_arena_memory_resource(std::size_t capacity = k_default_capacity)
        : arena_memory_resource(capacity)
    { }
};


TEST(ArenaMemoryTests, can_initialize_memory) {
    try {
        test_arena_memory_resource _;
        ASSERT_EQ(0x0040'0000, _.total_size());
        ASSERT_EQ(520, _.total_used());  // Stores a pointer to next inside region, and a vector of free blocks.
        ASSERT_EQ(1, _.total_regions()); // Should have only allocated 1 region with default settings.
        ASSERT_EQ(1, _.allocations());   // Free list counts as an allocation.
    } catch (const std::bad_alloc&) {
        FAIL() << "Memory could not be allocated!";
    }

    try {
        test_arena_memory_resource a(8);
        ASSERT_EQ(0x0080'0000, a.total_size());
        ASSERT_EQ(528, a.total_used());  // Stores a pointer to next inside region, and a vector of free blocks.
        ASSERT_EQ(2, a.total_regions()); // Should have allocated 2 regions with default settings.
        ASSERT_EQ(1, a.allocations());   // Free list counts as an allocation.
    } catch (const std::bad_alloc&) {
        FAIL() << "Memory could not be allocated!";
    }
}

TEST(ArenaMemoryTests, can_allocate_memory) {
    test_arena_memory_resource mem;
    auto res = mem.allocate(sizeof(int), alignof(int));
    ASSERT_NE(nullptr, res);
    ASSERT_EQ(2, mem.allocations());  // Free list counts as an allocation.
    ASSERT_EQ(524, mem.total_used()); // Next pointer, free list, and integer.

    // Assure pointer addresses are correct.
    const auto ptr = reinterpret_cast<uintptr_t>(res);
    const auto reg = reinterpret_cast<uintptr_t>(mem.first_region());
    const auto frd = mem.free_list();
    ASSERT_EQ(0, ptr % alignof(int));
    ASSERT_EQ(520, ptr - reg); // Separated by the free list and next pointer.

    // Assert that free list contains correct information.
    const auto  space = 0x0040'0000 - 524;
    const auto  addr  = reg + 524;
    const auto& node  = frd[0];
    ASSERT_EQ(1, frd.size());
    ASSERT_EQ(space, node.size);
    ASSERT_EQ(addr,  node.addr);
}

TEST(ArenaMemoryTests, can_deallocate_memory) {
    test_arena_memory_resource mem;
    auto res = mem.allocate(sizeof(int), alignof(int));
    ASSERT_NE(nullptr, res);

    mem.deallocate(res, sizeof(int), alignof(int));
    ASSERT_EQ(1, mem.allocations());
    ASSERT_EQ(520, mem.total_used()); // Next pointer, free list.

    // Assert that free list contains correct information.
    const auto  reg   = reinterpret_cast<uintptr_t>(mem.first_region());
    const auto  frd   = mem.free_list();
    const auto  space = 0x0040'0000 - 520; // Next pointer and free list.
    const auto  addr  = reg + 520;         // Next pointer and free list.
    const auto& node  = frd[0];
    ASSERT_EQ(1, frd.size());
    ASSERT_EQ(space, node.size);
    ASSERT_EQ(addr,  node.addr);
}
