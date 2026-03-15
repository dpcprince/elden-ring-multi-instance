/*
 * dinput8.dll proxy — blocks Elden Ring's "SekiroMutex" to allow multiple instances.
 *
 * Build (MinGW):
 *   x86_64-w64-mingw32-gcc -shared -o dinput8.dll dinput8_proxy.c -lkernel32 -Wall -O2
 *
 * Install:  Copy dinput8.dll to the Elden Ring Game/ folder
 * Uninstall: Delete dinput8.dll from the Game/ folder
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#include <stdio.h>

/* ── Debug log (writes to dinput8_proxy.log next to the DLL) ───────── */

static FILE *g_log = NULL;
static wchar_t g_log_path[MAX_PATH];

static void log_init(HINSTANCE hinstDLL)
{
    GetModuleFileNameW(hinstDLL, g_log_path, MAX_PATH);
    /* Replace .dll with .log */
    wchar_t *dot = wcsrchr(g_log_path, L'.');
    if (dot) wcscpy(dot, L".log");
    else wcscat(g_log_path, L".log");
    g_log = _wfopen(g_log_path, L"w");
}

static void proxy_log(const char *fmt, ...)
{
    if (!g_log) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    va_end(args);
    fflush(g_log);
}

/* ── Forward the real DirectInput8Create ────────────────────────────── */

typedef HRESULT (WINAPI *PFN_DirectInput8Create)(
    HINSTANCE hinst, DWORD dwVersion, const void *riidltf,
    void **ppvOut, void *punkOuter);

static PFN_DirectInput8Create real_DirectInput8Create = NULL;
static HMODULE real_dinput8 = NULL;

__declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD dwVersion, const void *riidltf,
    void **ppvOut, void *punkOuter)
{
    if (!real_dinput8) {
        wchar_t sysdir[MAX_PATH];
        GetSystemDirectoryW(sysdir, MAX_PATH);
        wcscat(sysdir, L"\\dinput8.dll");
        real_dinput8 = LoadLibraryW(sysdir);
        proxy_log("[PROXY] Loaded real dinput8.dll: %s\n", real_dinput8 ? "OK" : "FAILED");
    }
    if (!real_dinput8) return E_FAIL;

    if (!real_DirectInput8Create) {
        real_DirectInput8Create = (PFN_DirectInput8Create)
            GetProcAddress(real_dinput8, "DirectInput8Create");
    }
    if (!real_DirectInput8Create) return E_FAIL;

    return real_DirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

/* ── Mutex hooks (both CreateMutexW and CreateMutexExW) ────────────── */

typedef HANDLE (WINAPI *PFN_CreateMutexW)(
    LPSECURITY_ATTRIBUTES lpMutexAttributes,
    BOOL bInitialOwner,
    LPCWSTR lpName);

typedef HANDLE (WINAPI *PFN_CreateMutexExW)(
    LPSECURITY_ATTRIBUTES lpMutexAttributes,
    LPCWSTR lpName,
    DWORD dwFlags,
    DWORD dwDesiredAccess);

static PFN_CreateMutexW original_CreateMutexW = NULL;
static PFN_CreateMutexExW original_CreateMutexExW = NULL;

static HANDLE WINAPI hooked_CreateMutexW(
    LPSECURITY_ATTRIBUTES lpMutexAttributes,
    BOOL bInitialOwner,
    LPCWSTR lpName)
{
    if (lpName) {
        proxy_log("[HOOK] CreateMutexW: %ls\n", lpName);
        if (wcsstr(lpName, L"SekiroMutex")) {
            proxy_log("[HOOK] BLOCKED SekiroMutex (CreateMutexW)\n");
            return CreateEventW(NULL, FALSE, FALSE, NULL);
        }
    }
    return original_CreateMutexW(lpMutexAttributes, bInitialOwner, lpName);
}

static HANDLE WINAPI hooked_CreateMutexExW(
    LPSECURITY_ATTRIBUTES lpMutexAttributes,
    LPCWSTR lpName,
    DWORD dwFlags,
    DWORD dwDesiredAccess)
{
    if (lpName) {
        proxy_log("[HOOK] CreateMutexExW: %ls\n", lpName);
        if (wcsstr(lpName, L"SekiroMutex")) {
            proxy_log("[HOOK] BLOCKED SekiroMutex (CreateMutexExW)\n");
            return CreateEventW(NULL, FALSE, FALSE, NULL);
        }
    }
    return original_CreateMutexExW(lpMutexAttributes, lpName, dwFlags, dwDesiredAccess);
}

/* ── IAT patching ──────────────────────────────────────────────────── */

/* Scan ALL imported DLLs for a function by name (handles api-ms-* redirects) */
static int patch_iat_any_dll(HMODULE module, const char *func_name,
                             void *hook, void **original)
{
    BYTE *base = (BYTE *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;

    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    IMAGE_DATA_DIRECTORY *imp_dir =
        &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!imp_dir->VirtualAddress) return 0;

    IMAGE_IMPORT_DESCRIPTOR *imp =
        (IMAGE_IMPORT_DESCRIPTOR *)(base + imp_dir->VirtualAddress);

    for (; imp->Name; imp++) {
        const char *dll_name = (const char *)(base + imp->Name);

        IMAGE_THUNK_DATA *orig_thunk =
            (IMAGE_THUNK_DATA *)(base + imp->OriginalFirstThunk);
        IMAGE_THUNK_DATA *iat_thunk =
            (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);

        for (; orig_thunk->u1.AddressOfData; orig_thunk++, iat_thunk++) {
            if (orig_thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) continue;

            IMAGE_IMPORT_BY_NAME *import =
                (IMAGE_IMPORT_BY_NAME *)(base + orig_thunk->u1.AddressOfData);

            if (strcmp(import->Name, func_name) == 0) {
                proxy_log("[IAT] Found %s in %s — patching\n", func_name, dll_name);
                DWORD old_protect;
                VirtualProtect(&iat_thunk->u1.Function, sizeof(void *),
                               PAGE_READWRITE, &old_protect);
                *original = (void *)iat_thunk->u1.Function;
                iat_thunk->u1.Function = (ULONGLONG)hook;
                VirtualProtect(&iat_thunk->u1.Function, sizeof(void *),
                               old_protect, &old_protect);
                return 1;
            }
        }
    }
    return 0;
}

/* Log all imported DLLs for diagnostics */
static void log_imports(HMODULE module)
{
    BYTE *base = (BYTE *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;

    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    IMAGE_DATA_DIRECTORY *imp_dir =
        &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!imp_dir->VirtualAddress) return;

    IMAGE_IMPORT_DESCRIPTOR *imp =
        (IMAGE_IMPORT_DESCRIPTOR *)(base + imp_dir->VirtualAddress);

    proxy_log("[IAT] Imported DLLs:\n");
    for (; imp->Name; imp++) {
        const char *dll_name = (const char *)(base + imp->Name);
        proxy_log("[IAT]   %s\n", dll_name);
    }
}

/* ── Inline hook fallback (patch function prologue) ────────────────── */

#pragma pack(push, 1)
typedef struct {
    BYTE mov_rax[2];      /* 48 B8 */
    UINT64 address;       /* absolute address */
    BYTE jmp_rax[2];      /* FF E0 */
} JmpPatch;
#pragma pack(pop)

static BYTE saved_CreateMutexW_bytes[sizeof(JmpPatch)];
static BYTE saved_CreateMutexExW_bytes[sizeof(JmpPatch)];
static PFN_CreateMutexW real_addr_CreateMutexW = NULL;
static PFN_CreateMutexExW real_addr_CreateMutexExW = NULL;

static HANDLE WINAPI inline_hooked_CreateMutexW(
    LPSECURITY_ATTRIBUTES lpMutexAttributes,
    BOOL bInitialOwner,
    LPCWSTR lpName)
{
    if (lpName) {
        proxy_log("[INLINE] CreateMutexW: %ls\n", lpName);
        if (wcsstr(lpName, L"SekiroMutex")) {
            proxy_log("[INLINE] BLOCKED SekiroMutex (CreateMutexW)\n");
            return CreateEventW(NULL, FALSE, FALSE, NULL);
        }
    }
    /* Restore original bytes, call, re-patch */
    DWORD old;
    VirtualProtect(real_addr_CreateMutexW, sizeof(JmpPatch), PAGE_EXECUTE_READWRITE, &old);
    memcpy(real_addr_CreateMutexW, saved_CreateMutexW_bytes, sizeof(JmpPatch));
    VirtualProtect(real_addr_CreateMutexW, sizeof(JmpPatch), old, &old);

    HANDLE result = real_addr_CreateMutexW(lpMutexAttributes, bInitialOwner, lpName);

    VirtualProtect(real_addr_CreateMutexW, sizeof(JmpPatch), PAGE_EXECUTE_READWRITE, &old);
    JmpPatch patch = {{0x48, 0xB8}, (UINT64)inline_hooked_CreateMutexW, {0xFF, 0xE0}};
    memcpy(real_addr_CreateMutexW, &patch, sizeof(patch));
    VirtualProtect(real_addr_CreateMutexW, sizeof(JmpPatch), old, &old);

    return result;
}

static HANDLE WINAPI inline_hooked_CreateMutexExW(
    LPSECURITY_ATTRIBUTES lpMutexAttributes,
    LPCWSTR lpName,
    DWORD dwFlags,
    DWORD dwDesiredAccess)
{
    if (lpName) {
        proxy_log("[INLINE] CreateMutexExW: %ls\n", lpName);
        if (wcsstr(lpName, L"SekiroMutex")) {
            proxy_log("[INLINE] BLOCKED SekiroMutex (CreateMutexExW)\n");
            return CreateEventW(NULL, FALSE, FALSE, NULL);
        }
    }
    DWORD old;
    VirtualProtect(real_addr_CreateMutexExW, sizeof(JmpPatch), PAGE_EXECUTE_READWRITE, &old);
    memcpy(real_addr_CreateMutexExW, saved_CreateMutexExW_bytes, sizeof(JmpPatch));
    VirtualProtect(real_addr_CreateMutexExW, sizeof(JmpPatch), old, &old);

    HANDLE result = real_addr_CreateMutexExW(lpMutexAttributes, lpName, dwFlags, dwDesiredAccess);

    VirtualProtect(real_addr_CreateMutexExW, sizeof(JmpPatch), PAGE_EXECUTE_READWRITE, &old);
    JmpPatch patch = {{0x48, 0xB8}, (UINT64)inline_hooked_CreateMutexExW, {0xFF, 0xE0}};
    memcpy(real_addr_CreateMutexExW, &patch, sizeof(patch));
    VirtualProtect(real_addr_CreateMutexExW, sizeof(JmpPatch), old, &old);

    return result;
}

static int install_inline_hook(const char *func_name, void *hook, BYTE *saved_bytes)
{
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (!k32) return 0;

    void *func = (void *)GetProcAddress(k32, func_name);
    if (!func) return 0;

    proxy_log("[INLINE] Installing inline hook on %s at %p\n", func_name, func);

    DWORD old;
    VirtualProtect(func, sizeof(JmpPatch), PAGE_EXECUTE_READWRITE, &old);
    memcpy(saved_bytes, func, sizeof(JmpPatch));

    JmpPatch patch = {{0x48, 0xB8}, (UINT64)hook, {0xFF, 0xE0}};
    memcpy(func, &patch, sizeof(patch));

    VirtualProtect(func, sizeof(JmpPatch), old, &old);
    return 1;
}

/* ── DLL entry point ───────────────────────────────────────────────── */

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        log_init(hinstDLL);
        proxy_log("=== dinput8 proxy loaded ===\n");

        HMODULE exe = GetModuleHandleW(NULL);
        wchar_t exe_name[MAX_PATH];
        GetModuleFileNameW(exe, exe_name, MAX_PATH);
        proxy_log("[INIT] EXE: %ls\n", exe_name);
        proxy_log("[INIT] EXE base: %p\n", exe);

        /* Log all imports for diagnostics */
        log_imports(exe);

        /* Strategy 1: IAT hook — scan all DLLs for the function */
        int iat_w = patch_iat_any_dll(exe, "CreateMutexW",
                                       hooked_CreateMutexW, (void **)&original_CreateMutexW);
        int iat_ex = patch_iat_any_dll(exe, "CreateMutexExW",
                                        hooked_CreateMutexExW, (void **)&original_CreateMutexExW);

        proxy_log("[INIT] IAT hook CreateMutexW: %s\n", iat_w ? "OK" : "NOT FOUND");
        proxy_log("[INIT] IAT hook CreateMutexExW: %s\n", iat_ex ? "OK" : "NOT FOUND");

        /* Strategy 2: Inline hook (trampoline) as fallback for anything IAT missed */
        if (!iat_w) {
            HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
            real_addr_CreateMutexW = (PFN_CreateMutexW)GetProcAddress(k32, "CreateMutexW");
            if (install_inline_hook("CreateMutexW", inline_hooked_CreateMutexW,
                                    saved_CreateMutexW_bytes)) {
                proxy_log("[INIT] Inline hook CreateMutexW: OK\n");
            }
        }
        if (!iat_ex) {
            HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
            real_addr_CreateMutexExW = (PFN_CreateMutexExW)GetProcAddress(k32, "CreateMutexExW");
            if (install_inline_hook("CreateMutexExW", inline_hooked_CreateMutexExW,
                                    saved_CreateMutexExW_bytes)) {
                proxy_log("[INIT] Inline hook CreateMutexExW: OK\n");
            }
        }

        proxy_log("[INIT] All hooks installed. SekiroMutex will be blocked.\n");
    }
    else if (fdwReason == DLL_PROCESS_DETACH) {
        if (g_log) {
            proxy_log("=== dinput8 proxy unloaded ===\n");
            fclose(g_log);
            g_log = NULL;
        }
    }
    return TRUE;
}
