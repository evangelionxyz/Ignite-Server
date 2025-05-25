add_library(IgniteNet
    source/BufferStream.cpp
    source/Client.cpp
    source/Console.cpp
    source/FileStream.cpp
    source/Server.cpp
    source/StreamReader.cpp
    source/StreamWriter.cpp
)

target_include_directories(IgniteNet PRIVATE
    "${GameNetworkingSocketsDir}/include"
)

if (WIN32)
    target_compile_definitions(IgniteNet PRIVATE
            _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(IgniteNet PUBLIC
            ${GameNetworkingSocketsDir}/bin/Windows/${CMAKE_BUILD_TYPE}/GameNetworkingSockets.lib
    )

#    add_custom_command(TARGET IgniteNet POST_BUILD
#
#            COMMAND ${CMAKE_COMMAND} -E copy_if_different
#            "${GameNetworkingSocketsDir}/bin/Windows/${CMAKE_BUILD_TYPE}/GameNetworkingSockets.dll"
#            "$<TARGET_FILE_DIR:IgniteNet>/GameNetworkingSockets.dll"
#
#            COMMAND ${CMAKE_COMMAND} -E copy_if_different
#            "${GameNetworkingSocketsDir}/bin/Windows/${CMAKE_BUILD_TYPE}/libssl-3-x64.dll"
#            "$<TARGET_FILE_DIR:IgniteNet>/libssl-3-x64.dll"
#
#            COMMAND ${CMAKE_COMMAND} -E copy_if_different
#            "${GameNetworkingSocketsDir}/bin/Windows/${CMAKE_BUILD_TYPE}/libprotobuff.dll"
#            "$<TARGET_FILE_DIR:IgniteNet>/libprotobuff.dll"
#    )
endif()