// SingleStepBreakPoint.h - Single step breakpoint header
#pragma once

#include "Defs.h"
#include "BreakPointOnExecution.h"

namespace SDE
{

class CSingleStepOverBreakPoint : public IBreakPoint, public IDebuggerClient
{
public:
    CSingleStepOverBreakPoint(IDebugger & debugger, HANDLE hThread);
    virtual ~CSingleStepOverBreakPoint();

    virtual bool Initialize(HANDLE hProcess);
    virtual bool Enable(void);
    virtual bool Disable(void);
    virtual void Deinitialize(void);
    virtual bool IsTriggered(HANDLE hThread, EXCEPTION_DEBUG_INFO *pException);
    virtual LPVOID GetAddress(void) const;
    virtual HANDLE GetThread(void) const;
    virtual HANDLE GetProcess(void) const;
    virtual bool OnBreakPoint(IBreakPoint *pBreakPoint);
    virtual void OnFinishedDebugging(void);

private:
    HANDLE m_hProcess;
    HANDLE m_hThread;
    bool m_bIsInitialized;
    bool m_bIsEnabled;
    LPVOID m_lpVA;
    CBreakPointOnExecution *m_pBpOnExec;
    IDebugger & m_debugger;
    LPVOID m_lpMem;
    bool m_bShouldFree;
    bool m_bWaitForBreakPoint;
};

}