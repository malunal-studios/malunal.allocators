#include <benchmark/benchmark.h>
#include <simular/allocators.hpp>

static void
BM_StandardNewDeleteAllocatorCreate(benchmark::State& state) {
    for (auto _ : state)
        std::vector<int> myvec({ 1, 2, 3 });
}

static void
BM_StandardUnsynchronizedPoolCreate(benchmark::State& state) {
    using std::pmr::unsynchronized_pool_resource;
    unsynchronized_pool_resource pool;

    for (auto _ : state)
        std::pmr::vector<int> myvec({ 1, 2, 3 }, &pool);
}

static void
BM_SimularAllocatorsArenaMemoryCreate(benchmark::State& state) {
    using simular::allocators::arena_memory_resource;
    arena_memory_resource arena;

    for (auto _ : state)
        std::pmr::vector<int> myvec({ 1, 2, 3 }, &arena);
}

BENCHMARK(BM_StandardNewDeleteAllocatorCreate)->Iterations(1000000)->Threads(1);
BENCHMARK(BM_StandardUnsynchronizedPoolCreate)->Iterations(1000000)->Threads(1);
BENCHMARK(BM_SimularAllocatorsArenaMemoryCreate)->Iterations(1000000)->Threads(1);
