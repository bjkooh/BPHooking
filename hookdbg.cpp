


#include "windows.h"
#include "stdio.h"

LPVOID g_pfWriteFile = NULL;
CREATE_PROCESS_DEBUG_INFO g_cpdi;
BYTE g_chINT3 = 0xCC, g_chOrgByte = 0;

BOOL OnCreateProcessDebugEvent(LPDEBUG_EVENT pde)
{

    g_pfWriteFile = GetProcAddress(GetModuleHandleA("kernel32.dll"), "WriteFile");

    memcpy(&g_cpdi, &pde->u.CreateProcessInfo, sizeof(CREATE_PROCESS_DEBUG_INFO));
    bool test1=ReadProcessMemory(g_cpdi.hProcess, g_pfWriteFile, &g_chOrgByte, sizeof(BYTE), NULL);
    bool test2=WriteProcessMemory(g_cpdi.hProcess, g_pfWriteFile, &g_chINT3, sizeof(BYTE), NULL);

    return TRUE;
}

BOOL OnExceptionDebugEvent(LPDEBUG_EVENT pde)
{
    CONTEXT ctx;
    PBYTE lpBuffer = NULL;
    DWORD dwNumOfBytesToWrite, dwAddrOfBuffer, i;
    PEXCEPTION_RECORD per = &pde->u.Exception.ExceptionRecord;

    if( EXCEPTION_BREAKPOINT == per->ExceptionCode )
    {

        if( g_pfWriteFile == per->ExceptionAddress )
        {
          
            WriteProcessMemory(g_cpdi.hProcess, g_pfWriteFile, &g_chOrgByte, sizeof(BYTE), NULL);

            ctx.ContextFlags = CONTEXT_CONTROL;

            GetThreadContext(g_cpdi.hThread, &ctx);

            ReadProcessMemory(g_cpdi.hProcess, (LPVOID)(ctx.Esp+0x8), &dwAddrOfBuffer, sizeof(DWORD), NULL);
            ReadProcessMemory(g_cpdi.hProcess, (LPVOID)(ctx.Esp+0xC), &dwNumOfBytesToWrite, sizeof(DWORD), NULL);


            lpBuffer = (PBYTE)malloc(dwNumOfBytesToWrite+1);
            memset(lpBuffer, 0, dwNumOfBytesToWrite+1);


            ReadProcessMemory(g_cpdi.hProcess, (LPVOID)dwAddrOfBuffer,lpBuffer, dwNumOfBytesToWrite, NULL);

            for( i = 0; i < dwNumOfBytesToWrite; i++ )
            {
                if( 0x61 <= lpBuffer[i] && lpBuffer[i] <= 0x7A )
                    lpBuffer[i] -= 0x20;
            }

            WriteProcessMemory(g_cpdi.hProcess, (LPVOID)dwAddrOfBuffer,lpBuffer, dwNumOfBytesToWrite, NULL);
            
            free(lpBuffer);

            ctx.Eip = (DWORD)g_pfWriteFile;
            SetThreadContext(g_cpdi.hThread, &ctx);

            ContinueDebugEvent(pde->dwProcessId, pde->dwThreadId, DBG_CONTINUE);
            Sleep(0);

            WriteProcessMemory(g_cpdi.hProcess, g_pfWriteFile,&g_chINT3, sizeof(BYTE), NULL);

            return TRUE;
        }
    }

    return FALSE;
}

void DebugLoop()
{
    DEBUG_EVENT de;
    DWORD dwContinue;

    while( WaitForDebugEvent(&de, INFINITE) )
    {
        dwContinue = DBG_CONTINUE;

  
        if( CREATE_PROCESS_DEBUG_EVENT == de.dwDebugEventCode )
        {
            OnCreateProcessDebugEvent(&de);
        }

        else if( EXCEPTION_DEBUG_EVENT == de.dwDebugEventCode )
        {
            if( OnExceptionDebugEvent(&de) )
                continue;
        }
    
        else if( EXIT_PROCESS_DEBUG_EVENT == de.dwDebugEventCode )
        {
            break;
        }

        ContinueDebugEvent(de.dwProcessId, de.dwThreadId, dwContinue);
    }
}

int main(int argc, char* argv[])
{
    DWORD dwPID;
	
    if( argc != 2 )
    {
        printf("\nUSAGE : hookdbg.exe <pid>\n");
        return 1;
    }

    dwPID = atoi(argv[1]);
    if( !DebugActiveProcess(dwPID) )
    {
		printf("DebugActiveProcess(%d) ����!!!(%d)\n", dwPID, GetLastError());
        return 1;
    }


    DebugLoop();

    return 0;
}
