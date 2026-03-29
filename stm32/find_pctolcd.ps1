# find_pctolcd.ps1
# Uses Win32 API to enumerate all child windows and their text
# Works even when UIAutomation can't read Delphi controls
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Collections.Generic;

public class Win32Enum {
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool EnumChildWindows(IntPtr hWndParent, EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    public static extern int GetWindowTextLength(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    public static extern IntPtr FindWindowEx(IntPtr hwndParent, IntPtr hwndChildAfter, string lpszClass, string lpszWindow);

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    public static string GetWindowText(IntPtr hWnd) {
        int len = GetWindowTextLength(hWnd);
        if (len == 0) return "";
        StringBuilder sb = new StringBuilder(len + 1);
        GetWindowText(hWnd, sb, sb.Capacity);
        return sb.ToString();
    }

    public static bool IsMainWindow(IntPtr hWnd) {
        return IsWindowVisible(hWnd);
    }
}
"@

Write-Host "=== Find All PCtoLCD2002 Windows + Controls ==="
Write-Host ""

# Find the main window by class name TfrmMain or title
$hMain = [Win32Enum]::FindWindowEx([IntPtr]::Zero, [IntPtr]::Zero, "TfrmMain", $null)
if ($hMain -eq [IntPtr]::Zero) {
    Write-Host "TfrmMain not found. Searching all windows..."
    # Search all windows
    $found = $false
    $callback = {
        param($hWnd, $lParam)
        $title = [Win32Enum]::GetWindowText($hWnd)
        if ($title -match "PCtoLCD" -or $title -match "pctolcd") {
            $script:found = $true
            Write-Host "  FOUND: hWnd=$hWnd Title='$title'"
        }
        return $true
    }
    # Can't easily do this in a script block. Let's use another approach.
}

# Use pinvoke to enumerate all windows
$allWindows = New-Object 'System.Collections.Generic.List[object]'

# First, find the process
$exePath = "D:\BaiduNetdiskDownload\0.91寸OLED模块4针-SSD1306技术资料（GND）\04-OLED取模教程\01-PCtoLCD2002完美版\PCtoLCD2002.exe"
Write-Host "Starting: $exePath"
$proc = Start-Process $exePath -PassThru
Write-Host "  PID: $($proc.Id)"
Start-Sleep -Seconds 5

# Find window by process ID
function Get-WindowsForProcess {
    param($pid)
    $results = @()
    $callback = {
        param($hWnd, $lParam)
        $pid2 = 0
        [Win32Enum]::GetWindowThreadProcessId($hWnd, [ref]$pid2) | Out-Null
        if ($pid2 -eq $pid) {
            $title = [Win32Enum]::GetWindowText($hWnd)
            $visible = [Win32Enum]::IsWindowVisible($hWnd)
            $results.Add([PSCustomObject]@{Handle=$hWnd; Title=$title; Visible=$visible}) | Out-Null
        }
        return $true
    }

    # Use scriptblock-based enumeration
    $script:results = New-Object 'System.Collections.Generic.List[object]'
    $delegate = [Win32Enum+EnumWindowsProc]$callback

    # We need to do this differently - enumerate all windows and filter by PID
    [Win32Enum]::EnumChildWindows([IntPtr]::Zero, $delegate, [IntPtr]::Zero) | Out-Null
    return $script:results
}

# Alternative: find by class name "TfrmMain" using FindWindowEx
$hWnd = [Win32Enum]::FindWindowEx([IntPtr]::Zero, [IntPtr]::Zero, "TfrmMain", $null)
Write-Host "TfrmMain window handle: $hWnd"

if ($hWnd -ne [IntPtr]::Zero) {
    $title = [Win32Enum]::GetWindowText($hWnd)
    Write-Host "  Title: $title"
    Write-Host ""
    Write-Host "=== Child Windows ==="

    $childResults = New-Object 'System.Collections.Generic.List[object]'
    $childCallback = {
        param($hWnd, $lParam)
        $title = [Win32Enum]::GetWindowText($hWnd)
        $visible = [Win32Enum]::IsWindowVisible($hWnd)
        $script:childResults.Add([PSCustomObject]@{Handle=$hWnd; Title=$title; Visible=$visible}) | Out-Null
        return $true
    }
    $childDelegate = [Win32Enum+EnumWindowsProc]$childCallback

    # Use a helper that captures results
    $script:childResults = New-Object 'System.Collections.Generic.List[object]'

    # Define a custom class for the callback
    Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Management.Automation;

public class EnumHelper {
    public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
    public static List<object> _results = new List<object>();

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr hWndParent, EnumProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern int GetWindowTextLength(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern int GetClassName(IntPtr hWnd, System.Text.StringBuilder lpClassName, int nMaxCount);

    public static string GetText(IntPtr hWnd) {
        int len = GetWindowTextLength(hWnd);
        if (len == 0) return "";
        var sb = new System.Text.StringBuilder(len + 1);
        GetWindowText(hWnd, sb, sb.Capacity);
        return sb.ToString();
    }

    public static string GetClass(IntPtr hWnd) {
        var sb = new System.Text.StringBuilder(256);
        GetClassName(hWnd, sb, sb.Capacity);
        return sb.ToString();
    }
}
"@

    $callback2 = {
        param($hWnd, $lParam)
        $t = [EnumHelper]::GetText($hWnd)
        $c = [EnumHelper]::GetClass($hWnd)
        $v = [EnumHelper]::IsWindowVisible($hWnd)
        [EnumHelper]::_results.Add([PSCustomObject]@{Handle=$hWnd; Title=$t; Class=$c; Visible=$v}) | Out-Null
        return $true
    }

    $delegate2 = [EnumHelper+EnumProc]$callback2
    [EnumHelper]::_results.Clear()
    [EnumHelper]::EnumChildWindows($hWnd, $delegate2, [IntPtr]::Zero) | Out-Null

    foreach ($r in [EnumHelper]::_results) {
        $vmark = if ($r.Visible) { "[V]" } else { "[H]" }
        Write-Host "  $vmark Handle=$($r.Handle) Class='$($r.Class)' Title='$($r.Title)'"
    }
} else {
    Write-Host "TfrmMain not found - trying TMessageForm..."
    $hWnd2 = [Win32Enum]::FindWindowEx([IntPtr]::Zero, [IntPtr]::Zero, "TMessageForm", $null)
    Write-Host "TMessageForm: $hWnd2"
}

Write-Host ""
Write-Host "Done. Closing PCtoLCD2002..."
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
