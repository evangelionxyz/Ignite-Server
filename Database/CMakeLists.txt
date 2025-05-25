add_executable(IgniteDatabase
    source/main.cpp
    source/Database.hpp
        ../TestServer/source/TestServer.cpp
        ../TestServer/source/TestServer.hpp
        ../TestServer/source/Main.cpp
)

target_include_directories(IgniteDatabase PRIVATE ${MySQLDirectory}/include/jdbc)

if (WIN32)
    target_compile_definitions(IgniteDatabase PRIVATE
        _CRT_SECURE_NO_WARNINGS
        STATIC_CONCPP
    )
    target_link_libraries(IgniteDatabase PUBLIC
        ${MySQLDirectory}/lib64/vs14/mysqlcppconn-static.lib
    )

    # Post-build: Copy all .dll files from MySQLConnector to the output directory
    add_custom_command(TARGET IgniteDatabase POST_BUILD
    
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MySQLDirectory}/lib64/libcrypto-3-x64.dll"
            "$<TARGET_FILE_DIR:IgniteDatabase>/libcrypto-3-x64.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MySQLDirectory}/lib64/libssl-3-x64.dll"
            "$<TARGET_FILE_DIR:IgniteDatabase>/libssl-3-x64.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MySQLDirectory}/lib64/mysqlcppconn-10-vs14.dll"
            "$<TARGET_FILE_DIR:IgniteDatabase>/mysqlcppconn-10-vs14.dll"
    )
endif()