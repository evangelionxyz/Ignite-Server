add_executable(Database
    source/main.cpp
    source/Database.hpp
)

target_include_directories(Database PRIVATE ${MySQLDirectory}/include/jdbc)

IF (WIN32)
    target_compile_definitions(Database PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        STATIC_CONCPP
    )
    target_link_libraries(Database PUBLIC 
        ${MySQLDirectory}/lib64/vs14/mysqlcppconn-static.lib
    )

    # Post-build: Copy all .dll files from MySQLConnector to the output directory
    add_custom_command(TARGET Database POST_BUILD
    
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MySQLDirectory}/lib64/libcrypto-3-x64.dll"
            "$<TARGET_FILE_DIR:Database>/libcrypto-3-x64.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MySQLDirectory}/lib64/libssl-3-x64.dll"
            "$<TARGET_FILE_DIR:Database>/libssl-3-x64.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MySQLDirectory}/lib64/mysqlcppconn-10-vs14.dll"
            "$<TARGET_FILE_DIR:Database>/mysqlcppconn-10-vs14.dll"
    )
ENDIF()