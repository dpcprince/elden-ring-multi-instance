WHAT THIS DOES

Elden Ring blocks multiple instances by creating a system-wide mutex
(Global\SekiroMutex). Even if you run two copies under different
Windows users or desktops, the second one refuses to launch.

This mod removes that restriction with a tiny DLL proxy (48KB).
Two people can play on the same machine using ASTER, Sandboxie,
or any multi-seat / split-desktop setup.


HOW IT WORKS

The mod uses Windows DLL search order — the game loads our
dinput8.dll from its folder before the system one. On load, it
hooks the mutex creation call and returns a dummy handle instead
of a real named mutex. Each instance thinks it's the only one.

  - No game files modified
  - Survives game updates (hooks Windows API, not game code)
  - No scripts to run — drop the file and launch
  - Compatible with Seamless Coop

IMPORTANT: This mod does NOT work with Easy Anti-Cheat (EAC).
You must launch the game through Seamless Coop (ersc_launcher.exe)
or another launcher that bypasses EAC. The standard
start_protected_game.exe will not load the mod.


INSTALLATION

  1. Download dinput8.dll from the Files tab
  2. Copy it to your Elden Ring Game/ folder (next to eldenring.exe)
  3. Launch the game from both environments

To find your Game folder:
  Steam → right-click Elden Ring → Manage → Browse Local Files → Game/


UNINSTALLATION

Delete dinput8.dll from the Game/ folder. That's it.
Optionally delete DINPUT8.log (auto-generated diagnostic log).


VERIFYING IT WORKS

Each launch writes a small DINPUT8.log in the Game folder.
Open it — if you see "BLOCKED SekiroMutex", it's working.


COMPATIBILITY

  Seamless Coop   — Required. Its launcher bypasses EAC, which is
                    needed for the mod to load.
  ModEngine2      — Conflict (both use dinput8.dll).
                    Use the included kill_mutex.ps1 fallback instead.
  EAC             — NOT compatible. EAC prevents the DLL from loading.
                    You need a launcher that bypasses it.
  Game updates    — Unaffected, version-independent.


FALLBACK SCRIPT (kill_mutex.ps1)

If the DLL doesn't work for your setup (e.g., ModEngine2 conflict),
a PowerShell script is included that kills the mutex at runtime:

  1. Launch Elden Ring (first instance)
  2. Run as Administrator:
     powershell -ExecutionPolicy Bypass -File kill_mutex.ps1
  3. Launch second instance


SOURCE CODE

https://github.com/dpcprince/elden-ring-multi-instance
