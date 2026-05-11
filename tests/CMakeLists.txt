# 测试源文件列表（按模块组织）
set(TEST_SOURCES
    test_container.cpp
    test_sched.cpp
    test_network.cpp
)

foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)

    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE
        CustomLibraries::CustomLibraries
    )

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
