<#
.SYNOPSIS
    Scans subdirectories of $sourceDirectory. Anytime a filename of the form
    $hash.png is found, if another file with an uppercase extension also exists,
    e.g. $hash.PNG, then "$hash.png" is moved to $destinationDirectory.
    
#>

param(
  [Parameter(Mandatory = $true)]
  [string]$SourceDirectory,

  [Parameter(Mandatory = $true)]
  [string]$DestinationDirectory
)

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

Get-ChildItem -Path $SourceDirectory -File -Recurse | ForEach-Object {
  $file = $_

  if ($file.Extension -ceq ".png") {
    $upperCaseFile = Join-Path $file.DirectoryName ($file.BaseName + ".PNG")

    if (Test-Path -LiteralPath $upperCaseFile) {
      Move-Item -LiteralPath $file.FullName `
                -Destination (Join-Path $DestinationDirectory $file.Name)
    }
  }
}