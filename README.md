# Elden Ring Multi-Instance Mod

Elden Ring only allows one instance per machine. This is a problem if you're using [ASTER](https://www.ibik.ru/), [Sandboxie](https://sandboxie-plus.com/), or any multi-seat / desktop-sharing setup where two players share the same PC.

This mod removes that restriction.

## The Problem

Elden Ring creates a system-wide mutex (`Global\SekiroMutex`) on startup. If it already exists, the game exits immediately — even if the two instances run under different Windows users or in separate desktops. This kills multi-seat setups where two people want to play together (or separately) on the same machine.

## The Fix

A tiny DLL proxy (`dinput8.dll`, 48KB) that sits in the game folder and intercepts the mutex creation call. Instead of creating a real named mutex, it returns a dummy handle. Each instance thinks it's the only one running.

- No game files are modified
- Survives game updates (hooks Windows API, not game code)
- No scripts to run — just drop the file and launch

## Installation

1. Download `dinput8.dll` from [Releases](../../releases)
2. Copy it to your Elden Ring `Game/` folder (next to `eldenring.exe`)
3. Launch the game from both environments

### Where is the Game folder?

In Steam: right-click Elden Ring → Manage → Browse Local Files → `Game/`

Typical paths:
```
D:\SteamLibrary\steamapps\common\ELDEN RING\Game\
C:\Program Files (x86)\Steam\steamapps\common\ELDEN RING\Game\
```

## Uninstallation

Delete `dinput8.dll` from the `Game/` folder. Optionally delete `DINPUT8.log`.

## Compatibility

| | Status |
|---|---|
| **Seamless Coop** | Works — uses its own launcher + DLL, no conflict |
| **ModEngine2** | Conflict — also uses `dinput8.dll`. See [workaround](#modengine2-workaround) |
| **EAC** | Not compatible — use offline or Seamless Coop (which disables EAC) |
| **Game updates** | Unaffected — version-independent |

### ModEngine2 Workaround

ModEngine2 already uses the `dinput8.dll` slot. If you need both, use the included `kill_mutex.ps1` fallback script instead (see below).

## Fallback: kill_mutex.ps1

If the DLL doesn't work for your setup (e.g., ModEngine2 conflict), a PowerShell script is included that kills the mutex at runtime:

1. Launch Elden Ring (first instance)
2. Run as **Administrator**:
   ```powershell
   powershell -ExecutionPolicy Bypass -File kill_mutex.ps1
   ```
3. Launch the second instance

## Verifying It Works

Each launch writes a small `DINPUT8.log` in the Game folder:
```
=== dinput8 proxy loaded ===
[INLINE] BLOCKED SekiroMutex (CreateMutexExW)
```

If you see `BLOCKED SekiroMutex`, it's working.

## Troubleshooting

**Game won't start after installing:**
Delete `dinput8.dll` and check if the game starts without it. If it does, check `DINPUT8.log` for clues and [open an issue](../../issues).

**Second instance still won't launch:**
Check `DINPUT8.log` — if `BLOCKED SekiroMutex` doesn't appear, the game may have changed its mutex mechanism. Try the `kill_mutex.ps1` fallback.

## Building from Source

```bash
x86_64-w64-mingw32-gcc -shared -o dinput8.dll dinput8_proxy.c -lkernel32 -Wall -O2
```

## How It Works

The DLL uses Windows DLL search order — the game loads `dinput8.dll` from its own directory before `System32`. Our version hooks `CreateMutexW` and `CreateMutexExW` (via IAT patching + inline hook fallback) to intercept the `SekiroMutex` creation. All other calls, including DirectInput, are forwarded to the real system DLLs.

## License

[MIT](LICENSE)
