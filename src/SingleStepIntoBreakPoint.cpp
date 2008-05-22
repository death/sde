// SingleStepBreakPoint.h - Single step breakpoint implementation
#include "StdAfx.h"
#include "SingleStepIntoBreakPoint.h"
#include "InterruptFlagSet.h"

#pragma warning(disable:4311) // 'reinterpret_cast' : pointer truncation from 'PVOID' to 'DWORD'


namespace SDE
{

CSingleStepIntoBreakPoint::CSingleStepIntoBreakPoint(HANDLE hThread)
: m_hProcess(NULL)
, m_bIsInitialized(false)
, m_bIsEnabled(false)
, m_hThread(hThread)
{
}

CSingleStepIntoBreakPoint::~CSingleStepIntoBreakPoint()
{
    if (m_bIsInitialized == true)
        Deinitialize();
}

bool CSingleStepIntoBreakPoint::Initialize(HANDLE hProcess)
{
    m_hProcess = hProcess;
    m_bIsInitialized = true;
    return(true);
}

bool CSingleStepIntoBreakPoint::Enable(void)
{
    // Must be initialized
    if (m_bIsInitialized == false)
        return(false);

    // Is already enabled?
    if (m_bIsEnabled == true)
        return(false);

    // Set IF
    if (CInterruptFlagSet::SetInterruptFlag(m_hThread) == false)
        return(false);

    m_bIsEnabled = true;

    return(true);
}

bool CSingleStepIntoBreakPoint::Disable(void)
{
    // Must be initialized
    if (m_bIsInitialized == false)
        return(false);

    // Is already disabled?
    if (m_bIsEnabled == false)
        return(false);

    // Clear IF
    if (CInterruptFlagSet::ClearInterruptFlag(m_hThread) == false)
        return(false);

    m_bIsEnabled = false;

    return(true);
}

void CSingleStepIntoBreakPoint::Deinitialize(void)
{
    m_bIsInitialized = false;
}

bool CSingleStepIntoBreakPoint::IsTriggered(HANDLE hThread, EXCEPTION_DEBUG_INFO *pException)
{
    // Must be initialized and enabled
    if (m_bIsInitialized == false || m_bIsEnabled == false)
        return(false);

    // Must be the same thread
    if (hThread != m_hThread)
        return(false);

    // Must be a first-chance exception
    if (pException->dwFirstChance == 0)
        return(false);

    // Exception must be of EXCEPTION_SINGLE_STEP type
    if (pException->ExceptionRecord.ExceptionCode != EXCEPTION_SINGLE_STEP)
        return(false);

    m_lpVA = pException->ExceptionRecord.ExceptionAddress;
    CInterruptFlagSet::SetInterruptFlag(m_hThread);

    return(true);
}

LPVOID CSingleStepIntoBreakPoint::GetAddress(void) const
{
    return(m_lpVA);
}

HANDLE CSingleStepIntoBreakPoint::GetThread(void) const
{
    return(m_hThread);
}

HANDLE CSingleStepIntoBreakPoint::GetProcess(void) const
{
    return(m_hProcess);
}

}