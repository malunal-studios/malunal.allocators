#define MALUNAL_ALLOCATORS_REGION_MAXIMUM_ALLOCATION 0x00FF'FFF8
#define MALUNAL_ALLOCATORS_ARENA_DEFAULT_CAPACITY    16
#include <benchmark/benchmark.h>
#include <malunal/allocators.hpp>

static void
BM_StandardNewDeleteAllocatorInsert(benchmark::State& state) {
    std::vector<int> myvec;

    auto index = 0;
    for (auto _ : state)
        myvec.push_back(index++);
}

static void
BM_StandardUnsynchronizedPoolInsert(benchmark::State& state) {
    using std::pmr::unsynchronized_pool_resource;
    unsynchronized_pool_resource pool;
    std::pmr::vector<int> myvec(&pool);

    auto index = 0;
    for (auto _ : state)
        myvec.push_back(index++);
}

static void
BM_MalunalAllocatorsArenaMemoryInsert(benchmark::State& state) {
    using namespace malunal::allocators;
    arena_memory_resource arena;
    std::pmr::vector<int> myvec(&arena);

    auto index = 0;
    for (auto _ : state)
        myvec.push_back(index++);
}

BENCHMARK(BM_StandardNewDeleteAllocatorInsert)->Iterations(1000000)->Threads(1);
BENCHMARK(BM_StandardUnsynchronizedPoolInsert)->Iterations(1000000)->Threads(1);
BENCHMARK(BM_MalunalAllocatorsArenaMemoryInsert)->Iterations(1000000)->Threads(1);
