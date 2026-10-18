param([string]$Ids='')
$ErrorActionPreference='Stop'
$wanted=@($Ids.Split(',') | ForEach-Object {[int]$_})
$null=Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine
Start-Sleep -Milliseconds 1100
$rows=@(Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine | Where-Object {$_.Name -match 'pid_(\d+)_' -and $wanted -contains [int]$Matches[1]} | Select-Object Name,UtilizationPercentage)
ConvertTo-Json -InputObject @($rows) -Compress
