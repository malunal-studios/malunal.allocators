macro(create_test TESTNAME TESTFILE)
    add_executable(${TESTNAME} ${TESTFILE})
    target_link_libraries(
        ${TESTNAME}
        PRIVATE GTest::gtest_main
        PRIVATE simular::allocators
    )

    gtest_discover_tests(${TESTNAME})
endmacro()

# Create tests here.
create_test(simular.allocators.virtual.test vmem_tests.cpp)
# create_test(simular.allocators.arena.test arena_tests.cpp)
