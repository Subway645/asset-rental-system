# start_and_analyze.ps1
# Starts PCtoLCD2002 then dumps its UI tree
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

Write-Host "=== PCtoLCD2002 Launcher + Analyzer ==="

# Find the PCtoLCD2002 directory by looking for PCtoLCD2002.ini
$searchRoots = @(
    "$env:USERPROFILE\Desktop",
    "$env:USERPROFILE\Downloads",
    "D:\BaiduNetdiskDownload"
)
$foundPath = $null
foreach ($root in $searchRoots) {
    if (Test-Path $root) {
        $result = Get-ChildItem $root -Recurse -Filter "PCtoLCD2002.ini" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($result) {
            $foundPath = $result.FullName
            break
        }
    }
}

if (-not $foundPath) {
    Write-Host "[ERROR] PCtoLCD2002.ini not found anywhere"
    exit 1
}

$exePath = $foundPath.Replace("PCtoLCD2002.INI", "PCtoLCD2002.exe")
Write-Host "Found exe at: $exePath"
if (-not (Test-Path $exePath)) {
    Write-Host "[ERROR] Exe not found: $exePath"
    exit 1
}

Write-Host "[1/4] Starting PCtoLCD2002..."
$proc = Start-Process $exePath -PassThru
Write-Host "  PID: $($proc.Id)"
Start-Sleep -Seconds 4

Write-Host "[2/4] Waiting for window..."
Start-Sleep -Seconds 3

$root = [System.Windows.Automation.AutomationElement]::RootElement
# Try multiple conditions
$condName = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, "PCtoLCD2002")
$condClass = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ClassNameProperty, "TfrmMain")
$condOr = New-Object System.Windows.Automation.OrCondition @($condName, $condClass)
$windows = $root.FindAll([System.Windows.Automation.TreeScope]::Children, $condOr)
Write-Host "  Found $($windows.Count) window(s)"

if ($windows.Count -eq 0) {
    Write-Host "[WARN] No windows found. Trying to find all windows:"
    $all = $root.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($w in $all) {
        $n = $w.Current.Name
        $c = $w.Current.ClassName
        if ($n -match "PCtoLCD" -or $c -match "PCtoLCD" -or $c -eq "TfrmMain") {
            Write-Host "  MATCH: Name='$n' Class='$c' PID=$($w.Current.ProcessId)"
        }
    }
}

foreach ($w in $windows) {
    Write-Host ""
    Write-Host "=========================================="
    Write-Host "  Name:  $($w.Current.Name)"
    Write-Host "  Class: $($w.Current.ClassName)"
    Write-Host "  PID:   $($w.Current.ProcessId)"
    Write-Host "=========================================="

    function Dump-Controls {
        param($elem, $depth = 0, $maxD = 8)
        if ($depth -gt $maxD) { return }
        try {
            $walker = [System.Windows.Automation.Cache]::RawViewWalker
            $child = $walker.GetFirstChild($elem)
            while ($child) {
                $name = $child.Current.Name
                $class = $child.Current.ClassName
                $ctrl = $child.Current.ControlType
                $id = $child.Current.AutomationId
                $rect = $child.Current.BoundingRectangle
                $isKeyboardFocusable = $child.Current.IsKeyboardFocusable
                if ($name -or $class) {
                    $ind = "  " * $depth
                    $pos = "X=$([int]$rect.X) Y=$([int]$rect.Y) W=$([int]$rect.Width) H=$([int]$rect.Height)"
                    $flag = if ($isKeyboardFocusable) { "[Focusable]" } else { "" }
                    Write-Host "$ind[$($ctrl.ProgrammaticName)] Name='$name' Class='$class' ID='$id' $pos $flag"
                }
                Dump-Controls -elem $child -depth ($depth + 1) -maxD $maxD
                $child = $walker.GetNextSibling($child)
            }
        } catch {
            Write-Host "$('  ' * $depth)[ERROR] $_"
        }
    }

    Write-Host ""
    Write-Host "[Control Tree]"
    Dump-Controls -elem $w -depth 0 -maxD 8
}

Write-Host ""
Write-Host "[3/4] Done analyzing. Killing process..."
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

Write-Host "[4/4] Complete."
