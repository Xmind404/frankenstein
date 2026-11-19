$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$repo = 'Xmind404/frankenstein'
$asset = 'frankenstein-windows-x86_64.zip'
$base = "https://github.com/$repo/releases/latest/download"
$temporaryDirectory = Join-Path $env:TEMP ("frankenstein-install-" + [guid]::NewGuid().ToString('N'))
$archive = Join-Path $temporaryDirectory $asset
$checksums = Join-Path $temporaryDirectory 'SHA256SUMS'
$extract = Join-Path $temporaryDirectory 'extracted'
$installDir = if ($env:FRANKENSTEIN_INSTALL_DIR) { $env:FRANKENSTEIN_INSTALL_DIR } else { Join-Path $env:LOCALAPPDATA 'Frankenstein\bin' }

Write-Host '  _____                      __                           __         .__        ' -ForegroundColor DarkGreen
Write-Host '_/ ____\___________    ____ |  | __ ____   ____   _______/  |_  ____ |__| ____  ' -ForegroundColor Green
Write-Host '\   __\\_  __ \__  \  /    \|  |/ // __ \ /    \ /  ___/\   __\/ __ \|  |/    \ ' -ForegroundColor Green
Write-Host ' |  |   |  | \// __ \|   |  \    <\  ___/|   |  \\___ \  |  | \  ___/|  |   |  \' -ForegroundColor Cyan
Write-Host ' |__|   |__|  (____  /___|  /__|_ \___  >___|  /____  > |__|  \___  >__|___|  /' -ForegroundColor DarkCyan
Write-Host '                   \/     \/     \/    \/     \/     \/            \/        \/' -ForegroundColor Blue
Write-Host "`nFrankenstein installer`n"

try {
    New-Item -ItemType Directory -Path $temporaryDirectory -Force | Out-Null
    Invoke-WebRequest "$base/$asset" -OutFile $archive
    Invoke-WebRequest "$base/SHA256SUMS" -OutFile $checksums
    $line = Get-Content $checksums | Where-Object { $_ -match "\s+$([regex]::Escape($asset))$" } | Select-Object -First 1
    if (-not $line) { throw "No checksum entry found for $asset" }
    $expected = ($line -split '\s+')[0].ToLowerInvariant()
    $actual = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) { throw 'SHA-256 verification failed.' }

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
    Write-Host 'Updated the user PATH environment variable.'
    Write-Host 'Run: frankenstein --help'
} finally {
    Remove-Item $temporaryDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
