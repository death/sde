// SDETest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "SingleStepIntoBreakPoint.h"
#include "SingleStepOverBreakPoint.h"
#include "BreakPointOnExecution.h"
#include "BreakPointOnMemoryRange.h"
#include "BreakPointOnFlag.h"
#include "Debugger.h"

using namespace SDE;
using namespace std;

#pragma warning(disable:4311)

CDebugger debug;

class CDebuggerClient : public IDebuggerClient
{
public:
    CDebuggerClient()
        : m_psso(0)
        , m_pBpf(0)
    {
    }
    ~CDebuggerClient()
    {
        if (m_psso) {
            debug.RemoveBreakPoint(m_psso);
            delete m_psso;
        }
        if (m_pBpf) {
            debug.RemoveBreakPoint(m_pBpf);
            delete m_pBpf;
        }
    }
    virtual bool OnBreakPoint(IBreakPoint *pBreakPoint) 
    {
        CBreakPointOnExecution *bpExec = dynamic_cast<CBreakPointOnExecution *>(pBreakPoint);
        CSingleStepOverBreakPoint *bpSSO = dynamic_cast<CSingleStepOverBreakPoint *>(pBreakPoint);
        CBreakPointOnFlag *bpFlag = dynamic_cast<CBreakPointOnFlag *>(pBreakPoint);

        if (bpExec == &m_bpStart) {
            bpExec->Disable();
            debug.RemoveBreakPoint(bpExec);
            printf("(Start breakpoint)\n");

            // Set single stepping
            m_psso = new CSingleStepOverBreakPoint(debug, bpExec->GetThread());
            debug.AddBreakPoint(m_psso);
            m_psso->Enable();

            return(true); // We removed a breakpoint
        } else if (bpSSO && bpSSO == m_psso) {
            // Get EFlags
            _CONTEXT ctx;
            ZeroMemory(&ctx, sizeof(_CONTEXT));
            ctx.ContextFlags = CONTEXT_CONTROL;
            GetThreadContext(bpSSO->GetThread(), &ctx);
        
            printf("(SSO breakpoint: %p, Eflags = %08lX)\n", bpSSO->GetAddress(), ctx.EFlags);

            if (bpSSO->GetAddress() == reinterpret_cast<LPVOID>(0x4013AD)) {
                // Set a new breakpoint on a flag change
                m_pBpf = new CBreakPointOnFlag(debug, bpSSO->GetThread(), 0x40);
                debug.AddBreakPoint(m_pBpf);
                bpSSO->Disable(); // Only one SingleStep class can be used
                m_pBpf->Enable();
            }
        } else if (m_pBpf && m_pBpf == bpFlag) {
            _CONTEXT ctx;
            ZeroMemory(&ctx, sizeof(_CONTEXT));
            ctx.ContextFlags = CONTEXT_CONTROL;
            GetThreadContext(bpFlag->GetThread(), &ctx);

            printf("Flag breakpoint! (%p, Eflags = %08lX)\n", bpFlag->GetAddress(), ctx.EFlags);

            m_pBpf->Disable();
            m_psso->Enable();
        }

        return(false); // We didn't remove any breakpoints
    }
    virtual bool OnException(EXCEPTION_DEBUG_INFO *pException) 
    {
        printf("exception (%08lX - %p)!\n", pException->ExceptionRecord.ExceptionCode, pException->ExceptionRecord.ExceptionAddress);
        return(true); // Processed
    }
    virtual void OnThreadCreated(CREATE_THREAD_DEBUG_INFO *pCreateThread) 
    {
        printf("thread created (%p)\n", pCreateThread->lpStartAddress);
    }
    virtual void OnThreadExit(EXIT_THREAD_DEBUG_INFO *pExitThread) 
    {
        printf("thread exit\n");
    }
    virtual void OnProcessCreated(CREATE_PROCESS_DEBUG_INFO *pCreateProcess) 
    {
        printf("process created (%p)\n", pCreateProcess->lpStartAddress);
        m_lpStart = pCreateProcess->lpStartAddress;
        m_bpStart.SetAddress(m_lpStart);
        debug.AddBreakPoint(&m_bpStart);
        m_bpStart.Enable();
    }
    virtual void OnProcessExit(EXIT_PROCESS_DEBUG_INFO *pExitProcess) 
    {
        printf("process exit\n");
        debug.Stop();
    }
    virtual void OnModuleLoaded(LOAD_DLL_DEBUG_INFO *pLoadDll) 
    {
        char szImageName[128];
        szImageName[0] = '\0';
        if (pLoadDll->lpImageName) {
            DWORD dwRead;
            LPVOID lpAddr;
            debug.ReadProcessMemory(pLoadDll->lpImageName, &lpAddr, 4, &dwRead);
            if (dwRead && lpAddr) 
                debug.GetCStringFromProcess(lpAddr, szImageName, pLoadDll->fUnicode == TRUE ? 64 : 128, pLoadDll->fUnicode);
        }
        if (szImageName[0]) {
            if (pLoadDll->fUnicode)
                printf("module loaded (%S)\n", szImageName);
            else
                printf("module loaded (%s)\n", szImageName);
        } else {
            printf("module loaded\n");
        }
    }
    virtual void OnModuleUnloaded(UNLOAD_DLL_DEBUG_INFO *pUnloadDll) 
    {
        printf("module unloaded\n");
    }
    virtual void OnDebugString(OUTPUT_DEBUG_STRING_INFO *pDebugString) 
    {
        printf("debug string\n");
    }
    virtual void SetDebuggedThread(DWORD dwPid, DWORD dwTid) 
    {
        m_dwPid = dwPid;
        m_dwTid = dwTid;
    }
    virtual void OnFinishedDebugging(void) 
    {
        printf("finished debugging\n");
    }

private:
    DWORD m_dwPid;
    DWORD m_dwTid;
    LPVOID m_lpStart;

    CBreakPointOnExecution m_bpStart;
    CSingleStepOverBreakPoint *m_psso;
    CBreakPointOnFlag *m_pBpf;
};

int main(int argc, char * argv[])
{
    STARTUPINFO si;
    
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    
    DWORD dwPid = debug.LoadProcess("main.exe", "main.exe", NULL, NULL, &si);

    if (dwPid) {
        CDebuggerClient client;
        debug.AddClient(&client);
        debug.Go();
    }

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

/*
CBreakPointOnExecution bp;
CSingleStepOverBreakPoint *pss = 0;
CBreakPointOnMemoryRange *bpr;
CDebugger debug;

class CDebuggerClient : public IDebuggerClient
{
public:
    virtual bool OnBreakPoint(IBreakPoint *pBreakPoint) 
    {
        static nCounter = 0;

        CSingleStepOverBreakPoint *SingleStep = dynamic_cast<CSingleStepOverBreakPoint *>(pBreakPoint);
        CBreakPointOnMemoryRange *MemRange = dynamic_cast<CBreakPointOnMemoryRange *>(pBreakPoint);
        CBreakPointOnExecution *Exec = dynamic_cast<CBreakPointOnExecution *>(pBreakPoint);

        if (pss && SingleStep == pss) {
            printf("SingleStep: %p\n", SingleStep->GetAddress());
            nCounter++;
            if (nCounter == 20) {
                SingleStep->Disable();
            }
            return(false);
        } else if (Exec == &bp) {
            HANDLE hThread = pBreakPoint->GetThread();
            printf("breakpoint reached (%p)\n", pBreakPoint->GetAddress());
            pBreakPoint->Disable();
            if (pBreakPoint->GetAddress() == m_lpStart) {
                printf("setting next breakpoint...\n");
                bp.SetAddress(reinterpret_cast<LPVOID>(0x4013D1));
                bp.Enable();
                pss = new CSingleStepOverBreakPoint(debug, hThread);
                debug.AddBreakPoint(pss);
                pss->Enable();
            } else {
                pss->Disable();
                bpr = new CBreakPointOnMemoryRange(debug, make_pair(reinterpret_cast<LPVOID>(0x405130), reinterpret_cast<LPVOID>(0x40513F)));
                debug.AddBreakPoint(bpr);
                bpr->Enable();
            }

            _CONTEXT ctx;
            ZeroMemory(&ctx, sizeof(_CONTEXT));
            ctx.ContextFlags = CONTEXT_INTEGER;
            GetThreadContext(hThread, &ctx);
            printf("Thread Id: %08lX, EAX: %08lX\n", debug.GetThreadId(hThread), ctx.Eax);
            return(false);
        } else if (bpr && MemRange == bpr) {
            printf("Memory range breakpoint (%p)\n", bpr->GetAddress());
            MemRange->Disable();
            nCounter = 0;
            pss->Enable();
        }
        return(false);
    }
    virtual bool OnException(EXCEPTION_DEBUG_INFO *pException) 
    {
        printf("exception (%08lX)!\n", pException->ExceptionRecord.ExceptionCode);
        return(true); // Processed
    }
    virtual void OnThreadCreated(CREATE_THREAD_DEBUG_INFO *pCreateThread) 
    {
        printf("thread created (%p)\n", pCreateThread->lpStartAddress);
    }
    virtual void OnThreadExit(EXIT_THREAD_DEBUG_INFO *pExitThread) 
    {
        printf("thread exit\n");
    }
    virtual void OnProcessCreated(CREATE_PROCESS_DEBUG_INFO *pCreateProcess) 
    {
        printf("process created (%p)\n", pCreateProcess->lpStartAddress);
        
        m_lpStart = pCreateProcess->lpStartAddress;
        bp.SetAddress(m_lpStart);
        debug.AddBreakPoint(&bp);
        bp.Enable();
    }
    virtual void OnProcessExit(EXIT_PROCESS_DEBUG_INFO *pExitProcess) 
    {
        printf("process exit\n");
        debug.Stop();
    }
    virtual void OnModuleLoaded(LOAD_DLL_DEBUG_INFO *pLoadDll) 
    {
        char szImageName[128];
        szImageName[0] = '\0';
        if (pLoadDll->lpImageName) {
            DWORD dwRead;
            LPVOID lpAddr;
            debug.ReadProcessMemory(pLoadDll->lpImageName, &lpAddr, 4, &dwRead);
            if (dwRead && lpAddr) 
                debug.GetCStringFromProcess(lpAddr, szImageName, pLoadDll->fUnicode == TRUE ? 64 : 128, pLoadDll->fUnicode);
        }
        if (szImageName[0]) {
            if (pLoadDll->fUnicode)
                printf("module loaded (%S)\n", szImageName);
            else
                printf("module loaded (%s)\n", szImageName);
        } else {
            printf("module loaded\n");
        }
    }
    virtual void OnModuleUnloaded(UNLOAD_DLL_DEBUG_INFO *pUnloadDll) 
    {
        printf("module unloaded\n");
    }
    virtual void OnDebugString(OUTPUT_DEBUG_STRING_INFO *pDebugString) 
    {
        printf("debug string\n");
    }
    virtual void SetDebuggedThread(DWORD dwPid, DWORD dwTid) 
    {
        m_dwPid = dwPid;
        m_dwTid = dwTid;
    }
    virtual void OnFinishedDebugging(void) 
    {
        printf("finished debugging\n");
    }

private:
    DWORD m_dwPid;
    DWORD m_dwTid;
    LPVOID m_lpStart;
};

int main(int argc, char * argv[])
{
    STARTUPINFO si;
    
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    
    DWORD dwPid = debug.LoadProcess("main.exe", "main.exe", NULL, NULL, &si);

    if (dwPid) {
        CDebuggerClient client;
        debug.AddClient(&client);
        debug.Go();
    }

    if (pss) {
        debug.RemoveBreakPoint(pss);
        delete pss;
    }

    if (bpr) {
        debug.RemoveBreakPoint(bpr);
        delete bpr;
    }

	return 0;
}

int main2(int argc, char *argv[])
{
    STARTUPINFO si;

    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    
    DWORD dwPid = debug.AttachToProcess(508);

    if (dwPid) {
        CDebuggerClient client;
        debug.AddClient(&client);
        debug.Go();
    }

    return 0;
}
*/