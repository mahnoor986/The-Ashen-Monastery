$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$exePath = Join-Path $projectRoot "AshenMonastery.exe"
$assetsPath = Join-Path $projectRoot "assets"
$distPath = Join-Path $projectRoot "dist"
$zipPath = Join-Path $distPath "AshenMonastery_Windows.zip"
$stagePath = Join-Path $env:TEMP ("AshenMonastery-release-" + [Guid]::NewGuid().ToString("N"))

if (-not (Test-Path $exePath -PathType Leaf)) {
    throw "Release executable not found: $exePath"
}
if (-not (Test-Path $assetsPath -PathType Container)) {
    throw "Game assets folder not found: $assetsPath"
}

New-Item -ItemType Directory -Path $distPath -Force | Out-Null
New-Item -ItemType Directory -Path $stagePath | Out-Null

try {
    Copy-Item $exePath $stagePath
    Copy-Item $assetsPath $stagePath -Recurse
    Copy-Item (Join-Path $projectRoot "README.md") $stagePath
    Copy-Item (Join-Path $projectRoot "CREDITS.md") $stagePath

    @"
THE ASHEN MONASTERY

1. Extract this entire ZIP folder.
2. Open the extracted folder.
3. Double-click AshenMonastery.exe.

"Windows protected your PC"?
  This is normal for games made by students and small developers: the exe is
  not signed with a paid certificate, so Windows does not recognise it yet.
  It is not a virus warning. Click "More info", then "Run anyway".
  Tip: to avoid the message completely, right-click the downloaded ZIP BEFORE
  extracting -> Properties -> tick "Unblock" -> OK, then extract it.

Keep the assets folder next to AshenMonastery.exe. The game loads its maps,
shaders, fonts, textures and sounds from that folder.
"@ | Set-Content (Join-Path $stagePath "PLAY.txt") -Encoding ASCII

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    if (Test-Path $zipPath -PathType Leaf) {
        Remove-Item $zipPath -Force
    }
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $stagePath,
        $zipPath,
        [System.IO.Compression.CompressionLevel]::Optimal,
        $false
    )
    Write-Host "Created $zipPath"
}
finally {
    Remove-Item $stagePath -Recurse -Force
}
