// BreakPointOnExecution.h - Breakpoint on execution (using INT3) header
#pragma once

#include "Defs.h"

namespace SDE
{

class CBreakPointOnExecution : public IBreakPoint
{
public:
    CBreakPointOnExecution(LPVOID lpVA = 0);
    virtual ~CBreakPointOnExecution();

    bool SetAddress(LPVOID lpVA);

    // IBreakPoint implementation
    virtual bool Initialize(HANDLE hProcess);
    virtual bool Enable(void);
    virtual bool Disable(void);
    virtual void Deinitialize(void);
    virtual bool IsTriggered(HANDLE hThread, EXCEPTION_DEBUG_INFO *pException);
    virtual HANDLE GetThread(void) const;
    virtual HANDLE GetProcess(void) const;
    virtual LPVOID GetAddress(void) const;

protected:
    bool ReadByte(void);

private:
    bool m_bIsInitialized;
    LPVOID m_lpVA;
    bool m_bIsEnabled;
    BYTE m_bySavedByte;
    HANDLE m_hProcess;
    HANDLE m_hThread;
};

}