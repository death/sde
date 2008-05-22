// Debugger.h - SDE Debugger header
#pragma once

#include "Defs.h"
#include <map>
#include <deque>
#include <vector>

namespace SDE
{

class CDebugger : public IDebugger
{
    static const int cDebugStopEvent = 0;
    static const int cDebugStoppedEvent = 1;
    static const int cDebugRunningEvent = 2;
    static const int cNumEvents = 3;

    typedef std::deque<IBreakPoint *> BreakPointVector;
    typedef std::map<DWORD, HANDLE> Tid2Handle;
    typedef std::map<HANDLE, DWORD> Handle2Tid;
    typedef std::deque<IDebuggerClient *> ClientVector;
    typedef std::vector<BreakPointVector::iterator> BPVIteratorVector;
public:
    CDebugger();
    virtual ~CDebugger();

    // Implements IDebugger
    virtual DWORD LoadProcess(LPCSTR pszProcessName, LPCSTR pszArguments, LPCSTR pszCurrentDirectory, LPVOID lpEnvironment, LPSTARTUPINFO psi);
    virtual bool AttachToProcess(DWORD dwPid);

    virtual void AddClient(IDebuggerClient *pClient);
    virtual void RemoveClient(IDebuggerClient *pClient);

    virtual bool AddBreakPoint(IBreakPoint *pBreakPoint);
    virtual bool RemoveBreakPoint(IBreakPoint *pBreakPoint);
    virtual int GetNumBreakPoints(void);
    virtual bool EnumBreakPoints(EnumBreakPointProc EnumProc, void *pUserData);

    virtual bool Go(void);
    virtual void Stop(void);
    virtual void Suspend(void);
    virtual void Resume(void);

    BOOL ReadProcessMemory(LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T *lpNumberOfBytesRead);
    BOOL WriteProcessMemory(LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T *lpNumberOfBytesWritten);
    int GetCStringFromProcess(LPCVOID lpBaseAddress, LPSTR lpString, SIZE_T nMaxString, BOOL bUnicode);

    DWORD GetThreadId(HANDLE hThread) const;
    HANDLE GetThreadHandle(DWORD dwTid) const;

    void SetMultithread(bool bMultithread);
    
protected:
    static void DebuggerThread(void *pParam);

private:
    void CloseProcessHandles(void);
    void CheckSuspend(void);

private:
    ClientVector m_vClients;
    BreakPointVector m_BreakPoints;
    bool m_bProcessLoaded;
    bool m_bActiveDebug;
    volatile LONG m_lProcessesCreated;
    PROCESS_INFORMATION m_pi;
    HANDLE m_hEvents[cNumEvents];
    Tid2Handle m_Tid2Handle;
    Handle2Tid m_Handle2Tid;
    bool m_bMultithread;
    LONG m_lSuspendCount;
};

}