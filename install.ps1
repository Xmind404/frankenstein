$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$repo = 'Xmind404/frankenstein'
$asset = 'frankenstein-windows-x86_64.zip'
$base = "https://github.com/$repo/releases/latest/download"
$archive = Join-Path $env:TEMP $asset
$checksums = Join-Path $env:TEMP 'frankenstein-SHA256SUMS'
$extract = Join-Path $env:TEMP 'frankenstein-install'
$installDir = if ($env:FRANKENSTEIN_INSTALL_DIR) { $env:FRANKENSTEIN_INSTALL_DIR } else { Join-Path $env:LOCALAPPDATA 'Frankenstein\bin' }

Invoke-WebRequest "$base/$asset" -OutFile $archive
Invoke-WebRequest "$base/SHA256SUMS" -OutFile $checksums
$line = Get-Content $checksums | Where-Object { $_ -match "\s+$([regex]::Escape($asset))$" } | Select-Object -First 1
if (-not $line) { throw "No checksum entry found for $asset" }
$expected = ($line -split '\s+')[0].ToLowerInvariant()
$actual = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne $expected) { throw 'SHA-256 verification failed.' }

if (Test-Path $extract) { Remove-Item $extract -Recurse -Force }
Expand-Archive $archive -DestinationPath $extract -Force
New-Item -ItemType Directory -Path $installDir -Force | Out-Null
Copy-Item (Join-Path $extract 'frankenstein.exe') (Join-Path $installDir 'frankenstein.exe') -Force

$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
$pathEntries = @($userPath -split ';' | Where-Object { $_ })
if ($pathEntries -notcontains $installDir) {
    [Environment]::SetEnvironmentVariable('Path', (($pathEntries + $installDir) -join ';'), 'User')
}
$env:Path = "$installDir;$env:Path"
Write-Host "Installed Frankenstein in $installDir"
Write-Host "Run: frankenstein --help"

Remove-Item $archive, $checksums -Force -ErrorAction SilentlyContinue
Remove-Item $extract -Recurse -Force -ErrorAction SilentlyContinue
