// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_NET_GNS_INIT_HPP
#define IGN_NET_GNS_INIT_HPP

#include <mutex>
#include <steam/steamnetworkingsockets.h>

namespace ignite::net::internal
{
    inline std::mutex g_GnsMutex;
    inline int g_GnsRefCount = 0;

    inline bool InitGNS(SteamDatagramErrMsg &errMsg)
    {
        std::lock_guard<std::mutex> lock(g_GnsMutex);
        if (g_GnsRefCount == 0)
        {
            if (!GameNetworkingSockets_Init(nullptr, errMsg))
            {
                return false;
            }
        }
        ++g_GnsRefCount;
        return true;
    }

    inline void KillGNS()
    {
        std::lock_guard<std::mutex> lock(g_GnsMutex);
        if (--g_GnsRefCount == 0)
        {
            GameNetworkingSockets_Kill();
        }
    }
}

#endif
