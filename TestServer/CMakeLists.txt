add_executable(TestServer
    source/Main.cpp
    source/TestServer.cpp
)

target_link_libraries(TestServer PRIVATE IgniteNet)
