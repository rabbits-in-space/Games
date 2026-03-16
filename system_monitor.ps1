# === CONFIGURATION ===
$logPath = "$env:USERPROFILE\Desktop\SystemMonitorLog.csv"
$intervalMinutes = 5

# === HEADER ===
if (!(Test-Path $logPath)) {
    "Timestamp,CPU (%),Memory Used (MB),Memory Total (MB),Disk Free (GB),Disk Total (GB)" | Out-File $logPath
}

# === MONITOR LOOP ===
while ($true) {
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

    # CPU usage
    $cpu = (Get-Counter '\Processor(_Total)\% Processor Time').CounterSamples[0].CookedValue

    # Memory usage
    $mem = Get-CimInstance Win32_OperatingSystem
    $memUsed = [math]::Round(($mem.TotalVisibleMemorySize - $mem.FreePhysicalMemory) / 1024, 2)
    $memTotal = [math]::Round($mem.TotalVisibleMemorySize / 1024, 2)

    # Disk usage (C: drive)
    $disk = Get-PSDrive C
    $diskFree = [math]::Round($disk.Free / 1GB, 2)
    $diskTotal = [math]::Round($disk.Used + $disk.Free / 1GB, 2)

    # Log entry
    "$timestamp,$([math]::Round($cpu,2)),$memUsed,$memTotal,$diskFree,$diskTotal" | Out-File $logPath -Append

    # Wait before next check
    Start-Sleep -Seconds ($intervalMinutes * 60)
}
