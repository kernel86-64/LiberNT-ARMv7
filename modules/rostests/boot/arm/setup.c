/*
 * PROJECT:     LiberNT boot tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Provision and verify the local QEMU test account
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */
#define WIN32_NO_STATUS
#include <windows.h>
#include <lm.h>
#include <ntsecapi.h>
#include <uxtheme.h>
#include <winspool.h>
#include <ndk/rtlfuncs.h>
#include <ndk/kefuncs.h>

typedef struct _ARM_APP_WINDOW
{
    DWORD Process;
    HWND Window;
    WCHAR Title[256];
} ARM_APP_WINDOW;

typedef struct _ARM_TIMER_WORKER
{
    HANDLE Start;
    volatile LONG *Stop;
    volatile LONG Count;
} ARM_TIMER_WORKER;

static DWORD WINAPI TimerWorker(PVOID Parameter)
{
    ARM_TIMER_WORKER *Worker = Parameter;
    WaitForSingleObject(Worker->Start, INFINITE);
    while (!InterlockedCompareExchange(Worker->Stop, 0, 0))
        InterlockedIncrement(&Worker->Count);
    return 0;
}

static BOOL VerifyTimerPreemption(VOID)
{
    volatile LONG Stop = 0;
    ARM_TIMER_WORKER Workers[2] = {0};
    HANDLE Start = CreateEventW(NULL, TRUE, FALSE, NULL), Threads[2];
    ULONG Resolution;
    DWORD Count = 0, Index;
    BOOL Requested = FALSE, Passed = FALSE;

    if (!Start) return FALSE;
    for (Index = 0; Index < ARRAYSIZE(Workers); ++Index)
    {
        Workers[Index].Start = Start;
        Workers[Index].Stop = &Stop;
        Threads[Index] = CreateThread(NULL, 0, TimerWorker, &Workers[Index], 0, NULL);
        if (!Threads[Index]) break;
        ++Count;
    }
    if (Count == ARRAYSIZE(Workers))
    {
        Requested = NtSetTimerResolution(10000, TRUE, &Resolution) >= 0;
        if (Requested)
        {
            SetEvent(Start);
            Sleep(500);
            Passed = InterlockedCompareExchange(&Workers[0].Count, 0, 0) > 0 &&
                     InterlockedCompareExchange(&Workers[1].Count, 0, 0) > 0;
        }
    }
    InterlockedExchange(&Stop, 1);
    SetEvent(Start);
    if (Count && WaitForMultipleObjects(Count, Threads, TRUE, 5000) != WAIT_OBJECT_0)
    {
        DbgPrint("ARM timer preemption: worker shutdown failed\n");
        ExitProcess(1);
    }
    if (Requested) NtSetTimerResolution(10000, FALSE, &Resolution);
    for (Index = 0; Index < Count; ++Index) CloseHandle(Threads[Index]);
    CloseHandle(Start);
    DbgPrint("ARM timer preemption: %s (%ld, %ld)\n", Passed ? "PASS" : "FAIL", Workers[0].Count, Workers[1].Count);
    return Passed;
}

static VOID CALLBACK VerifyApc(ULONG_PTR Parameter)
{
    ++*(volatile ULONG *)Parameter;
}

static BOOL VerifyApcWait(VOID)
{
    volatile ULONG Delivered = 0;
    HANDLE Event = CreateEventW(NULL, TRUE, FALSE, NULL);
    BOOL Passed;

    if (!Event) return FALSE;
    Passed = QueueUserAPC(VerifyApc, GetCurrentThread(), (ULONG_PTR)&Delivered) &&
             SleepEx(0, TRUE) == WAIT_IO_COMPLETION && Delivered == 1 &&
             WaitForSingleObject(Event, 0) == WAIT_TIMEOUT;
    CloseHandle(Event);
    DbgPrint("ARM user APC wait: %s\n", Passed ? "PASS" : "FAIL");
    return Passed;
}

static BOOL VerifyUserMapping(VOID)
{
    WCHAR Name[32];
    HANDLE Section = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL);
    PVOID View;
    BOOL Passed;

    if (!Section) return FALSE;
    View = MapViewOfFile(Section, FILE_MAP_WRITE, 0, 0, 4096);
    Passed = (ULONG_PTR)View >= 0x10000 &&
             MultiByteToWideChar(CP_ACP, 0, "kernelbase.dll", -1, Name, ARRAYSIZE(Name)) &&
             !wcscmp(Name, L"kernelbase.dll");
    if (View) UnmapViewOfFile(View);
    CloseHandle(Section);
    DbgPrint("ARM user mapping and NLS: %s\n", Passed ? "PASS" : "FAIL");
    return Passed;
}

static BOOL VerifyPrintEnvironment(VOID)
{
    BYTE Buffer[1024];
    DWORD Needed, Count, Index;
    PPRINTPROCESSOR_INFO_1W Processors = (PVOID)Buffer;

    if (!EnumPrintProcessorsW(NULL, L"Windows ARM", 1, Buffer, sizeof(Buffer), &Needed, &Count))
    {
        DbgPrint("ARM print environment: failed %lu\n", GetLastError());
        return FALSE;
    }
    for (Index = 0; Index < Count; ++Index)
        if (!_wcsicmp(Processors[Index].pName, L"winprint"))
        {
            DbgPrint("ARM print environment: PASS (winprint)\n");
            return TRUE;
        }
    return FALSE;
}

static BOOL VerifyFinalPath(VOID)
{
    WCHAR Module[MAX_PATH], Path[MAX_PATH + 4];
    HANDLE File;
    DWORD Length;
    BOOL Passed;

    if (!GetModuleFileNameW(NULL, Module, ARRAYSIZE(Module))) return FALSE;
    File = CreateFileW(Module, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, 0, NULL);
    if (File == INVALID_HANDLE_VALUE) return FALSE;
    Length = GetFinalPathNameByHandleW(File, Path, ARRAYSIZE(Path), VOLUME_NAME_DOS | FILE_NAME_OPENED);
    Passed = Length > 4 && Length < ARRAYSIZE(Path) &&
             !wcsncmp(Path, L"\\\\?\\", 4) && !_wcsicmp(Path + 4, Module);
    DbgPrint("ARM final DOS path: %s (length %lu, error %lu)\n", Passed ? "PASS" : "FAIL", Length, GetLastError());
    CloseHandle(File);
    return Passed;
}

static BOOL CALLBACK FindApplicationWindow(HWND Window, LPARAM Parameter)
{
    ARM_APP_WINDOW *Application = (ARM_APP_WINDOW *)Parameter;
    DWORD Process;
    GetWindowThreadProcessId(Window, &Process);
    if (Process != Application->Process || !IsWindowVisible(Window) ||
        !GetWindowTextW(Window, Application->Title, ARRAYSIZE(Application->Title)))
        return TRUE;
    Application->Window = Window;
    return FALSE;
}

static int DesktopSmoke(VOID)
{
    static const WCHAR *Applications[] = {
        L"winver.exe", L"rundll32.exe shell32.dll,Control_RunDLL sysdm.cpl",
        L"notepad.exe", L"calc.exe", L"regedit.exe", L"taskmgr.exe"};
    WCHAR Theme[MAX_PATH], Command[256];
    ULONG Index, Attempt;
    STARTUPINFOW Startup;
    PROCESS_INFORMATION Process;
    ARM_APP_WINDOW Window;
    DWORD_PTR Result;
    HMODULE Native = GetModuleHandleW(L"ntdll.dll"), Kernel = GetModuleHandleW(L"kernel32.dll");
    PIMAGE_NT_HEADERS NativeHeaders = RtlImageNtHeader(Native), KernelHeaders = RtlImageNtHeader(Kernel);

    if (!VerifyTimerPreemption() || !VerifyApcWait() || !VerifyUserMapping() || !VerifyPrintEnvironment()) return 1;
    if (!NativeHeaders || !KernelHeaders) return 1;
    if (NativeHeaders->OptionalHeader.ImageBase == KernelHeaders->OptionalHeader.ImageBase)
    {
        if ((ULONG_PTR)Kernel == KernelHeaders->OptionalHeader.ImageBase) return 1;
        DbgPrint("ARM KnownDLL relocation: PASS (kernel32 preferred %p, mapped %p)\n",
                 (PVOID)(ULONG_PTR)KernelHeaders->OptionalHeader.ImageBase, Kernel);
    }
    if (!IsThemeActive() || FAILED(GetCurrentThemeName(Theme, ARRAYSIZE(Theme), NULL, 0, NULL, 0)))
    {
        DbgPrint("ARM desktop smoke: active theme missing\n");
        return 1;
    }
    DbgPrint("ARM desktop smoke: active theme %S\n", Theme);
    for (Index = 0; Index < ARRAYSIZE(Applications); ++Index)
    {
        ZeroMemory(&Startup, sizeof(Startup));
        Startup.cb = sizeof(Startup);
        wcscpy(Command, Applications[Index]);
        if (!CreateProcessW(NULL, Command, NULL, NULL, FALSE, 0, NULL, NULL, &Startup, &Process))
        {
            DbgPrint("ARM desktop smoke: %S launch failed %lu\n", Applications[Index], GetLastError());
            return 1;
        }
        ZeroMemory(&Window, sizeof(Window));
        Window.Process = Process.dwProcessId;
        WaitForInputIdle(Process.hProcess, 3000);
        for (Attempt = 0; Attempt < 60 && !Window.Window; ++Attempt)
        {
            EnumWindows(FindApplicationWindow, (LPARAM)&Window);
            if (!Window.Window) Sleep(100);
        }
        CloseHandle(Process.hThread);
        CloseHandle(Process.hProcess);
        if (!Window.Window)
        {
            DbgPrint("ARM desktop smoke: %S visible window missing\n", Applications[Index]);
            return 1;
        }
        DbgPrint("ARM desktop smoke: %S window '%S' verified\n", Applications[Index], Window.Title);
        SendMessageTimeoutW(Window.Window, WM_CLOSE, 0, 0, SMTO_ABORTIFHUNG, 1000, &Result);
    }
    DbgPrint("ARM desktop smoke: PASS (theme, winver, system properties, Notepad, Calculator, Registry Editor, Task Manager)\n");
    return 0;
}

static BOOL WaitForInterfaceWindow(PCWSTR Class, PCSTR Marker)
{
    ULONG Attempt;
    HWND Window;
    DWORD_PTR Reply;

    for (Attempt = 0; Attempt < 300; ++Attempt)
    {
        Window = FindWindowW(Class, NULL);
        if (Window && IsWindowVisible(Window) &&
            SendMessageTimeoutW(Window, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 1000, &Reply))
        {
            DbgPrint("ARM UI: %s responsive\n", Marker);
            return TRUE;
        }
        Sleep(100);
    }
    DbgPrint("ARM UI: %s missing or unresponsive\n", Marker);
    return FALSE;
}

static int VerifySession(BOOL Interface)
{
    WCHAR Name[256], Profile[MAX_PATH];
    DWORD Length = ARRAYSIZE(Name);
    ULONG Attempt;
    HKEY Probe;
    HWND Desktop, Tray;
    DWORD_PTR Reply;
    LONG Error;
    if (!GetUserNameW(Name, &Length) || wcscmp(Name, L"tester"))
    {
        DbgPrint("ARM user session: unexpected user or GetUserName failure %lu\n", GetLastError());
        return 1;
    }
    Length = GetEnvironmentVariableW(L"USERPROFILE", Profile, ARRAYSIZE(Profile));
    if (!Length || Length >= ARRAYSIZE(Profile) || _wcsicmp(Profile, L"C:\\Users\\tester"))
    {
        DbgPrint("ARM user session: unexpected profile %S\n", Length ? Profile : L"(none)");
        return 1;
    }
    Error = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\LiberNT\\ARMTest", 0, NULL, 0,
                            KEY_SET_VALUE, NULL, &Probe, NULL);
    if (Error)
    {
        DbgPrint("ARM user session: profile registry write failed %ld\n", Error);
        return 1;
    }
    RegCloseKey(Probe);
    DbgPrint("ARM user session: user and profile registry verified\n");
    for (Attempt = 0; Attempt < 300; ++Attempt)
    {
        Tray = FindWindowW(L"Shell_TrayWnd", NULL);
        Desktop = FindWindowW(L"Progman", NULL);
        if (Tray && Desktop && IsWindowVisible(Tray) && IsWindowVisible(Desktop) &&
            SendMessageTimeoutW(Tray, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 1000, &Reply) &&
            SendMessageTimeoutW(Desktop, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 1000, &Reply))
        {
            DbgPrint("ARM user session: PASS (tester, profile, Explorer desktop and taskbar)\n");
            if (Interface && (!WaitForInterfaceWindow(L"StartMenu2", "Start menu") ||
                              !WaitForInterfaceWindow(L"CabinetWClass", "Explorer window")))
                return 1;
            return 0;
        }
        Sleep(100);
    }
    DbgPrint("ARM user session: Explorer desktop or taskbar missing\n");
    return 1;
}

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE Previous, LPWSTR Command, int Show)
{
    USER_INFO_1 User = {0};
    LSA_OBJECT_ATTRIBUTES Attributes = {0};
    LSA_HANDLE Policy;
    LSA_UNICODE_STRING Right;
    DECLSPEC_ALIGN(8) BYTE WorldSid[SECURITY_MAX_SID_SIZE];
    DWORD WorldLength = sizeof(WorldSid);
    WCHAR Domain[256];
    DECLSPEC_ALIGN(8) BYTE Sid[SECURITY_MAX_SID_SIZE];
    DWORD SidLength = sizeof(Sid), DomainLength = ARRAYSIZE(Domain), Parameter = 0, Zero = 0;
    SID_NAME_USE Use;
    NET_API_STATUS Error;
    NTSTATUS Status;
    HANDLE Token;
    HKEY Setup;
    DECLSPEC_ALIGN(8) BYTE TokenBuffer[512];
    DWORD TokenLength;
    UNREFERENCED_PARAMETER(Instance);
    UNREFERENCED_PARAMETER(Previous);
    if (!wcscmp(Command, L"--desktop-smoke")) return DesktopSmoke();
    if (!wcscmp(Command, L"--verify-path")) return VerifyFinalPath() ? 0 : 1;
    if (!wcscmp(Command, L"--verify-session")) return VerifySession(FALSE);
    if (!wcscmp(Command, L"--verify-interface")) return VerifySession(TRUE);
    UNREFERENCED_PARAMETER(Show);

    DbgPrint("ARM account setup: starting\n");
    User.usri1_name = L"tester";
    User.usri1_password = L"armv7";
    User.usri1_priv = USER_PRIV_USER;
    User.usri1_flags = UF_SCRIPT | UF_NORMAL_ACCOUNT | UF_DONT_EXPIRE_PASSWD;
    Error = NetUserAdd(NULL, 1, (PBYTE)&User, &Parameter);
    if (Error != NERR_Success)
    {
        DbgPrint("ARM account setup: NetUserAdd failed %lu parameter %lu\n", Error, Parameter);
        return 1;
    }
    if (!LookupAccountNameW(NULL, L"tester", Sid, &SidLength, Domain, &DomainLength, &Use))
    {
        DbgPrint("ARM account setup: LookupAccountName failed %lu\n", GetLastError());
        return 1;
    }
    Attributes.Length = sizeof(Attributes);
    Status = LsaOpenPolicy(NULL, &Attributes, POLICY_CREATE_ACCOUNT | POLICY_LOOKUP_NAMES, &Policy);
    if (Status < 0)
    {
        DbgPrint("ARM account setup: LsaOpenPolicy failed %08lx\n", Status);
        return 1;
    }
    RtlInitUnicodeString(&Right, L"SeInteractiveLogonRight");
    Status = LsaAddAccountRights(Policy, Sid, &Right, 1);
    if (Status >= 0)
    {
        if (!CreateWellKnownSid(WinWorldSid, NULL, WorldSid, &WorldLength))
        {
            LsaClose(Policy);
            DbgPrint("ARM account setup: world SID failed %lu\n", GetLastError());
            return 1;
        }
        RtlInitUnicodeString(&Right, L"SeChangeNotifyPrivilege");
        Status = LsaAddAccountRights(Policy, WorldSid, &Right, 1);
    }
    LsaClose(Policy);
    if (Status < 0)
    {
        DbgPrint("ARM account setup: LsaAddAccountRights failed %08lx\n", Status);
        return 1;
    }
    if (LogonUserW(L"tester", L".", L"incorrect", LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &Token))
    {
        CloseHandle(Token);
        DbgPrint("ARM account setup: incorrect password accepted\n");
        return 1;
    }
    if (GetLastError() != ERROR_LOGON_FAILURE)
    {
        DbgPrint("ARM account setup: incorrect password returned %lu\n", GetLastError());
        return 1;
    }
    if (!LogonUserW(L"tester", L".", L"armv7", LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &Token))
    {
        DbgPrint("ARM account setup: LogonUser failed %lu\n", GetLastError());
        return 1;
    }
    if (!GetTokenInformation(Token, TokenUser, TokenBuffer, sizeof(TokenBuffer), &TokenLength) ||
        !EqualSid(Sid, ((PTOKEN_USER)TokenBuffer)->User.Sid))
    {
        CloseHandle(Token);
        DbgPrint("ARM account setup: interactive token user mismatch\n");
        return 1;
    }
    CloseHandle(Token);
    Error = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\Setup", 0, KEY_SET_VALUE, &Setup);
    if (!Error)
    {
        Error = RegSetValueExW(Setup, L"SetupType", 0, REG_DWORD, (PBYTE)&Zero, sizeof(Zero));
        if (!Error)
            Error = RegSetValueExW(Setup, L"SystemSetupInProgress", 0, REG_DWORD, (PBYTE)&Zero, sizeof(Zero));
        if (!Error) Error = RegFlushKey(Setup);
        RegCloseKey(Setup);
    }
    if (Error)
    {
        DbgPrint("ARM account setup: setup completion failed %lu\n", Error);
        return 1;
    }
    DbgPrint("ARM account setup: PASS (tester, interactive token and wrong-password rejection)\n");
    return 0;
}
