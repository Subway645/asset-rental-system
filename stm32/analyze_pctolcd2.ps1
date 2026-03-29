# analyze_pctolcd2.ps1
# Analyze PCtoLCD2002 UI structure via UIAutomation
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

Write-Host "=== PCtoLCD2002 UI Analysis ==="

$root = [System.Windows.Automation.AutomationElement]::RootElement

# Find PCtoLCD2002 window by various possible identifiers
$condName = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, "PCtoLCD2002")
$condClass = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ClassNameProperty, "TfrmMain")
$condOr = New-Object System.Windows.Automation.OrCondition @($condName, $condClass)

$windows = $root.FindAll([System.Windows.Automation.TreeScope]::Children, $condOr)
Write-Host "Found $($windows.Count) matching window(s)"

foreach ($w in $windows) {
    Write-Host ""
    Write-Host "  Window: $($w.Current.Name)"
    Write-Host "  Class:  $($w.Current.ClassName)"
    Write-Host "  PID:    $($w.Current.ProcessId)"
    Write-Host ""

    function Get-UIChildren {
        param($elem, $depth = 0, $maxD = 7)
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
                if ($name -or $class) {
                    $ind = "    " * $depth
                    $pos = "X=$($rect.X) Y=$($rect.Y) W=$($rect.Width) H=$($rect.Height)"
                    Write-Host "$ind[$($ctrl.ProgrammaticName)] Name='$name' Class='$class' ID='$id' $pos"
                }
                Get-UIChildren -elem $child -depth ($depth + 1) -maxD $maxD
                $child = $walker.GetNextSibling($child)
            }
        } catch {}
    }

    Write-Host "  [Control Tree]"
    Get-UIChildren -elem $w -depth 0 -maxD 7
}

Write-Host ""
Write-Host "Done."
