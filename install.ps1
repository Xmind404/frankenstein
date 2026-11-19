$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$repo = 'Xmind404/frankenstein'
$asset = 'frankenstein-windows-x86_64.zip'
$base = "https://github.com/$repo/releases/latest/download"
$installDir = if ($env:FRANKENSTEIN_INSTALL_DIR) { $env:FRANKENSTEIN_INSTALL_DIR } else { Join-Path $env:LOCALAPPDATA 'Frankenstein\bin' }
$binary = Join-Path $installDir 'frankenstein.exe'
$temporaryDirectory = Join-Path $env:TEMP ("frankenstein-install-" + [guid]::NewGuid().ToString('N'))
$archive = Join-Path $temporaryDirectory $asset
$checksums = Join-Path $temporaryDirectory 'SHA256SUMS'
$extract = Join-Path $temporaryDirectory 'extracted'

function Show-Logo {
  Write-Host '  _____                      __                           __         .__        ' -ForegroundColor DarkGreen
  Write-Host '_/ ____\___________    ____ |  | __ ____   ____   _______/  |_  ____ |__| ____  ' -ForegroundColor Green
  Write-Host '\   __\\_  __ \__  \  /    \|  |/ // __ \ /    \ /  ___/\   __\/ __ \|  |/    \ ' -ForegroundColor Green
  Write-Host ' |  |   |  | \// __ \|   |  \    <\  ___/|   |  \___ \  |  | \  ___/|  |   |  \' -ForegroundColor Cyan
  Write-Host ' |__|   |__|  (____  /___|  /__|_ \___  >___|  /____  > |__|  \___  >__|___|  /' -ForegroundColor DarkCyan
  Write-Host '                   \/     \/     \/    \/     \/     \/            \/        \/' -ForegroundColor Blue
  Write-Host "`nFrankenstein installer`n"
}

function Write-ItemLine([string]$Message) { Write-Host "◆ $Message" -ForegroundColor Green }

function Read-YesNo([string]$Question) {
  while ($true) {
    $answer = (Read-Host "◆ $Question [Y/N]").Trim()
    if ($answer -match '^[Yy]$') { return $true }
    if ($answer -match '^[Nn]$') { return $false }
    Write-Host '│ Please enter Y or N.' -ForegroundColor DarkGray
  }
}

function Select-Action([string[]]$Options) {
  $selected = 0
  while ($true) {
    Clear-Host
    Show-Logo
    Write-Host "`n◆ Choose an action:" -ForegroundColor Green
    for ($i = 0; $i -lt $Options.Count; $i++) {
      if ($i -eq $selected) { Write-Host "● $($Options[$i])" -ForegroundColor Green }
      else { Write-Host "○ $($Options[$i])" -ForegroundColor DarkGray }
    }
    Write-Host '└  ↑/↓ to navigate • Enter: confirm' -ForegroundColor DarkGray
    $key = [Console]::ReadKey($true)
    if ($key.Key -eq [ConsoleKey]::Up -and $selected -gt 0) { $selected-- }
    elseif ($key.Key -eq [ConsoleKey]::Down -and $selected -lt ($Options.Count - 1)) { $selected++ }
    elseif ($key.Key -eq [ConsoleKey]::Enter) { return $selected }
  }
}

function Remove-InstallPath {
  $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
  $entries = @($userPath -split ';' | Where-Object { $_ -and $_.TrimEnd('\') -ine $installDir.TrimEnd('\') })
  [Environment]::SetEnvironmentVariable('Path', ($entries -join ';'), 'User')
  $currentEntries = @($env:Path -split ';' | Where-Object { $_ -and $_.TrimEnd('\') -ine $installDir.TrimEnd('\') })
  $env:Path = $currentEntries -join ';'
}

Show-Logo
$installed = Test-Path $binary
$action = if ($env:FRANKENSTEIN_ACTION) { $env:FRANKENSTEIN_ACTION.ToLowerInvariant() } else { '' }
if (-not $action) {
  if ($installed) {
    $choice = Select-Action @('Reinstall', 'Uninstall', 'Exit')
    if ($choice -eq 0) { $action = 'reinstall' }
    elseif ($choice -eq 1) { $action = 'uninstall' }
    else { Write-ItemLine 'Cancelled.'; exit 0 }
  } else {
    $choice = Select-Action @('Install', 'Exit')
    if ($choice -eq 0) { $action = 'install' }
    else { Write-ItemLine 'Cancelled.'; exit 0 }
  }
}
if ($action -notin @('install', 'reinstall', 'uninstall')) { throw "Invalid FRANKENSTEIN_ACTION: $action" }
if (-not (Read-YesNo "Continue with $action?")) { Write-ItemLine 'Cancelled.'; exit 0 }

if ($action -eq 'uninstall') {
  Remove-Item $binary -Force -ErrorAction SilentlyContinue
  Remove-InstallPath
  Write-ItemLine "Uninstalled Frankenstein from $installDir"
  Write-ItemLine 'Removed the managed install directory from the current user PATH.'
  Write-ItemLine 'Open a new terminal to apply the change.'
  exit 0
}

try {
  New-Item -ItemType Directory -Path $temporaryDirectory -Force | Out-Null
  try {
    Invoke-WebRequest "$base/$asset" -OutFile $archive
  } catch {
    throw "No downloadable GitHub Release asset found. Publish a release first: https://github.com/$repo/releases"
  }
  try {
    Invoke-WebRequest "$base/SHA256SUMS" -OutFile $checksums
  } catch {
    throw 'Release is missing SHA256SUMS; refusing to install an unverified binary.'
  }
  $line = Get-Content $checksums | Where-Object { $_ -match "\s+$([regex]::Escape($asset))$" } | Select-Object -First 1
  if (-not $line) { throw "No checksum entry found for $asset" }
  $expected = ($line -split '\s+')[0].ToLowerInvariant()
  $actual = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
  if ($actual -ne $expected) { throw 'SHA-256 verification failed.' }

  Expand-Archive $archive -DestinationPath $extract -Force
  $extractedBinary = Join-Path $extract 'frankenstein.exe'
  if (-not (Test-Path $extractedBinary)) { throw 'Release archive does not contain frankenstein.exe.' }
  New-Item -ItemType Directory -Path $installDir -Force | Out-Null
  Copy-Item $extractedBinary $binary -Force
  Write-ItemLine "Installed Frankenstein in $binary"

  if (Read-YesNo 'Add the install directory to the user PATH automatically?') {
    $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    $entries = @($userPath -split ';' | Where-Object { $_ })
    if (-not ($entries | Where-Object { $_.TrimEnd('\') -ieq $installDir.TrimEnd('\') })) {
      [Environment]::SetEnvironmentVariable('Path', (($entries + $installDir) -join ';'), 'User')
    }
    if (-not ($env:Path -split ';' | Where-Object { $_.TrimEnd('\') -ieq $installDir.TrimEnd('\') })) { $env:Path = "$installDir;$env:Path" }
    Write-ItemLine 'Updated the current user PATH.'
    Write-Host '│ Open a new PowerShell window to use the updated PATH.'
  } else {
    Write-ItemLine "Add this directory to your user PATH: $installDir"
    Write-Host '│ Windows: Settings → System → About → Advanced system settings → Environment Variables → User variables → Path → Edit → New.'
    Write-Host '│ Add the directory above, save, and open a new terminal.'
  }
  Write-ItemLine 'After PATH is available, run: frankenstein --help'
} finally {
  Remove-Item $temporaryDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
