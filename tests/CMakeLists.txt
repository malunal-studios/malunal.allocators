macro(create_test TESTNAME TESTFILE)
    add_executable(${TESTNAME} ${TESTFILE})
    target_link_libraries(
        ${TESTNAME}
        PRIVATE GTest::gtest_main
        PRIVATE malunal::allocators
    )

    gtest_discover_tests(${TESTNAME})
endmacro()

# Create tests here.
create_test(mem.arena.test arena.cpp)
