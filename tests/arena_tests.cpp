#include <gtest/gtest.h>
#include <simular/allocators.hpp>

using namespace simular::allocators;


struct test_arena_memory_resource : arena_memory_resource {
    // Promote these to public members.
    using arena_memory_resource::first_region;
    using arena_memory_resource::free_list;

    explicit
    test_arena_memory_resource(std::size_t capacity = k_default_capacity)
        : arena_memory_resource(capacity)
    { }
};


TEST(ArenaMemoryTests, can_initialize_memory) {
    try {
        test_arena_memory_resource _;
        ASSERT_EQ(0x0040'0000, _.total_size());
        ASSERT_EQ(8, _.total_used());    // Stores a pointer to next inside region.
        ASSERT_EQ(1, _.total_regions()); // Should have only allocated 1 region with default settings.
        ASSERT_EQ(0, _.allocations());   // Nothing was allocated into it.
    } catch (const std::bad_alloc&) {
        FAIL() << "Memory could not be allocated!";
    }

    try {
        test_arena_memory_resource a(8);
        ASSERT_EQ(0x0080'0000, a.total_size());
        ASSERT_EQ(16, a.total_used());   // Stores a pointer to next inside region.
        ASSERT_EQ(2, a.total_regions()); // Should have allocated 2 regions with default settings.
        ASSERT_EQ(0, a.allocations());   // Nothing was allocated into it.
    } catch (const std::bad_alloc&) {
        FAIL() << "Memory could not be allocated!";
    }
}

TEST(ArenaMemoryTests, can_allocate_memory) {
    test_arena_memory_resource mem;
    auto res = mem.allocate(sizeof(int), alignof(int));
    ASSERT_NE(nullptr, res);
    ASSERT_EQ(1, mem.allocations());
    ASSERT_EQ(12, mem.total_used()); // Next pointer plus allocated object.

    // Assure pointer addresses are correct.
    const auto ptr = reinterpret_cast<std::uintptr_t>(res);
    const auto reg = reinterpret_cast<std::uintptr_t>(mem.first_region());
    const auto frd = reinterpret_cast<std::uintptr_t>(mem.free_list());
    ASSERT_EQ(0, ptr % alignof(int));
    ASSERT_EQ(8, ptr - reg);
    ASSERT_EQ(4, frd - ptr);
}

TEST(ArenaMemoryTests, can_deallocate_memory) {
    test_arena_memory_resource mem;
    auto res = mem.allocate(sizeof(int), alignof(int));
    ASSERT_NE(nullptr, res);

    mem.deallocate(res, sizeof(int), alignof(int));
    ASSERT_EQ(0, mem.allocations());
    ASSERT_EQ(8, mem.total_used());

    // Assure pointer addresses are correct.
    const auto reg = reinterpret_cast<std::uintptr_t>(mem.first_region());
    const auto frd = reinterpret_cast<std::uintptr_t>(mem.free_list());
    ASSERT_EQ(8, frd - reg);
}
