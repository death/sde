// Debugger.cpp - SDE Debugger implementation
#include "StdAfx.h"
#include "Debugger.h"

using namespace std;

namespace SDE
{

CDebugger::CDebugger()
: m_bProcessLoaded(false)
, m_bMultithread(false)
, m_lSuspendCount(0)
{
    // Create events
    for (int i = 0; i < cNumEvents; i++)
        m_hEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
}

CDebugger::~CDebugger()
{
    if (m_bProcessLoaded)
        Stop();

    // Remove events
    for (int n = 0; n < cNumEvents; n++) {
        if (m_hEvents[n])
            CloseHandle(m_hEvents[n]);
    }

    CloseProcessHandles();

    // Remove all breakpoints
    for_each(m_BreakPoints.begin(), m_BreakPoints.end(), mem_fun(IBreakPoint::Deinitialize));
}

DWORD CDebugger::LoadProcess(LPCSTR pszProcessName, LPCSTR pszArguments, LPCSTR pszCurrentDirectory, LPVOID lpEnvironment, LPSTARTUPINFO psi)
{
    if (m_bProcessLoaded == true)
        Stop();

    // Create the process
    BOOL bRet = CreateProcess(pszProcessName, const_cast<LPSTR>(pszArguments), NULL, NULL, FALSE, DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS, lpEnvironment, pszCurrentDirectory, psi, &m_pi);
    if (bRet == FALSE) {
        return(0);
    }

    // Set process-loaded flag
    m_bProcessLoaded = true;
    m_bActiveDebug = false;

    return(m_pi.dwProcessId);
}

bool CDebugger::AttachToProcess(DWORD dwPid)
{
    if (m_bProcessLoaded == true)
        Stop();

    if (DebugActiveProcess(dwPid) == FALSE) 
        return(false);

    m_bProcessLoaded = true;
    m_bActiveDebug = true;

    return(true);
}

void CDebugger::AddClient(IDebuggerClient *pClient)
{
    Suspend();
    m_vClients.push_front(pClient);
    Resume();
}

void CDebugger::RemoveClient(IDebuggerClient *pClient)
{
    Suspend();
    ClientVector::iterator i = find(m_vClients.begin(), m_vClients.end(), pClient);
    if (i != m_vClients.end()) {
        m_vClients.erase(i);
    }
    Resume();
}

bool CDebugger::AddBreakPoint(IBreakPoint *pBreakPoint)
{
    // A process must be loaded
    if (m_bProcessLoaded == false)
        return(false);

    Suspend();

    if (m_lProcessesCreated) {
        pBreakPoint->Initialize(m_pi.hProcess);
    }

    // Add breakpoint to vector
    m_BreakPoints.push_front(pBreakPoint);

    Resume();

    return(true);
}

bool CDebugger::RemoveBreakPoint(IBreakPoint *pBreakPoint)
{
    Suspend();

    BreakPointVector::iterator i = find(m_BreakPoints.begin(), m_BreakPoints.end(), pBreakPoint);
    if (i == m_BreakPoints.end()) {
        Resume();
        return(false);
    }

    pBreakPoint->Deinitialize();
    m_BreakPoints.erase(i);
    
    Resume();

    return(true);
}

int CDebugger::GetNumBreakPoints(void)
{
    Suspend();
    int nNumBreakPoints = static_cast<int>(m_BreakPoints.size());
    Resume();
    return(nNumBreakPoints);
}

bool CDebugger::EnumBreakPoints(EnumBreakPointProc EnumProc, void *pUserData)
{
    Suspend();
    BreakPointVector::const_iterator i;
    for (i = m_BreakPoints.begin(); i != m_BreakPoints.end(); ++i) {
        if (EnumProc(*i, pUserData) == false) {
            Resume();
            return(false);
        }
    }
    Resume();
    return(true);
}

bool CDebugger::Go(void)
{
    // A process must be loaded
    if (m_bProcessLoaded == false)
        return(false);

    CloseProcessHandles();

    // Clear stop event, set running event and begin debugger thread
    ResetEvent(m_hEvents[cDebugStopEvent]);
    SetEvent(m_hEvents[cDebugRunningEvent]);

    DebuggerThread(this);

    return(true);
}

void CDebugger::Stop(void)
{
    // A process must be loaded
    if (m_bProcessLoaded == false)
        return;

    // Signal stop event and wait until it becomes non-signaled again
    ResetEvent(m_hEvents[cDebugStoppedEvent]);
    SetEvent(m_hEvents[cDebugStopEvent]);
    
    if (m_bMultithread == true) 
        WaitForSingleObject(m_hEvents[cDebugStoppedEvent], INFINITE);

    m_lSuspendCount = 0;
}

void CDebugger::Suspend(void)
{
    // A process must be loaded
    if (m_bProcessLoaded == false)
        return;

    InterlockedIncrement(&m_lSuspendCount);

    CheckSuspend();
}

void CDebugger::Resume(void)
{
    // A process must be loaded
    if (m_bProcessLoaded == false)
        return;

    InterlockedDecrement(&m_lSuspendCount);

    CheckSuspend();
}

void CDebugger::DebuggerThread(void *pParam)
{
    CDebugger *pThis = reinterpret_cast<CDebugger *>(pParam);
    bool bFirstProcess = true;
    bool bFirstThread = true;
    
    pThis->m_lProcessesCreated = 0;

    // Main debugger loop
    while (1) {
        // Check stop event
        if (WaitForSingleObject(pThis->m_hEvents[cDebugStopEvent], 0) == WAIT_OBJECT_0)
            break;

        // Check running flag (for suspend/resume)
        WaitForSingleObject(pThis->m_hEvents[cDebugRunningEvent], INFINITE);

        // Wait for a debug event
        DEBUG_EVENT de;
        BOOL bRet = WaitForDebugEvent(&de, 1000);
        if (bRet == FALSE) {
            continue;
        }

        DWORD dwContinueStatus = DBG_CONTINUE;

        // Set debugged thread
        for (CDebugger::ClientVector::iterator i = pThis->m_vClients.begin(); i != pThis->m_vClients.end(); ++i) {
            IDebuggerClient *pClient = *i;
            if (pClient) {
                pClient->SetDebuggedThread(de.dwProcessId, de.dwThreadId);
            }
        }
        
        // Check event type
        switch (de.dwDebugEventCode) {
            case EXCEPTION_DEBUG_EVENT:
                // Exception
                {
                EXCEPTION_DEBUG_INFO *pException = &de.u.Exception;
                bool bNotifyBreakPoint = false;
                CDebugger::BreakPointVector TriggeredBreakPoints;

                // Scan through exceptions to see if we triggered one
                for (CDebugger::BreakPointVector::iterator i = pThis->m_BreakPoints.begin(); i != pThis->m_BreakPoints.end(); ++i) {
                    IBreakPoint *pBreakPoint = *i;
                    if (pBreakPoint) {
                        DWORD dwFirstChance = pException->dwFirstChance;
                        if (pBreakPoint->IsTriggered(pThis->m_Tid2Handle[de.dwThreadId], pException) == true) {
                            TriggeredBreakPoints.push_front(pBreakPoint);
                        }
                    }
                }

                // Notify client of breakpoint/exception
                if (TriggeredBreakPoints.empty() == false) {
                    // Notify each client of the triggered breakpoints
                    for (CDebugger::ClientVector::iterator i = pThis->m_vClients.begin(); i != pThis->m_vClients.end(); ++i) {
                        IDebuggerClient *pClient = *i;
                        if (pClient) {
                            for (CDebugger::BreakPointVector::iterator j = TriggeredBreakPoints.begin(); j != TriggeredBreakPoints.end(); ++j) {
                                IBreakPoint *pBreakPoint = *j;
                                if (pClient->OnBreakPoint(pBreakPoint) == true) {
                                    // Scan all triggered breakpoints, filter the ones already removed (by a breakpoint's IsTriggered)
                                    CDebugger::BPVIteratorVector v;
                                    for (CDebugger::BreakPointVector::iterator i = TriggeredBreakPoints.begin(); i != TriggeredBreakPoints.end(); ++i) {
                                        IBreakPoint *pBreakPoint = *i;
                                        if (find(pThis->m_BreakPoints.begin(), pThis->m_BreakPoints.end(), pBreakPoint) == pThis->m_BreakPoints.end()) {
                                            v.push_back(i);
                                        }
                                    }
                                    for (int i = 0; i < static_cast<int>(v.size()); i++) {
                                        TriggeredBreakPoints.erase(v[i]);
                                    }
                                    if (TriggeredBreakPoints.empty() == true)
                                        break;
                                }
                            }
                        }
                    }
                } else {
                    if (pException->dwFirstChance == 0xBADC0DE) break;

                    bool bRet = false;
                    for (CDebugger::ClientVector::iterator i = pThis->m_vClients.begin(); i != pThis->m_vClients.end(); ++i) {
                        IDebuggerClient *pClient = *i;
                        if (pClient) {
                            bRet |= pClient->OnException(pException);
                        }
                    }

                    if (bRet == false)
                        dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
                }
                }
                break;
            case CREATE_PROCESS_DEBUG_EVENT: 
                // A process was created
                {
                CREATE_PROCESS_DEBUG_INFO *pCreateProcess = &de.u.CreateProcessInfo;
                
                if (bFirstProcess == true) {
                    // Set process/thread handles
                    pThis->m_pi.hProcess = pCreateProcess->hProcess;
                    pThis->m_pi.hThread = pCreateProcess->hThread;
                    pThis->m_pi.dwProcessId = de.dwProcessId;
                    pThis->m_pi.dwThreadId = de.dwThreadId;

                    // Add TID/Handle to maps
                    if (pThis->m_pi.hThread && pThis->m_pi.dwThreadId) {
                        pThis->m_Tid2Handle[pThis->m_pi.dwThreadId] = pThis->m_pi.hThread;
                        pThis->m_Handle2Tid[pThis->m_pi.hThread] = pThis->m_pi.dwThreadId;
                    }

                    // Initialize all breakpoints
                    for (CDebugger::BreakPointVector::iterator i = pThis->m_BreakPoints.begin(); i != pThis->m_BreakPoints.end(); ++i) {
                        IBreakPoint *pBreakPoint = *i;
                        if (pBreakPoint) {
                            pBreakPoint->Initialize(pThis->m_pi.hProcess);
                        }
                    }
                    bFirstProcess = false;
                }

                InterlockedIncrement(&pThis->m_lProcessesCreated);

                for (CDebugger::ClientVector::iterator i = pThis->m_vClients.begin(); i != pThis->m_vClients.end(); ++i) {
                    IDebuggerClient *pClient = *i;
                    if (pClient) {
                        pClient->OnProcessCreated(pCreateProcess);
                    }
                }
                if (pCreateProcess->hFile)
                    CloseHandle(pCreateProcess->hFile);
                }
                break;
            case CREATE_THREAD_DEBUG_EVENT: 
                // A thread was created
                {
                CREATE_THREAD_DEBUG_INFO *pCreateThread = &de.u.CreateThread;

                if (bFirstThread) {
                    if (pThis->m_bActiveDebug == true) {
                        // Set as main thread
                        pThis->m_pi.hThread = pCreateThread->hThread;
                        pThis->m_pi.dwThreadId = de.dwThreadId;
                    }
                    bFirstThread = false;
                }

                // Add to TID/Handle maps
                if (pCreateThread->hThread) {
                    pThis->m_Tid2Handle[de.dwThreadId] = pCreateThread->hThread;
                    pThis->m_Handle2Tid[pCreateThread->hThread] = de.dwThreadId;
                }

                for (CDebugger::ClientVector::iterator i = pThis->m_vClients.begin(); i != pThis->m_vClients.end(); ++i) {
                    IDebuggerClient *pClient = *i;
                    if (pClient) {
                        pClient->OnThreadCreated(pCreateThread);
                    }
                }
                }
                break;
            case EXIT_THREAD_DEBUG_EVENT: 
                // A thread exit
                {
                EXIT_THREAD_DEBUG_INFO *pExitThread = &de.u.ExitThread;
                for (CDebugger::ClientVector::iterator i = pThis->m_vClients.begin(); i != pThis->m_vClients.end(); ++i) {
                    IDebuggerClient *pClient = *i;
                    if (pClient) {
                        pClient->OnThreadExit(pExitThread);
                    }
                }

                // Remove thread from maps
                pThis->m_Handle2Tid.erase(pThis->m_Tid2Handle[de.dwThreadId]);
                pThis->m_Tid2Handle.erase(de.dwThreadId);
                }
                break;
            case EXIT_PROCESS_DEBUG_EVENT: 
                // A process exit
                {
                EXIT_PROCESS_DEBUG_INFO *pExitProcess = &de.u.ExitProcess;
                InterlockedDecrement(&pThis->m_lProcessesCreated);
                for (CDebugger::ClientVector::iterator i = pThis->m_vClients.begin(); i != pThis->m_vClients.end(); ++i) {
                    IDebuggerClient *pClient = *i;
                    if (pClient) {
                        pClient->OnProcessExit(pExitProcess);
                    }
                }

                // Remove thread from maps
                pThis->m_Handle2Tid.erase(pThis->m_Tid2Handle[de.dwThreadId]);
                pThis->m_Tid2Handle.erase(de.dwThreadId);
                }
                break;
            case LOAD_DLL_DEBUG_EVENT: 
                // A module was loaded
                {
                LOAD_DLL_DEBUG_INFO *pLoadDll = &de.u.LoadDll;
                for (CDebugger::ClientVector::iterator i = pThis->m_vClients.begin(); i != pThis->m_vClients.end(); ++i) {
                    IDebuggerClient *pClient = *i;
                    if (pClient) {
                        pClient->OnModuleLoaded(pLoadDll);
                    }
                }
                }
                break;
            case UNLOAD_DLL_DEBUG_EVENT: 
                // A module was unloaded
                {
                UNLOAD_DLL_DEBUG_INFO *pUnloadDll = &de.u.UnloadDll;
                for (CDebugger::ClientVector::iterator i = pThis->m_vClients.begin(); i != pThis->m_vClients.end(); ++i) {
                    IDebuggerClient *pClient = *i;
                    if (pClient) {
                        pClient->OnModuleUnloaded(pUnloadDll);
                    }
                }
                }
                break;
            case OUTPUT_DEBUG_STRING_EVENT: 
                // A debug string was received
                {
                OUTPUT_DEBUG_STRING_INFO *pDebugString = &de.u.DebugString;
                for (CDebugger::ClientVector::iterator i = pThis->m_vClients.begin(); i != pThis->m_vClients.end(); ++i) {
                    IDebuggerClient *pClient = *i;
                    if (pClient) {
                        pClient->OnDebugString(pDebugString);
                    }
                }
                }
                break;
            default:
                break;
        }

        // Continue debugging
        if (ContinueDebugEvent(de.dwProcessId, de.dwThreadId, dwContinueStatus) == 0)
            break;
    }

    // Clear process loaded flag
    pThis->m_bProcessLoaded = false;

    // Null thread/process handle
    pThis->m_pi.hProcess = NULL;
    pThis->m_pi.hThread = NULL;

    // Clear maps
    pThis->m_Tid2Handle.clear();
    pThis->m_Handle2Tid.clear();

    // Set stopped event
    SetEvent(pThis->m_hEvents[cDebugStoppedEvent]);

    // Call client's finish notifciation method
    for (CDebugger::ClientVector::iterator i = pThis->m_vClients.begin(); i != pThis->m_vClients.end(); ++i) {
        IDebuggerClient *pClient = *i;
        if (pClient) {
            pClient->OnFinishedDebugging();
        }
    }
}

void CDebugger::CloseProcessHandles(void)
{
    // Close thread/process handles
    if (m_pi.hThread)
        CloseHandle(m_pi.hThread);
    if (m_pi.hProcess)
        CloseHandle(m_pi.hProcess);
    
    m_pi.hThread = m_pi.hProcess = NULL;
}

BOOL CDebugger::ReadProcessMemory(LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T *lpNumberOfBytesRead)
{
    return(::ReadProcessMemory(m_pi.hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead));
}

BOOL CDebugger::WriteProcessMemory(LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T *lpNumberOfBytesWritten)
{
    return(::WriteProcessMemory(m_pi.hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesWritten));
}

int CDebugger::GetCStringFromProcess(LPCVOID lpBaseAddress, LPSTR lpString, SIZE_T nMaxString, BOOL bUnicode)
{
    SIZE_T nCur = 0;
    int nCharSize = (bUnicode == TRUE) ? sizeof(wchar_t) : sizeof(char);

    if (nMaxString) {
        lpString[nMaxString - 1] = '\0';
        if (bUnicode)
            lpString[nMaxString - 2] = '\0';
    }

    while (nCur < (nMaxString - 1)) {
        DWORD dw = 0;
        ReadProcessMemory(lpBaseAddress, &lpString[nCur * nCharSize], nCharSize, &dw);
        if (dw == 0) break;
        if (memcmp(&lpString[nCur * nCharSize], "\0\0", nCharSize) == 0) break;
        nCur++;
        lpBaseAddress = reinterpret_cast<LPCSTR>(lpBaseAddress) + nCharSize;
    }

    return(static_cast<int>(nCur));
}

DWORD CDebugger::GetThreadId(HANDLE hThread) const
{
    Handle2Tid::const_iterator i = m_Handle2Tid.find(hThread);
    if (i == m_Handle2Tid.end()) return(0);
    return(i->second);
}

HANDLE CDebugger::GetThreadHandle(DWORD dwTid) const
{
    Tid2Handle::const_iterator i = m_Tid2Handle.find(dwTid);
    if (i == m_Tid2Handle.end()) return(NULL);
    return(i->second);
}

void CDebugger::SetMultithread(bool bMultithread)
{
    m_bMultithread = bMultithread;
}

void CDebugger::CheckSuspend(void)
{
    if (m_lSuspendCount > 0) {
        ResetEvent(m_hEvents[cDebugRunningEvent]);
    } else {
        SetEvent(m_hEvents[cDebugRunningEvent]);
        m_lSuspendCount = 0;
    }
}

}
