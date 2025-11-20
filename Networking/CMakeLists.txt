file(GLOB_RECURSE IGNITE_NET_SOURCE
    "source/**.cpp"
    "source/**.hpp"
    "source/**.h"
)

add_library(IgniteNet ${IGNITE_NET_SOURCE})

target_include_directories(IgniteNet PRIVATE "${GameNetworkingSocketsDir}/include")

if (WIN32)
    target_compile_definitions(IgniteNet PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
    
    target_link_libraries(IgniteNet PUBLIC
        $<$<CONFIG:Debug>:${GameNetworkingSocketsDir}/lib/Windows/Debug/GameNetworkingSockets.lib>
        $<$<CONFIG:Release>:${GameNetworkingSocketsDir}/lib/Windows/Release/GameNetworkingSockets.lib>
        $<$<CONFIG:RelWithDebInfo>:${GameNetworkingSocketsDir}/lib/Windows/Release/GameNetworkingSockets.lib>
        $<$<CONFIG:MinSizeRel>:${GameNetworkingSocketsDir}/lib/Windows/Release/GameNetworkingSockets.lib>
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(IgniteNet PUBLIC
        ${GameNetworkingSocketsDir}/lib/Linux/libGameNetworkingSockets.so
        ${GameNetworkingSocketsDir}/lib/Linux/libprotobuf.so.23
    )
endif()