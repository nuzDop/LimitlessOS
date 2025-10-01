/*
 * Windows Persona Implementation
 * Translates Win32 API calls to LimitlessOS syscalls
 */

#include "win32_persona.h"
#include "posix_persona.h"  // Reuse POSIX syscalls where applicable
#include <string.h>
#include <stdlib.h>

/* Global Win32 context */
static win32_context_t g_win32_ctx = {0};
static win32_stats_t g_win32_stats = {0};

/* Handle types */
#define HANDLE_TYPE_FILE    1
#define HANDLE_TYPE_PROCESS 2
#define HANDLE_TYPE_THREAD  3
#define HANDLE_TYPE_HEAP    4

typedef struct win32_handle {
    uint32_t type;
    uint32_t flags;
    union {
        int fd;           // File descriptor (for files)
        uint32_t pid;     // Process ID
        uint32_t tid;     // Thread ID
        void* heap_ptr;   // Heap pointer
    };
} win32_handle_t;

/* Initialize Win32 persona */
int win32_persona_init(void) {
    /* Initialize context */
    for (int i = 0; i < sizeof(g_win32_ctx); i++) {
        ((uint8_t*)&g_win32_ctx)[i] = 0;
    }

    /* Allocate handle table */
    g_win32_ctx.max_handles = 1024;
    g_win32_ctx.handle_table = (void**)malloc(sizeof(void*) * g_win32_ctx.max_handles);
    if (!g_win32_ctx.handle_table) {
        return -1;
    }

    for (uint32_t i = 0; i < g_win32_ctx.max_handles; i++) {
        g_win32_ctx.handle_table[i] = NULL;
    }

    g_win32_ctx.next_handle = 1;
    g_win32_ctx.process_id = getpid();
    g_win32_ctx.last_error = ERROR_SUCCESS;

    /* Initialize current directory */
    g_win32_ctx.current_directory = (char*)malloc(260);  // MAX_PATH
    if (g_win32_ctx.current_directory) {
        getcwd(g_win32_ctx.current_directory, 260);
    }

    return 0;
}

/* Shutdown Win32 persona */
int win32_persona_shutdown(void) {
    if (g_win32_ctx.handle_table) {
        free(g_win32_ctx.handle_table);
    }
    if (g_win32_ctx.current_directory) {
        free(g_win32_ctx.current_directory);
    }
    if (g_win32_ctx.command_line) {
        free(g_win32_ctx.command_line);
    }
    return 0;
}

/* Get Win32 context */
win32_context_t* win32_get_context(void) {
    return &g_win32_ctx;
}

/* Allocate handle */
static HANDLE win32_alloc_handle(uint32_t type) {
    __sync_lock_test_and_set(&g_win32_ctx.lock, 1);

    for (uint32_t i = g_win32_ctx.next_handle; i < g_win32_ctx.max_handles; i++) {
        if (g_win32_ctx.handle_table[i] == NULL) {
            win32_handle_t* h = (win32_handle_t*)malloc(sizeof(win32_handle_t));
            if (!h) {
                __sync_lock_release(&g_win32_ctx.lock);
                return INVALID_HANDLE_VALUE;
            }

            for (int j = 0; j < sizeof(win32_handle_t); j++) {
                ((uint8_t*)h)[j] = 0;
            }

            h->type = type;
            g_win32_ctx.handle_table[i] = h;
            g_win32_ctx.next_handle = i + 1;

            __sync_lock_release(&g_win32_ctx.lock);
            return (HANDLE)(uintptr_t)i;
        }
    }

    __sync_lock_release(&g_win32_ctx.lock);
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return INVALID_HANDLE_VALUE;
}

/* Get handle data */
static win32_handle_t* win32_get_handle(HANDLE h) {
    uintptr_t index = (uintptr_t)h;
    if (index == 0 || index >= g_win32_ctx.max_handles) {
        return NULL;
    }
    return (win32_handle_t*)g_win32_ctx.handle_table[index];
}

/* File I/O */
HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   SECURITY_ATTRIBUTES* lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    (void)dwShareMode;
    (void)lpSecurityAttributes;
    (void)hTemplateFile;
    (void)dwFlagsAndAttributes;

    if (!lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    /* Translate access flags */
    int flags = 0;
    if (dwDesiredAccess & GENERIC_READ) flags |= 0x0001;  // O_RDONLY
    if (dwDesiredAccess & GENERIC_WRITE) flags |= 0x0002;  // O_WRONLY

    /* Translate creation disposition */
    switch (dwCreationDisposition) {
        case CREATE_NEW:
            flags |= 0x0100;  // O_CREAT | O_EXCL
            break;
        case CREATE_ALWAYS:
            flags |= 0x0200 | 0x0100;  // O_CREAT | O_TRUNC
            break;
        case OPEN_EXISTING:
            break;
        case OPEN_ALWAYS:
            flags |= 0x0100;  // O_CREAT
            break;
        case TRUNCATE_EXISTING:
            flags |= 0x0200;  // O_TRUNC
            break;
    }

    /* Open file via POSIX */
    int fd = open(lpFileName, flags, 0644);
    if (fd < 0) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }

    /* Allocate handle */
    HANDLE handle = win32_alloc_handle(HANDLE_TYPE_FILE);
    if (handle == INVALID_HANDLE_VALUE) {
        close(fd);
        return INVALID_HANDLE_VALUE;
    }

    win32_handle_t* h = win32_get_handle(handle);
    h->fd = fd;

    g_win32_stats.syscalls_translated++;
    g_win32_stats.files_opened++;

    SetLastError(ERROR_SUCCESS);
    return handle;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              DWORD* lpNumberOfBytesRead, OVERLAPPED* lpOverlapped) {
    (void)lpOverlapped;

    win32_handle_t* h = win32_get_handle(hFile);
    if (!h || h->type != HANDLE_TYPE_FILE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    ssize_t result = read(h->fd, lpBuffer, nNumberOfBytesToRead);
    if (result < 0) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }

    if (lpNumberOfBytesRead) {
        *lpNumberOfBytesRead = (DWORD)result;
    }

    g_win32_stats.syscalls_translated++;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               DWORD* lpNumberOfBytesWritten, OVERLAPPED* lpOverlapped) {
    (void)lpOverlapped;

    win32_handle_t* h = win32_get_handle(hFile);
    if (!h || h->type != HANDLE_TYPE_FILE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    ssize_t result = write(h->fd, lpBuffer, nNumberOfBytesToWrite);
    if (result < 0) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }

    if (lpNumberOfBytesWritten) {
        *lpNumberOfBytesWritten = (DWORD)result;
    }

    g_win32_stats.syscalls_translated++;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL CloseHandle(HANDLE hObject) {
    win32_handle_t* h = win32_get_handle(hObject);
    if (!h) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    /* Close based on type */
    if (h->type == HANDLE_TYPE_FILE) {
        close(h->fd);
    }

    /* Free handle */
    uintptr_t index = (uintptr_t)hObject;
    if (index < g_win32_ctx.max_handles) {
        free(g_win32_ctx.handle_table[index]);
        g_win32_ctx.handle_table[index] = NULL;
    }

    g_win32_stats.syscalls_translated++;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG* lpDistanceToMoveHigh,
                     DWORD dwMoveMethod) {
    win32_handle_t* h = win32_get_handle(hFile);
    if (!h || h->type != HANDLE_TYPE_FILE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return 0xFFFFFFFF;
    }

    off_t offset = lDistanceToMove;
    if (lpDistanceToMoveHigh) {
        offset |= ((off_t)*lpDistanceToMoveHigh) << 32;
    }

    int whence = 0;  // SEEK_SET
    if (dwMoveMethod == 1) whence = 1;  // SEEK_CUR
    else if (dwMoveMethod == 2) whence = 2;  // SEEK_END

    off_t result = lseek(h->fd, offset, whence);
    if (result < 0) {
        SetLastError(ERROR_ACCESS_DENIED);
        return 0xFFFFFFFF;
    }

    g_win32_stats.syscalls_translated++;
    SetLastError(ERROR_SUCCESS);
    return (DWORD)result;
}

BOOL DeleteFileA(LPCSTR lpFileName) {
    if (!lpFileName) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (unlink(lpFileName) < 0) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    g_win32_stats.syscalls_translated++;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

/* Memory Management */
LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType,
                    DWORD flProtect) {
    (void)lpAddress;
    (void)flAllocationType;
    (void)flProtect;

    void* ptr = malloc(dwSize);
    if (!ptr) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    g_win32_stats.syscalls_translated++;
    g_win32_stats.allocations++;
    SetLastError(ERROR_SUCCESS);
    return ptr;
}

BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    (void)dwSize;
    (void)dwFreeType;

    if (!lpAddress) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    free(lpAddress);

    g_win32_stats.syscalls_translated++;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

HANDLE GetProcessHeap(void) {
    static HANDLE heap = NULL;
    if (!heap) {
        heap = win32_alloc_handle(HANDLE_TYPE_HEAP);
    }
    return heap;
}

LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) {
    (void)hHeap;
    (void)dwFlags;

    void* ptr = malloc(dwBytes);
    if (!ptr) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    g_win32_stats.syscalls_translated++;
    g_win32_stats.allocations++;
    SetLastError(ERROR_SUCCESS);
    return ptr;
}

BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
    (void)hHeap;
    (void)dwFlags;

    if (!lpMem) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    free(lpMem);

    g_win32_stats.syscalls_translated++;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

/* Process Management */
BOOL CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                    SECURITY_ATTRIBUTES* lpProcessAttributes,
                    SECURITY_ATTRIBUTES* lpThreadAttributes,
                    BOOL bInheritHandles, DWORD dwCreationFlags,
                    LPVOID lpEnvironment, LPCSTR lpCurrentDirectory,
                    STARTUPINFO* lpStartupInfo, PROCESS_INFORMATION* lpProcessInformation) {
    (void)lpProcessAttributes;
    (void)lpThreadAttributes;
    (void)bInheritHandles;
    (void)dwCreationFlags;
    (void)lpEnvironment;
    (void)lpCurrentDirectory;
    (void)lpStartupInfo;

    if (!lpApplicationName && !lpCommandLine) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pid_t pid = fork();
    if (pid < 0) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    if (pid == 0) {
        /* Child process */
        const char* argv[2] = { lpApplicationName ? lpApplicationName : lpCommandLine, NULL };
        execve(argv[0], (char* const*)argv, NULL);
        _exit(1);
    }

    /* Parent process */
    if (lpProcessInformation) {
        HANDLE process_handle = win32_alloc_handle(HANDLE_TYPE_PROCESS);
        win32_handle_t* h = win32_get_handle(process_handle);
        if (h) {
            h->pid = pid;
        }

        lpProcessInformation->hProcess = process_handle;
        lpProcessInformation->hThread = INVALID_HANDLE_VALUE;
        lpProcessInformation->dwProcessId = pid;
        lpProcessInformation->dwThreadId = 0;
    }

    g_win32_stats.syscalls_translated++;
    g_win32_stats.processes_created++;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

void ExitProcess(DWORD uExitCode) {
    exit((int)uExitCode);
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    (void)dwMilliseconds;

    win32_handle_t* h = win32_get_handle(hHandle);
    if (!h || h->type != HANDLE_TYPE_PROCESS) {
        SetLastError(ERROR_INVALID_HANDLE);
        return WAIT_FAILED;
    }

    int status;
    pid_t result = waitpid((pid_t)h->pid, &status, 0);
    if (result < 0) {
        SetLastError(ERROR_INVALID_HANDLE);
        return WAIT_FAILED;
    }

    g_win32_stats.syscalls_translated++;
    SetLastError(ERROR_SUCCESS);
    return WAIT_OBJECT_0;
}

BOOL GetExitCodeProcess(HANDLE hProcess, DWORD* lpExitCode) {
    (void)hProcess;
    if (!lpExitCode) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    *lpExitCode = 0;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

HANDLE GetCurrentProcess(void) {
    static HANDLE current_process = NULL;
    if (!current_process) {
        current_process = win32_alloc_handle(HANDLE_TYPE_PROCESS);
        win32_handle_t* h = win32_get_handle(current_process);
        if (h) {
            h->pid = getpid();
        }
    }
    return current_process;
}

DWORD GetCurrentProcessId(void) {
    return (DWORD)getpid();
}

/* Error Handling */
DWORD GetLastError(void) {
    return g_win32_ctx.last_error;
}

void SetLastError(DWORD dwErrCode) {
    g_win32_ctx.last_error = dwErrCode;
}

/* String Manipulation */
int lstrlenA(LPCSTR lpString) {
    if (!lpString) return 0;
    return (int)strlen(lpString);
}

LPSTR lstrcpyA(LPSTR lpString1, LPCSTR lpString2) {
    if (!lpString1 || !lpString2) return NULL;
    return strcpy(lpString1, lpString2);
}

LPSTR lstrcatA(LPSTR lpString1, LPCSTR lpString2) {
    if (!lpString1 || !lpString2) return NULL;
    return strcat(lpString1, lpString2);
}

int lstrcmpA(LPCSTR lpString1, LPCSTR lpString2) {
    if (!lpString1 || !lpString2) return 0;
    return strcmp(lpString1, lpString2);
}

/* Console I/O */
HANDLE GetStdHandle(DWORD nStdHandle) {
    static HANDLE handles[3] = {NULL, NULL, NULL};
    int index = 0;

    if (nStdHandle == STD_INPUT_HANDLE) index = 0;
    else if (nStdHandle == STD_OUTPUT_HANDLE) index = 1;
    else if (nStdHandle == STD_ERROR_HANDLE) index = 2;

    if (!handles[index]) {
        handles[index] = win32_alloc_handle(HANDLE_TYPE_FILE);
        win32_handle_t* h = win32_get_handle(handles[index]);
        if (h) {
            h->fd = index;  // 0=stdin, 1=stdout, 2=stderr
        }
    }

    return handles[index];
}

BOOL WriteConsoleA(HANDLE hConsoleOutput, const void* lpBuffer, DWORD nNumberOfCharsToWrite,
                   DWORD* lpNumberOfCharsWritten, void* lpReserved) {
    (void)lpReserved;
    return WriteFile(hConsoleOutput, lpBuffer, nNumberOfCharsToWrite, lpNumberOfCharsWritten, NULL);
}

BOOL ReadConsoleA(HANDLE hConsoleInput, void* lpBuffer, DWORD nNumberOfCharsToRead,
                  DWORD* lpNumberOfCharsRead, void* lpInputControl) {
    (void)lpInputControl;
    return ReadFile(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, NULL);
}

/* Stub functions */
void Sleep(DWORD dwMilliseconds) {
    usleep(dwMilliseconds * 1000);
}

DWORD GetTickCount(void) {
    return 0;  // TODO: Implement
}

HMODULE LoadLibraryA(LPCSTR lpLibFileName) {
    (void)lpLibFileName;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}

BOOL FreeLibrary(HMODULE hLibModule) {
    (void)hLibModule;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

void* GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    (void)hModule;
    (void)lpProcName;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}

LPSTR GetCommandLineA(void) {
    return g_win32_ctx.command_line;
}

/* Statistics */
int win32_get_stats(win32_stats_t* out_stats) {
    if (!out_stats) return -1;

    out_stats->syscalls_translated = g_win32_stats.syscalls_translated;
    out_stats->files_opened = g_win32_stats.files_opened;
    out_stats->processes_created = g_win32_stats.processes_created;
    out_stats->threads_created = g_win32_stats.threads_created;
    out_stats->allocations = g_win32_stats.allocations;
    out_stats->modules_loaded = g_win32_stats.modules_loaded;

    return 0;
}
