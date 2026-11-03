// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_NET_GNS_INIT_HPP
#define IGN_NET_GNS_INIT_HPP

#include <mutex>
#include <cstdio>
#include <cstdint>
#include <steam/steamnetworkingsockets.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ignite::net::internal
{
    struct GnsControlBlock
    {
        std::mutex mutex;
        int refCount = 0;
    };

    inline GnsControlBlock *GetControlBlock()
    {
#if defined(_WIN32)
        char env[64] = { 0 };
        DWORD len = GetEnvironmentVariableA("__IGN_GNS_CTRL_PTR__", env, sizeof(env));
        if (len > 0)
        {
            uintptr_t ptrVal = 0;
            if (sscanf_s(env, "%llx", &ptrVal) == 1 && ptrVal != 0)
            {
                return reinterpret_cast<GnsControlBlock *>(ptrVal);
            }
        }

        static std::mutex s_LocalInitMutex;
        std::lock_guard<std::mutex> localLock(s_LocalInitMutex);

        len = GetEnvironmentVariableA("__IGN_GNS_CTRL_PTR__", env, sizeof(env));
        if (len > 0)
        {
            uintptr_t ptrVal = 0;
            if (sscanf_s(env, "%llx", &ptrVal) == 1 && ptrVal != 0)
            {
                return reinterpret_cast<GnsControlBlock *>(ptrVal);
            }
        }

        auto *block = new GnsControlBlock();
        char buf[64];
        sprintf_s(buf, "%llx", (unsigned long long)reinterpret_cast<uintptr_t>(block));
        SetEnvironmentVariableA("__IGN_GNS_CTRL_PTR__", buf);
        return block;
#else
        static GnsControlBlock s_Block;
        return &s_Block;
#endif
    }

    inline bool InitGNS(SteamDatagramErrMsg &errMsg)
    {
        GnsControlBlock *cb = GetControlBlock();
        std::lock_guard<std::mutex> lock(cb->mutex);
        if (cb->refCount == 0)
        {
            if (!GameNetworkingSockets_Init(nullptr, errMsg))
            {
                return false;
            }
        }
        ++cb->refCount;
        return true;
    }

    inline void KillGNS()
    {
        GnsControlBlock *cb = GetControlBlock();
        std::lock_guard<std::mutex> lock(cb->mutex);
        if (cb->refCount > 0)
        {
            --cb->refCount;
            if (cb->refCount == 0)
            {
                GameNetworkingSockets_Kill();
            }
        }
    }
}

#endif
