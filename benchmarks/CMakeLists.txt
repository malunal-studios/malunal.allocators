macro(create_bench TESTNAME TESTFILE)
    add_executable(${TESTNAME} ${TESTFILE})
    target_link_libraries(
        ${TESTNAME}
        PRIVATE benchmark::benchmark
        PRIVATE benchmark::benchmark_main

        # Currently header only, may not be forever
        PRIVATE simular::allocators
    )
endmacro()

create_bench(arena.create.bench arena_create.cpp)
create_bench(arena.insert.bench arena_insert.cpp)
