# dump_ui.ps1
# Dumps PCtoLCD2002 UI tree with TMessageForm class
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

Write-Host "=== PCtoLCD2002 UI Dump ==="
Write-Host ""

$root = [System.Windows.Automation.AutomationElement]::RootElement

# Find by TMessageForm class
$condClass = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ClassNameProperty, "TMessageForm")
$windows = $root.FindAll([System.Windows.Automation.TreeScope]::Children, $condClass)
Write-Host "Found $($windows.Count) TMessageForm window(s)"

foreach ($w in $windows) {
    $name = $w.Current.Name
    if ($name -notmatch "Pctolcd2002") { continue }
    Write-Host ""
    Write-Host "=============================================="
    Write-Host "  Name:  $($w.Current.Name)"
    Write-Host "  Class: $($w.Current.ClassName)"
    Write-Host "  PID:   $($w.Current.ProcessId)"
    Write-Host "=============================================="

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
                $isFocusable = $child.Current.IsKeyboardFocusable
                if ($name -or $class) {
                    $ind = "  " * $depth
                    $pos = "X=$([int]$rect.X) Y=$([int]$rect.Y) W=$([int]$rect.Width) H=$([int]$rect.Height)"
                    $f = if ($isFocusable) { "[F]" } else { "" }
                    Write-Host "$ind[$($ctrl.ProgrammaticName)] Name='$name' Class='$class' ID='$id' $pos $f"
                }
                Dump-Controls -elem $child -depth ($depth + 1) -maxD $maxD
                $child = $walker.GetNextSibling($child)
            }
        } catch {
            Write-Host "$('  ' * $depth)[ERROR: $_]"
        }
    }

    Write-Host ""
    Dump-Controls -elem $w -depth 0 -maxD 8
}

Write-Host ""
Write-Host "Done."
