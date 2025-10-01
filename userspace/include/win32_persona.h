#ifndef LIMITLESS_WIN32_PERSONA_H
#define LIMITLESS_WIN32_PERSONA_H

/*
 * Windows Persona - Win32 API Translation Layer
 * Translates Win32 API calls to LimitlessOS syscalls
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Win32 types */
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HINSTANCE;
typedef void* HWND;
typedef void* HDC;
typedef void* HKEY;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef long LONG;
typedef int BOOL;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef wchar_t* LPWSTR;
typedef const wchar_t* LPCWSTR;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef size_t SIZE_T;

/* Boolean values */
#define TRUE  1
#define FALSE 0

/* Invalid handle value */
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

/* File access modes */
#define GENERIC_READ    0x80000000
#define GENERIC_WRITE   0x40000000
#define GENERIC_EXECUTE 0x20000000
#define GENERIC_ALL     0x10000000

/* File share modes */
#define FILE_SHARE_READ   0x00000001
#define FILE_SHARE_WRITE  0x00000002
#define FILE_SHARE_DELETE 0x00000004

/* File creation disposition */
#define CREATE_NEW        1
#define CREATE_ALWAYS     2
#define OPEN_EXISTING     3
#define OPEN_ALWAYS       4
#define TRUNCATE_EXISTING 5

/* File attributes */
#define FILE_ATTRIBUTE_READONLY  0x00000001
#define FILE_ATTRIBUTE_HIDDEN    0x00000002
#define FILE_ATTRIBUTE_SYSTEM    0x00000004
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_ARCHIVE   0x00000020
#define FILE_ATTRIBUTE_NORMAL    0x00000080

/* Standard handles */
#define STD_INPUT_HANDLE  ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE  ((DWORD)-12)

/* Memory allocation types */
#define MEM_COMMIT    0x00001000
#define MEM_RESERVE   0x00002000
#define MEM_RELEASE   0x00008000

/* Memory protection */
#define PAGE_NOACCESS          0x01
#define PAGE_READONLY          0x02
#define PAGE_READWRITE         0x04
#define PAGE_EXECUTE           0x10
#define PAGE_EXECUTE_READ      0x20
#define PAGE_EXECUTE_READWRITE 0x40

/* Error codes */
#define ERROR_SUCCESS           0
#define ERROR_FILE_NOT_FOUND    2
#define ERROR_ACCESS_DENIED     5
#define ERROR_INVALID_HANDLE    6
#define ERROR_NOT_ENOUGH_MEMORY 8
#define ERROR_INVALID_PARAMETER 87
#define ERROR_CALL_NOT_IMPLEMENTED 120

/* Process creation flags */
#define CREATE_NEW_CONSOLE        0x00000010
#define CREATE_NO_WINDOW          0x08000000
#define CREATE_SUSPENDED          0x00000004
#define DETACHED_PROCESS          0x00000008

/* Wait results */
#define WAIT_OBJECT_0  0
#define WAIT_TIMEOUT   258
#define WAIT_FAILED    0xFFFFFFFF
#define INFINITE       0xFFFFFFFF

/* SECURITY_ATTRIBUTES */
typedef struct {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES;

/* PROCESS_INFORMATION */
typedef struct {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
} PROCESS_INFORMATION;

/* STARTUPINFO */
typedef struct {
    DWORD cb;
    LPSTR lpReserved;
    LPSTR lpDesktop;
    LPSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFO;

/* OVERLAPPED */
typedef struct {
    uintptr_t Internal;
    uintptr_t InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        };
        LPVOID Pointer;
    };
    HANDLE hEvent;
} OVERLAPPED;

/* Win32 Persona Context */
typedef struct win32_context {
    /* Process information */
    uint32_t process_id;
    uint32_t thread_id;

    /* Handles */
    void** handle_table;
    uint32_t max_handles;
    uint32_t next_handle;

    /* Environment */
    char** environment;
    char* command_line;
    char* current_directory;

    /* Error state */
    DWORD last_error;

    /* Loaded modules */
    void* module_list;
    uint32_t num_modules;

    /* Thread safety */
    uint32_t lock;
} win32_context_t;

/* Win32 Persona API */
int win32_persona_init(void);
int win32_persona_shutdown(void);
win32_context_t* win32_get_context(void);

/* File I/O */
HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   SECURITY_ATTRIBUTES* lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              DWORD* lpNumberOfBytesRead, OVERLAPPED* lpOverlapped);

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               DWORD* lpNumberOfBytesWritten, OVERLAPPED* lpOverlapped);

BOOL CloseHandle(HANDLE hObject);

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG* lpDistanceToMoveHigh,
                     DWORD dwMoveMethod);

BOOL DeleteFileA(LPCSTR lpFileName);

/* Memory Management */
LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType,
                    DWORD flProtect);

BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);

HANDLE GetProcessHeap(void);

LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);

BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);

/* Process Management */
BOOL CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                    SECURITY_ATTRIBUTES* lpProcessAttributes,
                    SECURITY_ATTRIBUTES* lpThreadAttributes,
                    BOOL bInheritHandles, DWORD dwCreationFlags,
                    LPVOID lpEnvironment, LPCSTR lpCurrentDirectory,
                    STARTUPINFO* lpStartupInfo, PROCESS_INFORMATION* lpProcessInformation);

void ExitProcess(DWORD uExitCode);

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);

BOOL GetExitCodeProcess(HANDLE hProcess, DWORD* lpExitCode);

HANDLE GetCurrentProcess(void);

DWORD GetCurrentProcessId(void);

/* Thread Management */
HANDLE CreateThread(SECURITY_ATTRIBUTES* lpThreadAttributes, SIZE_T dwStackSize,
                    void* (*lpStartAddress)(void*), LPVOID lpParameter,
                    DWORD dwCreationFlags, DWORD* lpThreadId);

void ExitThread(DWORD dwExitCode);

DWORD GetCurrentThreadId(void);

/* Console I/O */
HANDLE GetStdHandle(DWORD nStdHandle);

BOOL WriteConsoleA(HANDLE hConsoleOutput, const void* lpBuffer, DWORD nNumberOfCharsToWrite,
                   DWORD* lpNumberOfCharsWritten, void* lpReserved);

BOOL ReadConsoleA(HANDLE hConsoleInput, void* lpBuffer, DWORD nNumberOfCharsToRead,
                  DWORD* lpNumberOfCharsRead, void* lpInputControl);

/* Error Handling */
DWORD GetLastError(void);
void SetLastError(DWORD dwErrCode);

/* String Manipulation */
int lstrlenA(LPCSTR lpString);
LPSTR lstrcpyA(LPSTR lpString1, LPCSTR lpString2);
LPSTR lstrcatA(LPSTR lpString1, LPCSTR lpString2);
int lstrcmpA(LPCSTR lpString1, LPCSTR lpString2);

/* System Information */
void GetSystemTime(void* lpSystemTime);
DWORD GetTickCount(void);
void Sleep(DWORD dwMilliseconds);

/* Module/DLL Management */
HMODULE LoadLibraryA(LPCSTR lpLibFileName);
BOOL FreeLibrary(HMODULE hLibModule);
void* GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
HMODULE GetModuleHandleA(LPCSTR lpModuleName);

/* Environment */
DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize);
BOOL SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue);
LPSTR GetCommandLineA(void);

/* Directory Management */
DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer);
BOOL SetCurrentDirectoryA(LPCSTR lpPathName);
BOOL CreateDirectoryA(LPCSTR lpPathName, SECURITY_ATTRIBUTES* lpSecurityAttributes);
BOOL RemoveDirectoryA(LPCSTR lpPathName);

/* Statistics */
typedef struct win32_stats {
    uint64_t syscalls_translated;
    uint64_t files_opened;
    uint64_t processes_created;
    uint64_t threads_created;
    uint64_t allocations;
    uint64_t modules_loaded;
} win32_stats_t;

int win32_get_stats(win32_stats_t* out_stats);

#endif /* LIMITLESS_WIN32_PERSONA_H */
