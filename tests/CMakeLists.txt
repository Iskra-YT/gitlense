add_executable(gitlense_tests
    main.cpp
    test_format.cpp
    test_table.cpp
    test_language.cpp
    test_args.cpp
    test_oid.cpp
)

target_link_libraries(gitlense_tests PRIVATE gitlense_core)
target_include_directories(gitlense_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gitlense_tests PRIVATE -Wall -Wextra)
endif()

add_test(NAME unit COMMAND gitlense_tests)
