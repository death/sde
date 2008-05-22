#pragma once

#include "Defs.h"

namespace SDE
{

class CInterruptFlagSet
{
public:
    static bool SetInterruptFlag(HANDLE hThread)
    {
        if (hThread == NULL)
            return(false);

        _CONTEXT ctx;
        ZeroMemory(&ctx, sizeof(_CONTEXT));
        ctx.ContextFlags = CONTEXT_CONTROL;
        GetThreadContext(hThread, &ctx);
        ctx.EFlags |= 0x100;
        SetThreadContext(hThread, &ctx);

        return(true);
    }
    static bool ClearInterruptFlag(HANDLE hThread)
    {
        if (hThread == NULL)
            return(false);

        _CONTEXT ctx;
        ZeroMemory(&ctx, sizeof(_CONTEXT));
        ctx.ContextFlags = CONTEXT_CONTROL;
        GetThreadContext(hThread, &ctx);
        ctx.EFlags &= ~0x100;
        SetThreadContext(hThread, &ctx);

        return(true);
    }
};

}