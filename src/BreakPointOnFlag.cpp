#include "StdAfx.h"
#include "BreakPointOnFlag.h"

namespace SDE
{

CBreakPointOnFlag::CBreakPointOnFlag(IDebugger & debugger, HANDLE hThread, DWORD dwFlag, bool bOnFlagSet)
: m_bIsInitialized(false)
, m_bIsEnabled(false)
, m_dwFlag(dwFlag)
, m_bOnFlagSet(bOnFlagSet)
, m_lpVA(0)
, m_hProcess(0)
, m_hThread(hThread)
, m_pss(0)
, m_debugger(debugger)
, m_bIsTriggered(false)
{
    m_pss = new CSingleStepIntoBreakPoint(m_hThread);
}

CBreakPointOnFlag::~CBreakPointOnFlag()
{
    if (m_bIsInitialized == true)
        Deinitialize();

    if (m_pss) {
        delete m_pss;
    }
}

void CBreakPointOnFlag::SetFlag(DWORD dwFlag, bool bOnFlagSet)
{
    m_dwFlag = dwFlag;
    m_bOnFlagSet = bOnFlagSet;
}

// IBreakPoint implementation
bool CBreakPointOnFlag::Initialize(HANDLE hProcess)
{
    // A flag must be specified
    if (m_dwFlag == 0)
        return(false);

    // Initialize member variables
    m_bIsInitialized = true;
    m_hProcess = m_hProcess;

    return(true);
}

bool CBreakPointOnFlag::Enable(void)
{
    // Must be initialized
    if (m_bIsInitialized == false)
        return(false);

    // A flag must be specified
    if (m_dwFlag == 0)
        return(false);

    m_debugger.AddClient(this);
    m_debugger.AddBreakPoint(m_pss);
    m_pss->Enable();

    // Set enabled flag
    m_bIsEnabled = true;

    return(true);
}

bool CBreakPointOnFlag::Disable(void)
{
    // Must be initialized
    if (m_bIsInitialized == false)
        return(false);

    m_pss->Disable();
    m_debugger.RemoveBreakPoint(m_pss);
    m_debugger.RemoveClient(this);

    // Set disable flag
    m_bIsEnabled = false;

    return(true);
}

void CBreakPointOnFlag::Deinitialize(void)
{
    m_bIsInitialized = false;
}

bool CBreakPointOnFlag::IsTriggered(HANDLE hThread, EXCEPTION_DEBUG_INFO *pException)
{
    // Must be initialized and enabled
    if (m_bIsInitialized == false || m_bIsEnabled == false)
        return(false);

    if (hThread != m_hThread)
        return(false);

    return(m_bIsTriggered);
}

HANDLE CBreakPointOnFlag::GetThread(void) const
{
    return(m_hThread);
}

HANDLE CBreakPointOnFlag::GetProcess(void) const
{
    return(m_hProcess);
}

LPVOID CBreakPointOnFlag::GetAddress(void) const
{
    return(m_lpVA);
}

bool CBreakPointOnFlag::OnBreakPoint(IBreakPoint *pBreakPoint)
{
    CSingleStepIntoBreakPoint *pSingleStep = dynamic_cast<CSingleStepIntoBreakPoint *>(pBreakPoint);

    if (pSingleStep == m_pss) {
        // Our single stepper, check if the flag meets our requirements
        _CONTEXT ctx;
        ZeroMemory(&ctx, sizeof(_CONTEXT));
        ctx.ContextFlags = CONTEXT_CONTROL;
        GetThreadContext(m_hThread, &ctx);

        if (m_bOnFlagSet == true) {
            m_bIsTriggered = (ctx.EFlags & m_dwFlag) ? true : false;
        } else {
            m_bIsTriggered = (ctx.EFlags & ~m_dwFlag) ? true : false;
        }

        if (m_bIsTriggered == true) {
            m_lpVA = pBreakPoint->GetAddress();
        }
    }

    return(false); // We didn't remove any breakpoints
}

}