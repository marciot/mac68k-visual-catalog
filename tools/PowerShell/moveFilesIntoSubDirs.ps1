<#
.SYNOPSIS
    Moves files in to their correct prefix subdirectories. This
    tool operates recursively on the source directory, so it can
    be used to merge two icon databases into one.

.DESCRIPTION
    On input:

      <sourceDirectory>:
        _0AP65rF0VbfFH2xcZADkWayP4Y.png
        _0AP65rF0VbfFH2xcZADkWayP4Y.csv
        _e2WnnRz1UPstn9ZeiRlqZQUf-0.png
        _e2WnnRz1UPstn9ZeiRlqZQUf-0.csv

    On output:

      <destinationDirectory>
        _0
          _0AP65rF0VbfFH2xcZADkWayP4Y.png
          _0AP65rF0VbfFH2xcZADkWayP4Y.csv
        _e
          _e2WnnRz1UPstn9ZeiRlqZQUf-0.png
          _e2WnnRz1UPstn9ZeiRlqZQUf-0.csv

#>

param(
  [Parameter(Mandatory = $true)]
  [string]$SourceDirectory,

  [Parameter(Mandatory = $true)]
  [string]$DestinationDirectory,

  [switch]$Overwrite
)

# Prompt only when double-clicked from Explorer
if ($args.Count -eq 0 -and -not $PSBoundParameters.ContainsKey('Overwrite')) {
  $response = Read-Host "Overwrite existing files? (Y/N)"

  if ($response -match '^(y|yes)$') {
      $Overwrite = $true
  }
}

# Ensure the destination root exists
if (-not (Test-Path -LiteralPath $DestinationDirectory)) {
  New-Item -ItemType Directory -Path $DestinationDirectory | Out-Null
}

# Ensure the destination root exists
if (-not (Test-Path -LiteralPath $DestinationDirectory)) {
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

# Find all files in the SourceDirectory and merge them into the
# proper places in the proper subdirectories of DestinationDirectory

Get-ChildItem -Path $SourceDirectory -File -Recurse | ForEach-Object {
  $file = $_

  # Use the filename without its path
  $name = $file.Name

  # Determine the subdirectory name
  if ($name.Length -ge 2) {
    $subdir = $name.Substring(0, 2)
  } else {
    $subdir = $name
  }

  $targetDir = Join-Path $DestinationDirectory $subdir

  # Create the subdirectory if needed
  if (-not (Test-Path -LiteralPath $targetDir)) {
    New-Item -ItemType Directory -Path $targetDir | Out-Null
  }

  $targetFile = Join-Path $targetDir $name

  if (Test-Path -LiteralPath $targetFile) {
    if ($Overwrite) {
      Move-Item -LiteralPath $file.FullName -Destination $targetFile -Force
      Write-Host "Overwrote '$targetFile' with '$($file.FullName)'"
    } else {
      Write-Warning "Skipping '$($file.FullName)' because '$targetFile' already exists."
    }
  } else {
    Move-Item -LiteralPath $file.FullName -Destination $targetFile
    Write-Host "Moved '$($file.FullName)' -> '$targetFile'"
  }
}