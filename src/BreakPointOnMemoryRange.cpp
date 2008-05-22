#include "StdAfx.h"
#include "breakpointonmemoryrange.h"
#include <algorithm>

using namespace std;

namespace SDE {

CBreakPointOnMemoryRange::Debugger2RangeVector CBreakPointOnMemoryRange::m_Ranges;

CBreakPointOnMemoryRange::CBreakPointOnMemoryRange(IDebugger & debugger, RangePair range)
: m_bIsEnabled(false)
, m_bIsInitialized(false)
, m_hProcess(NULL)
, m_lpVA(0)
, m_debugger(debugger)
, m_pss(0)
, m_bWaitForBreakPoint(false)
{
    m_Ranges[&debugger].push_back(range);
}

CBreakPointOnMemoryRange::~CBreakPointOnMemoryRange()
{
    if (m_bIsInitialized == true)
        Deinitialize();

    if (m_pss) {
        delete m_pss;
    }
}

bool CBreakPointOnMemoryRange::AddRange(RangePair range)
{
    bool bIsEnabled = m_bIsEnabled;

    if (bIsEnabled == true)
        Disable();

    m_Ranges[&m_debugger].push_back(range);

    if (bIsEnabled == true)
        Enable();

    return(true);
}

bool CBreakPointOnMemoryRange::RemoveRange(RangePair range)
{
    bool bIsEnabled = m_bIsEnabled;

    if (bIsEnabled == true)
        Disable();

    RangeVector & ranges = m_Ranges[&m_debugger];
    RangeVector::iterator i = find(ranges.begin(), ranges.end(), range);
    if (i != ranges.end()) {
        ranges.erase(i);
    }

    if (bIsEnabled == true)
        Enable();

    return(true);
}

LPVOID CBreakPointOnMemoryRange::GetAddress(void) const
{
    return(m_lpVA);
}

bool CBreakPointOnMemoryRange::Initialize(HANDLE hProcess)
{
    // Breakpoint address must be initialized
    if (InvalidRange())
        return(false);

    // Initialize member variables
    m_bIsInitialized = true;
    m_hProcess = hProcess;

    return(true);
}

bool CBreakPointOnMemoryRange::Enable(void)
{
    // Must be initialized first
    if (m_bIsInitialized == false)
        return(false);

    if (InvalidRange())
        return(false);

    // Set guard pages
    if (SetGuardPages() == false)
        return(false);

    // Set enabled flag
    m_bIsEnabled = true;

    return(true);
}

bool CBreakPointOnMemoryRange::Disable(void)
{
    // Must be initialized first
    if (m_bIsInitialized == false)
        return(false);

    if (InvalidRange())
        return(false);

    // Remove guard pages
    if (RemoveGuardPages() == false)
        return(false);

    // Set enabled flag
    m_bIsEnabled = false;

    return(true);
}

void CBreakPointOnMemoryRange::Deinitialize(void)
{
    m_bIsInitialized = false;
}

bool CBreakPointOnMemoryRange::IsTriggered(HANDLE hThread, EXCEPTION_DEBUG_INFO *pException)
{
    // Must be initialized and enabled, and not waiting for breakpoint
    if (m_bIsInitialized == false || m_bIsEnabled == false || m_bWaitForBreakPoint == true)
        return(false);

    // Must be a first-chance exception
    if (pException->dwFirstChance == 0)
        return(false);

    // Exception must be of EXCEPTION_BREAKPOINT type
    if (pException->ExceptionRecord.ExceptionCode != EXCEPTION_GUARD_PAGE)
        return(false);

    // Did this exception occur because of our breakpoint?
    ULONG_PTR ulpAddress = pException->ExceptionRecord.ExceptionInformation[1];
    RangeVector & ranges = m_Ranges[&m_debugger];
    for (size_t i = 0; i < ranges.size(); i++) {
        RangePair & range = ranges[i];
        if (!(ulpAddress >= reinterpret_cast<ULONG_PTR>(range.first) && ulpAddress <= reinterpret_cast<ULONG_PTR>(range.second))) {
            // Did we cause the exception?
            ulpAddress &= 0xFFFFF000;
            ULONG_PTR ulpLow = reinterpret_cast<ULONG_PTR>(range.first) & 0xFFFFF000;
            ULONG_PTR ulpHigh = reinterpret_cast<ULONG_PTR>(range.second) & 0xFFFFF000;
            if (ulpAddress >= ulpLow && ulpAddress <= ulpHigh) {
                // Continue to next instruction and set guard pages again
                if (m_pss) {
                    delete m_pss;
                    m_pss = 0;
                }

                m_pss = new CSingleStepIntoBreakPoint(hThread);
                m_debugger.AddBreakPoint(m_pss);
                m_pss->Enable();
                m_debugger.AddClient(this);

                m_bWaitForBreakPoint = true;

                pException->dwFirstChance = 0xBADC0DE;
            }
            return(false);
        }
    }

    // Set guard pages
    SetGuardPages();

    // Set virtual address
    m_lpVA = reinterpret_cast<LPVOID>(pException->ExceptionRecord.ExceptionInformation[1]);

    m_hThread = hThread;

    return(true);
}

bool CBreakPointOnMemoryRange::SetGuardPages(void)
{
    DWORD dwOldProtect = 0;
    RangeVector & ranges = m_Ranges[&m_debugger];
    for (size_t i = 0; i < ranges.size(); i++) {
        RangePair & range = ranges[i];

        ULONG_PTR ulpDelta = reinterpret_cast<ULONG_PTR>(range.second) - reinterpret_cast<ULONG_PTR>(range.first) + 1;
        VirtualProtectEx(m_hProcess, range.first, ulpDelta, PAGE_EXECUTE_READWRITE, &dwOldProtect);
        VirtualProtectEx(m_hProcess, range.first, ulpDelta, dwOldProtect | PAGE_GUARD, &dwOldProtect);
    }
    return(true);
}

bool CBreakPointOnMemoryRange::RemoveGuardPages(void)
{
    DWORD dwOldProtect = 0;
    RangeVector & ranges = m_Ranges[&m_debugger];
    for (size_t i = 0; i < ranges.size(); i++) {
        RangePair & range = ranges[i];

        ULONG_PTR ulpDelta = reinterpret_cast<ULONG_PTR>(range.second) - reinterpret_cast<ULONG_PTR>(range.first) + 1;
        VirtualProtectEx(m_hProcess, range.first, ulpDelta, PAGE_EXECUTE_READWRITE, &dwOldProtect);
        VirtualProtectEx(m_hProcess, range.first, ulpDelta, dwOldProtect & ~PAGE_GUARD, &dwOldProtect);
    }
    return(true);
}

HANDLE CBreakPointOnMemoryRange::GetThread(void) const
{
    return(m_hThread);
}

HANDLE CBreakPointOnMemoryRange::GetProcess(void) const
{
    return(m_hProcess);
}

bool CBreakPointOnMemoryRange::OnBreakPoint(IBreakPoint *pBreakPoint)
{
    CSingleStepIntoBreakPoint *pBp = dynamic_cast<CSingleStepIntoBreakPoint *>(pBreakPoint);

    if (pBp == m_pss) {
        m_pss->Disable();
        m_debugger.RemoveBreakPoint(pBreakPoint);
        delete m_pss;
        m_pss = 0;

        m_debugger.RemoveClient(this);

        SetGuardPages();

        m_bWaitForBreakPoint = false;

        return(true); // We removed a breakpoint
    }

    return(false); // We didn't remove any breakpoints
}

bool CBreakPointOnMemoryRange::InvalidRange(void) const
{
    RangeVector & ranges = m_Ranges[&m_debugger];

    if (ranges.empty() == true)
        return(false);

    for (size_t i = 0; i < ranges.size(); i++) {
        RangePair & range = ranges[i];

        if (range.first == 0 || range.second == 0)
            return(true);
    }
    return(false);
}

void CBreakPointOnMemoryRange::OnFinishedDebugging(void)
{
    if (m_pss) {
        m_pss->Disable();
        m_debugger.RemoveBreakPoint(m_pss);
        delete m_pss;
        m_pss = 0;
    }

    m_debugger.RemoveClient(this);
}

}

