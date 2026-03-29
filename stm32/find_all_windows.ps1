# find_all_windows.ps1
# Finds ALL windows and prints their names and classes
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

Write-Host "=== All Windows ==="
$root = [System.Windows.Automation.AutomationElement]::RootElement
$all = $root.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition)
Write-Host "Total windows: $($all.Count)"
Write-Host ""
foreach ($w in $all) {
    $n = $w.Current.Name
    $c = $w.Current.ClassName
    $p = $w.Current.ProcessId
    if ($n -or $c) {
        Write-Host "[PID=$p] Class='$c' Name='$n'"
    }
}
Write-Host ""
Write-Host "Done."
