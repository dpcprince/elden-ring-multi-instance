# Kill Elden Ring's "Global\SekiroMutex" to allow a second instance
# Requires: Run as Administrator (needs SeDebugPrivilege to open game process handles)
#
# Usage:
#   1. Launch Elden Ring (first instance)
#   2. Wait for it to reach the title screen
#   3. Run this script as Administrator
#   4. Launch Elden Ring again (second instance)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Collections.Generic;
using System.ComponentModel;

public class MutexKiller
{
    // NT API for querying system handles
    [DllImport("ntdll.dll")]
    public static extern uint NtQuerySystemInformation(int infoClass, IntPtr info, int size, ref int returnLength);

    [DllImport("ntdll.dll")]
    public static extern uint NtQueryObject(IntPtr handle, int infoClass, IntPtr info, int size, ref int returnLength);

    // Kernel32
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(int access, bool inherit, int pid);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool DuplicateHandle(IntPtr sourceProcess, IntPtr sourceHandle,
        IntPtr targetProcess, out IntPtr targetHandle, int access, bool inherit, int options);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool CloseHandle(IntPtr handle);

    [DllImport("kernel32.dll")]
    public static extern IntPtr GetCurrentProcess();

    // Constants
    public const int SystemHandleInformation = 16;  // SystemExtendedHandleInformation = 64
    public const int ObjectNameInformation = 1;
    public const int PROCESS_DUP_HANDLE = 0x0040;
    public const int DUPLICATE_CLOSE_SOURCE = 0x1;
    public const int DUPLICATE_SAME_ACCESS = 0x2;
    public const uint STATUS_INFO_LENGTH_MISMATCH = 0xC0000004u;

    [StructLayout(LayoutKind.Sequential)]
    public struct SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX
    {
        public IntPtr Object;
        public IntPtr UniqueProcessId;
        public IntPtr HandleValue;
        public uint GrantedAccess;
        public ushort CreatorBackTraceIndex;
        public ushort ObjectTypeIndex;
        public uint HandleAttributes;
        public uint Reserved;
    }

    public static List<SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX> GetHandlesForPid(int targetPid)
    {
        var result = new List<SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX>();
        int size = 1024 * 1024;  // Start with 1MB
        int returnLength = 0;

        for (int attempt = 0; attempt < 10; attempt++)
        {
            IntPtr buffer = Marshal.AllocHGlobal(size);
            try
            {
                uint status = NtQuerySystemInformation(64, buffer, size, ref returnLength);
                if (status == STATUS_INFO_LENGTH_MISMATCH)
                {
                    size = returnLength + 1024 * 1024;
                    continue;
                }
                if (status != 0)
                {
                    throw new Exception("NtQuerySystemInformation failed: 0x" + status.ToString("X8"));
                }

                // Parse the handle table
                long count = Marshal.ReadInt64(buffer, 0);
                int entrySize = Marshal.SizeOf(typeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX));
                IntPtr entryPtr = IntPtr.Add(buffer, IntPtr.Size * 2);  // Skip NumberOfHandles + Reserved

                for (long i = 0; i < count; i++)
                {
                    var entry = (SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX)Marshal.PtrToStructure(entryPtr, typeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX));
                    if (entry.UniqueProcessId.ToInt64() == targetPid)
                    {
                        result.Add(entry);
                    }
                    entryPtr = IntPtr.Add(entryPtr, entrySize);
                }
                return result;
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }
        throw new Exception("Failed to query system handles after multiple attempts");
    }

    public static string GetHandleName(IntPtr processHandle, IntPtr handleValue)
    {
        IntPtr dupHandle = IntPtr.Zero;
        try
        {
            if (!DuplicateHandle(processHandle, handleValue, GetCurrentProcess(), out dupHandle, 0, false, DUPLICATE_SAME_ACCESS))
                return null;

            int size = 1024;
            IntPtr buffer = Marshal.AllocHGlobal(size);
            try
            {
                int returnLength = 0;
                uint status = NtQueryObject(dupHandle, ObjectNameInformation, buffer, size, ref returnLength);
                if (status != 0)
                    return null;

                // UNICODE_STRING: Length (ushort), MaxLength (ushort), padding, then pointer
                int nameLength = Marshal.ReadInt16(buffer, 0);
                if (nameLength == 0) return null;

                IntPtr namePtr = Marshal.ReadIntPtr(buffer, IntPtr.Size);
                return Marshal.PtrToStringUni(namePtr, nameLength / 2);
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }
        finally
        {
            if (dupHandle != IntPtr.Zero) CloseHandle(dupHandle);
        }
    }

    public static bool CloseRemoteHandle(IntPtr processHandle, IntPtr handleValue)
    {
        IntPtr dummy;
        return DuplicateHandle(processHandle, handleValue, IntPtr.Zero, out dummy, 0, false, DUPLICATE_CLOSE_SOURCE);
    }
}
"@

Write-Output "=== Elden Ring Mutex Killer ==="
Write-Output ""

# Find eldenring.exe process
$procs = Get-Process -Name "eldenring" -ErrorAction SilentlyContinue
if (-not $procs) {
    Write-Output "[ERROR] eldenring.exe is not running!"
    Write-Output "  1. Launch Elden Ring first"
    Write-Output "  2. Wait for title screen"
    Write-Output "  3. Run this script as Administrator"
    exit 1
}

foreach ($proc in $procs) {
    Write-Output "Found eldenring.exe (PID: $($proc.Id))"

    # Open process with DUP_HANDLE permission
    $hProcess = [MutexKiller]::OpenProcess([MutexKiller]::PROCESS_DUP_HANDLE, $false, $proc.Id)
    if ($hProcess -eq [IntPtr]::Zero) {
        Write-Output "[ERROR] Cannot open process. Are you running as Administrator?"
        continue
    }

    try {
        Write-Output "Enumerating handles..."
        $handles = [MutexKiller]::GetHandlesForPid($proc.Id)
        Write-Output "  Found $($handles.Count) handles in process"

        $killed = 0
        foreach ($h in $handles) {
            try {
                $name = [MutexKiller]::GetHandleName($hProcess, $h.HandleValue)
                if ($name -and $name -like "*SekiroMutex*") {
                    Write-Output ""
                    Write-Output "  [FOUND] Handle 0x$($h.HandleValue.ToString('X')) -> $name"
                    Write-Output "  Closing handle..."

                    if ([MutexKiller]::CloseRemoteHandle($hProcess, $h.HandleValue)) {
                        Write-Output "  [SUCCESS] Mutex handle closed!"
                        $killed++
                    } else {
                        Write-Output "  [FAILED] Could not close handle"
                    }
                }
            } catch {
                # Some handles can't be queried, skip silently
            }
        }

        if ($killed -gt 0) {
            Write-Output ""
            Write-Output "=== Done! Killed $killed mutex handle(s) ==="
            Write-Output "You can now launch the second instance of Elden Ring."
        } else {
            Write-Output ""
            Write-Output "[WARNING] SekiroMutex not found in handle table."
            Write-Output "  The mutex may have already been killed, or the game may use a different mechanism."
        }
    }
    finally {
        [MutexKiller]::CloseHandle($hProcess) | Out-Null
    }
}
