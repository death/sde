#pragma once

#include "Defs.h"
#include "SingleStepIntoBreakpoint.h"
#include <utility>
#include <map>
#include <vector>

namespace SDE {

class CBreakPointOnMemoryRange : public IBreakPoint, public IDebuggerClient
{
    typedef std::pair<LPVOID, LPVOID> RangePair;
    typedef std::vector<RangePair> RangeVector;
    typedef std::map<IDebugger *, RangeVector> Debugger2RangeVector;
public:
    CBreakPointOnMemoryRange(IDebugger & debugger, RangePair range);
    virtual ~CBreakPointOnMemoryRange(void);

    bool AddRange(RangePair range);
    bool RemoveRange(RangePair range);

    virtual bool OnBreakPoint(IBreakPoint *pBreakPoint);
    virtual bool Initialize(HANDLE hProcess);
    virtual bool Enable(void);
    virtual bool Disable(void);
    virtual void Deinitialize(void);
    virtual bool IsTriggered(HANDLE hThread, EXCEPTION_DEBUG_INFO *pException);
    virtual LPVOID GetAddress(void) const;
    virtual HANDLE GetThread(void) const;
    virtual HANDLE GetProcess(void) const;
    bool InvalidRange(void) const;
    virtual void OnFinishedDebugging(void);

protected:
    bool SetGuardPages(void);
    bool RemoveGuardPages(void);

private:
    bool m_bIsInitialized;
    LPVOID m_lpVA;
    bool m_bIsEnabled;
    HANDLE m_hProcess;
    HANDLE m_hThread;
    IDebugger & m_debugger;
    CSingleStepIntoBreakPoint *m_pss;
    bool m_bWaitForBreakPoint;

    static Debugger2RangeVector m_Ranges;
};

}