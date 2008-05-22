#pragma once

#include "Defs.h"
#include "SingleStepIntoBreakPoint.h"

namespace SDE
{

class CBreakPointOnFlag : public IBreakPoint, public IDebuggerClient
{
public:
    CBreakPointOnFlag(IDebugger & debugger, HANDLE hThread, DWORD dwFlag = 0, bool bOnFlagSet = true);
    ~CBreakPointOnFlag();

    void SetFlag(DWORD dwFlag, bool bOnFlagSet = true);

    // IBreakPoint implementation
    virtual bool Initialize(HANDLE hProcess);
    virtual bool Enable(void);
    virtual bool Disable(void);
    virtual void Deinitialize(void);
    virtual bool IsTriggered(HANDLE hThread, EXCEPTION_DEBUG_INFO *pException);
    virtual HANDLE GetThread(void) const;
    virtual HANDLE GetProcess(void) const;
    virtual LPVOID GetAddress(void) const;
    virtual bool OnBreakPoint(IBreakPoint *pBreakPoint);

private:
    bool m_bIsInitialized;
    bool m_bIsEnabled;
    LPVOID m_lpVA;
    HANDLE m_hProcess;
    HANDLE m_hThread;
    
    DWORD m_dwFlag;
    bool m_bOnFlagSet;

    bool m_bIsTriggered;

    IDebugger & m_debugger;
    CSingleStepIntoBreakPoint *m_pss;
};

}