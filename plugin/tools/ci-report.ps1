# Shows the important lines of a failed CI step as notes on the GitHub run page
# (visible without signing in). Usage: ./plugin/tools/ci-report.ps1 <logfile>
param([string]$Log)
if (-not (Test-Path $Log)) { exit 0 }
$lines = Get-Content $Log
$errs = $lines | Where-Object { $_ -match 'error|Error|ERROR|FAIL|fatal' } | Select-Object -First 40
$tail = $lines | Select-Object -Last 40
foreach ($chunk in @($errs, $tail)) {
  if (-not $chunk) { continue }
  $text = ($chunk -join "`n").Replace('%', '%25').Replace("`r", '').Replace("`n", '%0A')
  if ($text.Length -gt 3800) { $text = $text.Substring(0, 3800) }
  Write-Output "::error title=$([IO.Path]::GetFileName($Log))::$text"
}
