# Elden Ring Multi-Instance Mod

Run multiple instances of Elden Ring simultaneously. Useful for co-op with yourself, testing, or playing on multiple accounts.

## How It Works

Elden Ring prevents multiple instances by creating a mutex named `Global\SekiroMutex` on startup. If the mutex already exists, the game refuses to launch.

This mod is a lightweight **DLL proxy** (`dinput8.dll`) that intercepts mutex creation at the Windows API level. When the game tries to create "SekiroMutex", the mod returns a dummy handle instead — no real named mutex is created, so subsequent instances launch without conflict.

**Technical details:**
- Hooks both `CreateMutexW` and `CreateMutexExW` via IAT patching + inline hook fallback
- Forwards all real DirectInput calls to the system `dinput8.dll`
- Writes a diagnostic log (`DINPUT8.log`) on each launch
- Does **not** modify any game files — works via Windows DLL search order

## Installation

1. Download `dinput8.dll` from [Releases](../../releases)
2. Copy it to your Elden Ring `Game/` folder (next to `eldenring.exe`)
3. Launch the game normally (or via Seamless Coop launcher) — twice

That's it. Both instances will start.

### Where is the Game folder?

Typical locations:
```
D:\SteamLibrary\steamapps\common\ELDEN RING\Game\
C:\Program Files (x86)\Steam\steamapps\common\ELDEN RING\Game\
```

Or in Steam: right-click Elden Ring → Manage → Browse Local Files.

## Uninstallation

Delete `dinput8.dll` from the `Game/` folder. Optionally delete `DINPUT8.log`.

## Compatibility

| Mod | Status | Notes |
|-----|--------|-------|
| **Seamless Coop** | Compatible | Uses its own launcher + `ersc.dll`, no conflict |
| **ModEngine2** | **Conflict** | Also uses `dinput8.dll` — see [ModEngine2 Workaround](#modengine2-workaround) |
| **EAC** | Not compatible | Only use offline or with Seamless Coop (which disables EAC) |
| **Game updates** | Survives | Hooks Windows API, not game code — version-independent |

### ModEngine2 Workaround

If you use ModEngine2, it already occupies the `dinput8.dll` slot. You can rebuild this mod as `version.dll` instead:

```bash
# Edit dinput8_proxy.c: rename DirectInput8Create export to GetFileVersionInfoW (and other version.dll exports)
# Or use the kill_mutex.ps1 fallback script instead
```

## Fallback: Runtime Mutex Kill

If the DLL approach doesn't work for your setup, a PowerShell script is included that kills the mutex at runtime:

1. Launch Elden Ring (first instance)
2. Run as **Administrator**:
   ```powershell
   powershell -ExecutionPolicy Bypass -File kill_mutex.ps1
   ```
3. Launch second instance

## Troubleshooting

**Game won't start / crashes on launch:**
Delete `dinput8.dll`. If the game works without it, check `DINPUT8.log` for diagnostics.

**Second instance still won't launch:**
Check `DINPUT8.log` — look for `BLOCKED SekiroMutex`. If absent, the game may have changed its mutex mechanism. Use `kill_mutex.ps1` as fallback and [open an issue](../../issues).

**How do I know it's working?**
After launching, check `DINPUT8.log` in the Game folder. You should see:
```
=== dinput8 proxy loaded ===
[INIT] ...
[INLINE] BLOCKED SekiroMutex (CreateMutexExW)
```

## Building from Source

Requires [MinGW-w64](https://www.mingw-w64.org/) (or any x86_64 Windows C compiler).

```bash
x86_64-w64-mingw32-gcc -shared -o dinput8.dll dinput8_proxy.c -lkernel32 -Wall -O2
```

Or with MSVC:
```bash
cl /LD /O2 dinput8_proxy.c kernel32.lib /Fe:dinput8.dll
```

## How It Works (Deep Dive)

1. **DLL Proxy**: Windows searches the application directory before `System32` when loading DLLs. By placing our `dinput8.dll` next to `eldenring.exe`, the game loads ours first. We export `DirectInput8Create` and forward it to the real system DLL.

2. **IAT Hooking**: On `DLL_PROCESS_ATTACH`, we walk the game's PE import table looking for `CreateMutexW`/`CreateMutexExW`. If found, we replace the function pointer with our hook.

3. **Inline Hooking (fallback)**: If the function isn't in the IAT (e.g., resolved through API sets like `api-ms-win-core-synch-*`), we patch the first 12 bytes of the actual kernel32 function with a `mov rax, addr; jmp rax` trampoline.

4. **The Hook**: When `CreateMutex*` is called with a name containing "SekiroMutex", we return a `CreateEventW()` handle instead — a valid Windows handle that keeps the game happy, but not a named mutex that would block other instances.

## License

MIT License — do whatever you want with it.
