set(TestSources
    TestContainer.cpp
    TestNetwork.cpp
    TestTransport.cpp
    TestSched.cpp
    TestWeb.cpp
    TestPublicApi.cpp
    TestStress.cpp
    TestApplication.cpp
    TestTls.cpp
)

foreach(TestSource ${TestSources})
    get_filename_component(TestName ${TestSource} NAME_WE)

    add_executable(${TestName} ${TestSource})
    target_link_libraries(${TestName} PRIVATE
        CustomLibraries::CustomLibraries
    )

    # 避免可执行文件的 .dll.a 导出文件冲突
    set_target_properties(${TestName} PROPERTIES
        ENABLE_EXPORTS OFF
        WIN32_EXECUTABLE FALSE
    )

    add_test(NAME ${TestName} COMMAND ${TestName})
endforeach()
