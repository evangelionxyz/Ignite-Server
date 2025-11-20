file(GLOB_RECURSE TEST_SERVER_SOURCE
    "source/**.cpp"
    "source/**.hpp"
    "source/**.h"
)

add_executable(TestServer ${TEST_SERVER_SOURCE})

target_link_libraries(TestServer PRIVATE IgniteNet)
target_include_directories(TestServer PRIVATE
    "${IGNITE_NET_SOURCE_DIR}"
    "${GameNetworkingSocketsDir}/include"
)

if(WIN32)
target_link_libraries(TestServer PUBLIC
    $<$<CONFIG:Debug>:${GameNetworkingSocketsDir}/lib/Windows/Debug/GameNetworkingSockets.lib>
    $<$<CONFIG:Release>:${GameNetworkingSocketsDir}/lib/Windows/Release/GameNetworkingSockets.lib>
    $<$<CONFIG:RelWithDebInfo>:${GameNetworkingSocketsDir}/lib/Windows/Release/GameNetworkingSockets.lib>
    $<$<CONFIG:MinSizeRel>:${GameNetworkingSocketsDir}/lib/Windows/Release/GameNetworkingSockets.lib>
)

add_custom_command(TARGET TestServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GameNetworkingSocketsDir}/lib/Windows/$<CONFIG>/GameNetworkingSockets.dll"
    "$<TARGET_FILE_DIR:TestServer>/GameNetworkingSockets.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GameNetworkingSocketsDir}/lib/Windows/$<CONFIG>/libcrypto-3-x64.dll"
    "$<TARGET_FILE_DIR:TestServer>/libcrypto-3-x64.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GameNetworkingSocketsDir}/lib/Windows/$<CONFIG>/libprotobuf$<$<CONFIG:Debug>:d>.dll"
    "$<TARGET_FILE_DIR:TestServer>/libprotobuf$<$<CONFIG:Debug>:d>.dll"
)
endif()