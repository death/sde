// BreakPointOnExecution.cpp - Breakpoint on execution (using INT3) implementation
#include "StdAfx.h"
#include "BreakPointOnExecution.h"

#pragma warning(disable:4312) // 'reinterpret_cast' : conversion from 'DWORD' to 'LPCVOID' of greater size

namespace SDE
{

CBreakPointOnExecution::CBreakPointOnExecution(LPVOID lpVA)
: m_lpVA(lpVA)
, m_bIsEnabled(false)
, m_bIsInitialized(false)
, m_hProcess(NULL)
{
}

CBreakPointOnExecution::~CBreakPointOnExecution()
{
    if (m_bIsInitialized == true)
        Deinitialize();
}

bool CBreakPointOnExecution::SetAddress(LPVOID lpVA)
{
    bool bIsEnabled = m_bIsEnabled;

    if (bIsEnabled == true)
        Disable();

    m_lpVA = lpVA;

    if (m_hProcess)
        ReadByte();

    if (bIsEnabled == true)
        Enable();

    return(true);
}

LPVOID CBreakPointOnExecution::GetAddress(void) const
{
    return(m_lpVA);
}

bool CBreakPointOnExecution::Initialize(HANDLE hProcess)
{
    // Breakpoint address must be initialized
    if (m_lpVA == 0) 
        return(false);

    // Initialize member variables
    m_bIsInitialized = true;
    m_hProcess = hProcess;

    return(ReadByte());
}

bool CBreakPointOnExecution::Enable(void)
{
    // Must be initialized first
    if (m_bIsInitialized == false)
        return(false);

    if (m_lpVA == 0)
        return(false);

    // Place an INT3 in the address
    BYTE byInt3 = 0xCC;

    DWORD dwNumWritten = 0;
    WriteProcessMemory(m_hProcess, m_lpVA, &byInt3, 1, &dwNumWritten);
    FlushInstructionCache(m_hProcess, m_lpVA, dwNumWritten);

    if (dwNumWritten == 0)
        return(false);

    // Set enabled flag
    m_bIsEnabled = true;

    return(true);
}

bool CBreakPointOnExecution::Disable(void)
{
    // Must be initialized first
    if (m_bIsInitialized == false)
        return(false);

    if (m_lpVA == 0)
        return(false);

    // Place the original byte in the address
    DWORD dwNumWritten = 0;
    WriteProcessMemory(m_hProcess, m_lpVA, &m_bySavedByte, 1, &dwNumWritten);
    FlushInstructionCache(m_hProcess, m_lpVA, dwNumWritten);

    if (dwNumWritten == 0)
        return(false);

    // Set enabled flag
    m_bIsEnabled = false;

    return(true);
}

void CBreakPointOnExecution::Deinitialize(void)
{
    m_bIsInitialized = false;
}

bool CBreakPointOnExecution::IsTriggered(HANDLE hThread, EXCEPTION_DEBUG_INFO *pException)
{
    // Must be initialized and enabled
    if (m_bIsInitialized == false || m_bIsEnabled == false)
        return(false);

    // Must be a first-chance exception
    if (pException->dwFirstChance == 0)
        return(false);

    // Exception must be of EXCEPTION_BREAKPOINT type
    if (pException->ExceptionRecord.ExceptionCode != EXCEPTION_BREAKPOINT)
        return(false);

    // Did this exception occur because of our breakpoint?
    if (pException->ExceptionRecord.ExceptionAddress != m_lpVA)
        return(false);

    // It's our breakpoint, set EIP back to INT3
    _CONTEXT ctx;
    ZeroMemory(&ctx, sizeof(_CONTEXT));
    ctx.ContextFlags = CONTEXT_CONTROL;
    GetThreadContext(hThread, &ctx);
    ctx.Eip--;
    SetThreadContext(hThread, &ctx);

    m_hThread = hThread;

    return(true);
}

bool CBreakPointOnExecution::ReadByte(void)
{
    // Save the byte from the address
    DWORD dwNumRead = 0;
    ReadProcessMemory(m_hProcess, m_lpVA, &m_bySavedByte, 1, &dwNumRead);

    if (dwNumRead == 0)
        return(false);

    return(true);
}

HANDLE CBreakPointOnExecution::GetThread(void) const
{
    return(m_hThread);
}

HANDLE CBreakPointOnExecution::GetProcess(void) const
{
    return(m_hProcess);
}

}