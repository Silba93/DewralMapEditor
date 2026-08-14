Start-Sleep -Seconds 5

$codexPackageMarker = "OpenAI.Codex_"
$desktopProcesses = Get-Process -Name ChatGPT -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -like "*WindowsApps*$codexPackageMarker*" }

$desktopExecutable = $desktopProcesses |
    Select-Object -First 1 -ExpandProperty Path

if (-not $desktopExecutable) {
    $desktopExecutable = Get-Process -Name codex -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -like "*WindowsApps*$codexPackageMarker*resources*" } |
        Select-Object -First 1 -ExpandProperty Path
    if ($desktopExecutable) {
        $desktopExecutable = Join-Path (Split-Path (Split-Path $desktopExecutable -Parent) -Parent) "ChatGPT.exe"
    }
}

$desktopProcesses | Stop-Process -Force
Start-Sleep -Seconds 3

if ($desktopExecutable -and (Test-Path -LiteralPath $desktopExecutable)) {
    Start-Process -FilePath $desktopExecutable
}
