// Defs.h - SDE Definitions and interfaces
#pragma once

namespace SDE
{

class IBreakPoint
{
public:
    virtual bool Initialize(HANDLE hProcess) = 0;
    virtual bool Enable(void) = 0;
    virtual bool Disable(void) = 0;
    virtual void Deinitialize(void) = 0;
    virtual bool IsTriggered(HANDLE hThread, EXCEPTION_DEBUG_INFO *pException) = 0;
    virtual LPVOID GetAddress(void) const = 0;
    virtual HANDLE GetThread(void) const = 0;
    virtual HANDLE GetProcess(void) const = 0;
};

class IDebuggerClient
{
public:
    virtual bool OnBreakPoint(IBreakPoint *pBreakPoint) 
    {
        return(false); // We didn't remove any breakpoints
    }
    virtual bool OnException(EXCEPTION_DEBUG_INFO *pException) 
    {
        return(false); // Not processed
    }
    virtual void OnThreadCreated(CREATE_THREAD_DEBUG_INFO *pCreateThread) {}
    virtual void OnThreadExit(EXIT_THREAD_DEBUG_INFO *pExitThread) {}
    virtual void OnProcessCreated(CREATE_PROCESS_DEBUG_INFO *pCreateProcess) {}
    virtual void OnProcessExit(EXIT_PROCESS_DEBUG_INFO *pExitProcess) {}
    virtual void OnModuleLoaded(LOAD_DLL_DEBUG_INFO *pLoadDll) {}
    virtual void OnModuleUnloaded(UNLOAD_DLL_DEBUG_INFO *pUnloadDll) {}
    virtual void OnDebugString(OUTPUT_DEBUG_STRING_INFO *pDebugString) {}
    virtual void SetDebuggedThread(DWORD dwPid, DWORD dwTid) {}
    virtual void OnFinishedDebugging(void) {}
};

typedef bool (*EnumBreakPointProc)(IBreakPoint *pBreakPoint, void *pUserData);

class IDebugger
{
public:
    virtual DWORD LoadProcess(LPCSTR pszProcessName, LPCSTR pszArguments, LPCSTR pszCurrentDirectory, LPVOID lpEnvironment, LPSTARTUPINFO psi) = 0;
    virtual bool AttachToProcess(DWORD dwPid) = 0;

    virtual void AddClient(IDebuggerClient *pClient) = 0;
    virtual void RemoveClient(IDebuggerClient *pClient) = 0;

    virtual bool AddBreakPoint(IBreakPoint *pBreakPoint) = 0;
    virtual bool RemoveBreakPoint(IBreakPoint *pBreakPoint) = 0;
    virtual int GetNumBreakPoints(void) = 0;
    virtual bool EnumBreakPoints(EnumBreakPointProc EnumProc, void *pUserData) = 0;

    virtual bool Go(void) = 0;
    virtual void Stop(void) = 0;
    virtual void Suspend(void) = 0;
    virtual void Resume(void) = 0;
};

}