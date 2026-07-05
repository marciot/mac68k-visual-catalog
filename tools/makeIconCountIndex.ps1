<#
.SYNOPSIS
    Writes or updates counts in index.csv file

.DESCRIPTION
    For every sub directory in <destination>, writes:

      <subDirName>,<pngCount>

    to:
      <destination>\index.csv
#>

param(
  [Parameter(Mandatory = $true)]
  [string]$DestinationDirectory
)

$IndexFile = Join-Path $DestinationDirectory "index.csv"

# Start a fresh index
if (Test-Path $IndexFile) {
  Remove-Item $IndexFile
}

Get-ChildItem -Path $DestinationDirectory -Directory | ForEach-Object {
  $Subdir = $_
  Write-Host "Processing $Subdir..."
  $Count = (Get-ChildItem -Path $Subdir.FullName -File -Filter "*.png").Count
  Add-Content $IndexFile "$Subdir,$Count"
}