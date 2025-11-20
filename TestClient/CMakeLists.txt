file(GLOB_RECURSE TEST_CLIENT_SOURCE
    "source/**.cpp"
    "source/**.hpp"
    "source/**.h"
)

add_executable(TestClient ${TEST_CLIENT_SOURCE})

target_link_libraries(TestClient PRIVATE IgniteNet)
target_include_directories(TestClient PRIVATE
    "${IGNITE_NET_SOURCE_DIR}"
    "${GameNetworkingSocketsDir}/include"
)

if(WIN32)
target_link_libraries(TestClient PUBLIC
    $<$<CONFIG:Debug>:${GameNetworkingSocketsDir}/lib/Windows/Debug/GameNetworkingSockets.lib>
    $<$<CONFIG:Release>:${GameNetworkingSocketsDir}/lib/Windows/Release/GameNetworkingSockets.lib>
    $<$<CONFIG:RelWithDebInfo>:${GameNetworkingSocketsDir}/lib/Windows/Release/GameNetworkingSockets.lib>
    $<$<CONFIG:MinSizeRel>:${GameNetworkingSocketsDir}/lib/Windows/Release/GameNetworkingSockets.lib>
)

add_custom_command(TARGET TestClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GameNetworkingSocketsDir}/lib/Windows/$<CONFIG>/GameNetworkingSockets.dll"
    "$<TARGET_FILE_DIR:TestClient>/GameNetworkingSockets.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GameNetworkingSocketsDir}/lib/Windows/$<CONFIG>/libcrypto-3-x64.dll"
    "$<TARGET_FILE_DIR:TestClient>/libcrypto-3-x64.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GameNetworkingSocketsDir}/lib/Windows/$<CONFIG>/libprotobuf$<$<CONFIG:Debug>:d>.dll"
    "$<TARGET_FILE_DIR:TestClient>/libprotobuf$<$<CONFIG:Debug>:d>.dll"
)
elseif(UNIX AND NOT APPLE)
target_link_libraries(TestClient PUBLIC
    ${GameNetworkingSocketsDir}/lib/Linux/libGameNetworkingSockets.so
    ${GameNetworkingSocketsDir}/lib/Linux/libprotobuf.so.23
)

add_custom_command(TARGET TestClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GameNetworkingSocketsDir}/lib/Linux/libGameNetworkingSockets.so"
    "$<TARGET_FILE_DIR:TestClient>/libGameNetworkingSockets.so"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GameNetworkingSocketsDir}/lib/Linux/libprotobuf.so.23"
    "$<TARGET_FILE_DIR:TestClient>/libprotobuf.so.23"
)
endif()