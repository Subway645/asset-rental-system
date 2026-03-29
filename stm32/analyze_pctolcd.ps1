# analyze_pctolcd.ps1
# 分析 PCtoLCD2002 的 UI 结构
param(
    [string]$exePath = "D:\BaiduNetdiskDownload\0.91寸OLED模块4针-SSD1306技术资料（GND）\04-OLED取模教程\01-PCtoLCD2002完美版\PCtoLCD2002.exe",
    [string]$iniPath = "D:\BaiduNetdiskDownload\0.91寸OLED模块4针-SSD1306技术资料（GND）\04-OLED取模教程\01-PCtoLCD2002完美版\PCtoLCD2002.INI"
)

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

Write-Host "=== PCtoLCD2002 UI 分析脚本 ===" -ForegroundColor Cyan
Write-Host "Exe: $exePath"
Write-Host ""

# 启动进程
Write-Host "[1/3] 启动 PCtoLCD2002..." -ForegroundColor Yellow
$proc = Start-Process $exePath -PassThru
Start-Sleep -Seconds 3

# 查找主窗口
Write-Host "[2/3] 查找主窗口..." -ForegroundColor Yellow
$condition = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, "PCtoLCD2002 完美版 2002")
$root = [System.Windows.Automation.AutomationElement]::RootElement
$window = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $condition)

if (-not $window) {
    Write-Host "  尝试用窗口类名找..." -ForegroundColor Gray
    $condition2 = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ClassNameProperty, "TfrmMain")
    $window = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $condition2)
}

if (-not $window) {
    Write-Host "  尝试用类名 'TApplication'..." -ForegroundColor Gray
    $condition3 = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ClassNameProperty, "TApplication")
    $window = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $condition3)
}

if (-not $window) {
    Write-Host "  尝试找所有顶层窗口..." -ForegroundColor Gray
    $allWindows = @()
    $root.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition) | ForEach-Object {
        $name = $_.Current.Name
        $class = $_.Current.ClassName
        if ($name -and $class) {
            $allWindows += "$class : $name"
        }
    }
    Write-Host "  找到的窗口：" -ForegroundColor Gray
    $allWindows | Select-Object -First 20 | ForEach-Object { Write-Host "    $_" -ForegroundColor Gray }
    $window = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition)
}

Write-Host "  找到主窗口: $($window.Current.Name)" -ForegroundColor Green
Write-Host "  类名: $($window.Current.ClassName)" -ForegroundColor Green
Write-Host ""

# 递归打印所有子控件
Write-Host "[3/3] 遍历所有子控件..." -ForegroundColor Yellow
function Get-UIChildren {
    param($element, $depth = 0, $maxDepth = 5)
    if ($depth -gt $maxDepth) { return }
    try {
        $walker = [System.Windows.Automation.Cache]::RawViewWalker
        $child = $walker.GetFirstChild($element)
        while ($child) {
            $name = $child.Current.Name
            $class = $child.Current.ClassName
            $control = $child.Current.ControlType
            $id = $child.Current.AutomationId
            if ($name -or $class) {
                $indent = "  " * $depth
                Write-Host "$indent[$($control.ProgrammaticName)] Name='$name' Class='$class' ID='$id'"
            }
            Get-UIChildren -element $child -depth ($depth + 1) -maxDepth $maxDepth
            $child = $walker.GetNextSibling($child)
        }
    } catch {
        # ignore
    }
}

Get-UIChildren -element $window -depth 0 -maxDepth 6

Write-Host ""
Write-Host "分析完成！按回车关闭程序..." -ForegroundColor Cyan
Read-Host
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
