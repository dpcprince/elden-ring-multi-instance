WHAT THIS DOES

Elden Ring blocks multiple instances by creating a system-wide mutex
(Global\SekiroMutex). Even if you run two copies under different
Windows users or desktops, the second one refuses to launch.

This mod removes that restriction with a tiny DLL proxy (48KB).
Two people can play on the same machine using ASTER, Sandboxie,
or any multi-seat / split-desktop setup.

  - No game files modified
  - Survives game updates (hooks Windows API, not game code)
  - No scripts to run — drop the file and launch


ABOUT ANTI-CHEAT (EAC)

The standard launcher (start_protected_game.exe) activates EAC,
which blocks DLL mods from loading — including this one.

You have two options depending on your use case:

  Playing co-op together (two players, one PC):
    Use Seamless Coop. Its launcher (ersc_launcher.exe) bypasses
    EAC and provides its own P2P networking so both instances can
    connect to each other.

  Playing separately (two players, different saves, offline):
    Launch eldenring.exe directly instead of start_protected_game.exe.
    No EAC, no Seamless Coop needed. Both instances run independently.


HOW IT WORKS

The mod uses Windows DLL search order — the game loads our
dinput8.dll from its folder before the system one. On load, it
hooks the mutex creation call and returns a dummy handle instead
of a real named mutex. Each instance thinks it's the only one.


INSTALLATION

  1. Download dinput8.dll from the Files tab
  2. Copy it to your Elden Ring Game/ folder (next to eldenring.exe)
  3. Launch the game from both environments

To find your Game folder:
  Steam > right-click Elden Ring > Manage > Browse Local Files > Game/


UNINSTALLATION

Delete dinput8.dll from the Game/ folder. That's it.
Optionally delete DINPUT8.log (auto-generated diagnostic log).


VERIFYING IT WORKS

Each launch writes a small DINPUT8.log in the Game folder.
Open it — if you see "BLOCKED SekiroMutex", it's working.


COMPATIBILITY

  Seamless Coop   — Works. Needed for co-op between instances.
  Offline / solo  — Works. Launch eldenring.exe directly.
  Official online — Not compatible. EAC blocks DLL mods.
  ModEngine2      — Conflict (both use dinput8.dll).
                    Use the included kill_mutex.ps1 fallback instead.
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
