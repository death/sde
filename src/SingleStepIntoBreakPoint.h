// SingleStepBreakPoint.h - Single step breakpoint header
#pragma once

#include "Defs.h"

namespace SDE
{

class CSingleStepIntoBreakPoint : public IBreakPoint
{
public:
    CSingleStepIntoBreakPoint(HANDLE hThread);
    virtual ~CSingleStepIntoBreakPoint();

    virtual bool Initialize(HANDLE hProcess);
    virtual bool Enable(void);
    virtual bool Disable(void);
    virtual void Deinitialize(void);
    virtual bool IsTriggered(HANDLE hThread, EXCEPTION_DEBUG_INFO *pException);
    virtual LPVOID GetAddress(void) const;
    virtual HANDLE GetThread(void) const;
    virtual HANDLE GetProcess(void) const;

private:
    HANDLE m_hProcess;
    HANDLE m_hThread;
    bool m_bIsInitialized;
    bool m_bIsEnabled;
    LPVOID m_lpVA;
};

}