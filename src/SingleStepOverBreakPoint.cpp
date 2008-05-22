// SingleStepBreakPoint.h - Single step breakpoint implementation
#include "StdAfx.h"
#include "SingleStepOverBreakPoint.h"
#include "InterruptFlagSet.h"

#pragma warning(disable:4311) // 'reinterpret_cast' : pointer truncation from 'PVOID' to 'DWORD'
#pragma warning(disable:4312) // 'reinterpret_cast' : conversion from 'DWORD' to 'LPVOID' of greater size

namespace SDE
{

CSingleStepOverBreakPoint::CSingleStepOverBreakPoint(IDebugger & debugger, HANDLE hThread)
: m_hProcess(NULL)
, m_bIsInitialized(false)
, m_bIsEnabled(false)
, m_hThread(hThread)
, m_pBpOnExec(NULL)
, m_debugger(debugger)
, m_bShouldFree(false)
, m_bWaitForBreakPoint(false)
{
}

CSingleStepOverBreakPoint::~CSingleStepOverBreakPoint()
{
    if (m_bIsInitialized == true)
        Deinitialize();

    if (m_pBpOnExec) {
        delete m_pBpOnExec;
    }
}

bool CSingleStepOverBreakPoint::Initialize(HANDLE hProcess)
{
    m_hProcess = hProcess;
    m_bIsInitialized = true;
    return(true);
}

bool CSingleStepOverBreakPoint::Enable(void)
{
    // Must be initialized
    if (m_bIsInitialized == false)
        return(false);

    // Is already enabled?
    if (m_bIsEnabled == true)
        return(false);

    // Set IF
    if (CInterruptFlagSet::SetInterruptFlag(m_hThread) == false)
        return(false);

    m_bIsEnabled = true;

    return(true);
}

bool CSingleStepOverBreakPoint::Disable(void)
{
    // Must be initialized
    if (m_bIsInitialized == false)
        return(false);

    // Is already disabled?
    if (m_bIsEnabled == false)
        return(false);

    // Clear IF
    if (CInterruptFlagSet::ClearInterruptFlag(m_hThread) == false)
        return(false);

    m_bIsEnabled = false;

    return(true);
}

void CSingleStepOverBreakPoint::Deinitialize(void)
{
    m_bIsInitialized = false;
}

bool CSingleStepOverBreakPoint::IsTriggered(HANDLE hThread, EXCEPTION_DEBUG_INFO *pException)
{
    // Must be initialized and enabled
    if (m_bIsInitialized == false || m_bIsEnabled == false || m_bWaitForBreakPoint == true)
        return(false);

    // Check thread
    if (hThread != m_hThread)
        return(false);

    // Must be a first-chance exception
    if (pException->dwFirstChance == 0)
        return(false);

    // Exception must be of EXCEPTION_SINGLE_STEP type
    if (pException->ExceptionRecord.ExceptionCode != EXCEPTION_SINGLE_STEP)
        return(false);

    m_lpVA = pException->ExceptionRecord.ExceptionAddress;
    
    // Check if we need to write the stack value back (from our exec-breakpoint handler)
    if (m_bShouldFree == true) {
        VirtualFreeEx(m_hProcess, m_lpMem, 5, MEM_RELEASE);
        m_bShouldFree = false;
    }
    
    // Get first byte of instruction
    BYTE byte;
    DWORD dwNumRead = 0;
    ReadProcessMemory(m_hProcess, m_lpVA, &byte, 1, &dwNumRead);

    int nInstructionSize = 0;

    if (dwNumRead) {
        switch (byte) {
            case 0xCC: // INT 3
                nInstructionSize = 1;
                break;
            case 0xCD: // INT xx
                nInstructionSize = 2;
                break;
            case 0xE8: // CALL Dword [mem]
                nInstructionSize = 5;
                break;
            case 0x9A: // CALL Fword [mem]
                nInstructionSize = 7;
                break;
            case 0xFF:
                {
                dwNumRead = 0;
                ReadProcessMemory(m_hProcess, reinterpret_cast<LPVOID>(reinterpret_cast<ULONG_PTR>(m_lpVA) + 1), &byte, 1, &dwNumRead);
                if (dwNumRead) {
                    if (byte == 0x15) {
                        // CALL Dword
                        nInstructionSize = 6;
                    } else if ((byte & 0xD8) == 0xD0) {
                        // CALL reg
                        nInstructionSize = 2;
                    } else if ((byte & 0x90) == 0x90) {
                        // CALL d,[reg][mem] / CALL f,[reg][mem]
                        nInstructionSize = 6;
                    } else if ((byte & 0x50) == 0x50) {
                        // CALL d,[reg][pos] / CALL f,[reg][pos]
                        nInstructionSize = 3;
                    } else if ((byte & 0x10) == 0x10) {
                        // CALL d,[reg] / CALL f,[reg]
                        nInstructionSize = 2;
                    }
                }
                }
                break;
            default:
                break;
        }
    }

    if (nInstructionSize) {
        // Set breakpoint after the instruction and continue single stepping from there
        ULONG_PTR ulpVA = reinterpret_cast<ULONG_PTR>(m_lpVA);
        ulpVA += nInstructionSize;
        
        if (m_pBpOnExec) {
            delete m_pBpOnExec;
            m_pBpOnExec = 0;
        }
        
        m_pBpOnExec = new CBreakPointOnExecution(reinterpret_cast<LPVOID>(ulpVA));
        m_debugger.AddBreakPoint(m_pBpOnExec);
        m_pBpOnExec->Enable();

        m_debugger.AddClient(this);
 
        m_bWaitForBreakPoint = true;
        
        CInterruptFlagSet::ClearInterruptFlag(m_hThread);
    } else {
        CInterruptFlagSet::SetInterruptFlag(m_hThread);
    }

    return(true);
}

LPVOID CSingleStepOverBreakPoint::GetAddress(void) const
{
    return(m_lpVA);
}

HANDLE CSingleStepOverBreakPoint::GetThread(void) const
{
    return(m_hThread);
}

HANDLE CSingleStepOverBreakPoint::GetProcess(void) const
{
    return(m_hProcess);
}

bool CSingleStepOverBreakPoint::OnBreakPoint(IBreakPoint *pBreakPoint)
{
    CBreakPointOnExecution *pBp = dynamic_cast<CBreakPointOnExecution *>(pBreakPoint);

    if (pBp == m_pBpOnExec) {
        DWORD dwAddress = reinterpret_cast<DWORD>(pBreakPoint->GetAddress());
        m_pBpOnExec->Disable();
        m_debugger.RemoveBreakPoint(m_pBpOnExec);
        delete m_pBpOnExec;
        m_pBpOnExec = 0;

        m_debugger.RemoveClient(this);
        
        // Allocate some space for a JMP <our address>
        m_lpMem = VirtualAllocEx(m_hProcess, 0, 5, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (m_lpMem) {
            m_bShouldFree = true;

            // Write JMP code
            BYTE Jmp[5];
            Jmp[0] = 0xE9;
            *((DWORD *)&Jmp[1]) = dwAddress;
            *((DWORD *)&Jmp[1]) -= reinterpret_cast<DWORD>(m_lpMem);
            *((DWORD *)&Jmp[1]) -= 5;
            DWORD dw;
            WriteProcessMemory(m_hProcess, m_lpMem, Jmp, 5, &dw);

            // Set EIP to the JMP code
            _CONTEXT ctx;
            ZeroMemory(&ctx, sizeof(_CONTEXT));
            ctx.ContextFlags = CONTEXT_CONTROL;
            GetThreadContext(m_hThread, &ctx);
            ctx.Eip = reinterpret_cast<DWORD>(m_lpMem);
            SetThreadContext(m_hThread, &ctx);
        }

        CInterruptFlagSet::SetInterruptFlag(m_hThread);

        m_bWaitForBreakPoint = false;

        return(true); // We removed a breakpoint
    }

    return(false); // We didn't remove any breakpoints
}

void CSingleStepOverBreakPoint::OnFinishedDebugging(void)
{
    if (m_pBpOnExec) {
        m_pBpOnExec->Disable();
        m_debugger.RemoveBreakPoint(m_pBpOnExec);
        delete m_pBpOnExec;
        m_pBpOnExec = 0;
    }

    m_debugger.RemoveClient(this);
}

}