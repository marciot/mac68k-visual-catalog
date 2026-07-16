<#
.SYNOPSIS
    Packs and indexes icon database for deployment

.DESCRIPTION
    On input:

      <sourceDirectory>
        _0
          _0AP65rF0VbfFH2xcZADkWayP4Y.png
          _0AP65rF0VbfFH2xcZADkWayP4Y.csv
        _e
          _e2WnnRz1UPstn9ZeiRlqZQUf-0.png
          _e2WnnRz1UPstn9ZeiRlqZQUf-0.csv

    On output:

      <outputDirectory>
        _0.tgz
        _e.tgz
        index.csv

    Compresses every sub directory of the source directory to:

      <destinationDirectory>\<subDirName>.tgz

    Also writes an index with entries:

      <subDirName>,<pngCount>

    to:
      <destinationDirectory>\index.csv

#>

param(
  [Parameter(Mandatory = $true)]
  [string]$SourceDirectory,

  [Parameter(Mandatory = $true)]
  [string]$DestinationDirectory
)

# Ensure destination directory exists
if (-not (Test-Path $DestinationDirectory)) {
  New-Item -ItemType Directory -Path $DestinationDirectory | Out-Null
}

# Check for case sensitivity so directories like "_A" and "_a" can coexist.
$query = & fsutil.exe file queryCaseSensitiveInfo $DestinationDirectory 2>&1

if ($LASTEXITCODE -eq 0 -and $query -notmatch 'Enabled') {
  Write-Host @"
Case sensitivity not enabled for target directory. Please run the following as administrator:
        
fsutil.exe file setCaseSensitiveInfo "$DestinationDirectory" enable
"@
  return
}

$indexFile = Join-Path $DestinationDirectory "index.csv"
$resolvedTar = (Get-Command "tar.exe" -ErrorAction Stop).Source

# Remove existing index if present
if (Test-Path $indexFile) {
  Remove-Item $indexFile
}

Get-ChildItem -Path $SourceDirectory -Directory | ForEach-Object {
  $subdir = $_

  Write-Host "Processing $Subdir..."

  $tgzPath = Join-Path (Resolve-Path -Path $DestinationDirectory).Path ($subdir.Name + ".tgz")

  # Remove existing archive if present
  if (Test-Path $tgzPath) {
    Remove-Item $tgzPath
  }

  # Compress the contents of the directory (not the directory itself)
  $args = "-zcf",$tgzPath,"-C",$subdir.FullName,"*"
  & $resolvedTar @args

  if ($LASTEXITCODE) {
    throw "tar failed with exit code $LASTEXITCODE."
  }

  # Count files recursively
  $fileCount = (Get-ChildItem -Path $subdir.FullName -File -Filter "*.png").Count

  # Append a CSV row
  "$($subdir.Name),$fileCount" | Add-Content $indexFile
}