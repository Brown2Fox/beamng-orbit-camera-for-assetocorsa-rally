param(
    [string]$UE4SSArchiveDir = "D:\DevTools\UE4SS"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path $PSScriptRoot -Parent
$BuildDir = Join-Path $RepoRoot "build\Shipping"
$Config = "Game__Shipping__Win64"
$BuiltDll = Join-Path $BuildDir "Source\$Config\BeamNGOrbitCamera.dll"
if (-not (Test-Path -LiteralPath $BuiltDll -PathType Leaf)) {
    throw "Shipping DLL is missing. Run .\tools\build.ps1 Shipping before packaging."
}

# Supported non-development distribution: UE4SS_v3.0.1-1127-g2bfa839f.zip.
$ExpectedHash = "29367CE89F3637A537D507F2C79B9D33DF3D145C63FD8BB0179F737A5CE46374"
$UE4SSArchive = Get-ChildItem -LiteralPath $UE4SSArchiveDir -Filter '*.zip' -File |
    Where-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -eq $ExpectedHash } |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $UE4SSArchive) {
    throw "Supported release UE4SS archive was not found in '$UE4SSArchiveDir'. Required SHA256: $ExpectedHash"
}
Write-Host "UE4SS distribution: $UE4SSArchive"

$ModSource = Get-Content -LiteralPath (Join-Path $RepoRoot "Source\BeamNGOrbitCameraMod.cpp") -Raw
if ($ModSource -notmatch 'ModVersion\s*=\s*STR\("([0-9]+\.[0-9]+\.[0-9]+)"\)') {
    throw "Could not determine the mod version."
}
$Version = $Matches[1]
$Win64 = "acr/Binaries/Win64"
$Runtime = "$Win64/ue4ss"
$Mod = "$Runtime/Mods/BeamNGOrbitCamera"
$Files = [ordered]@{
    "$Mod/dlls/main.dll" = $BuiltDll
    "$Mod/Data/config.ini" = (Join-Path $RepoRoot "Data\config.ini")
    "README.md" = (Join-Path $RepoRoot "res\SHORT_README_FOR_PACKAGING.md")
}
foreach ($Extension in @("pak", "ucas", "utoc")) {
    $Name = "BeamNGOrbitModifier.$Extension"
    $Files["acr/Content/Paks/$Name"] = Join-Path $RepoRoot "Data\$Name"
}
foreach ($Source in $Files.Values) {
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Required package file is missing: '$Source'."
    }
}

$OutputDir = Join-Path $RepoRoot "dist"
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$ArchivePath = Join-Path $OutputDir "BeamNGOrbitCamera-v$Version.zip"
# CreateNew protects an existing release archive from accidental replacement.
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$Stream = [System.IO.File]::Open($ArchivePath, [System.IO.FileMode]::CreateNew)
try {
    $Archive = [System.IO.Compression.ZipArchive]::new($Stream, [System.IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        $Distribution = [System.IO.Compression.ZipFile]::OpenRead($UE4SSArchive)
        try {
            foreach ($SourceEntry in $Distribution.Entries) {
                if ($SourceEntry.FullName.EndsWith('/')) { continue }
                $Entry = $Archive.CreateEntry("$Win64/$($SourceEntry.FullName)")
                $InputStream = $SourceEntry.Open()
                $OutputStream = $Entry.Open()
                try {
                    if ($SourceEntry.FullName -in @('ue4ss/Mods/mods.txt', 'ue4ss/Mods/mods.json')) {
                        $Reader = [System.IO.StreamReader]::new($InputStream)
                        try { $Content = $Reader.ReadToEnd() } finally { $Reader.Dispose() }
                        if ($SourceEntry.FullName.EndsWith('mods.txt')) {
                            $Content = "BeamNGOrbitCamera : 1`r`n" + $Content
                        }
                        else {
                            $Mods = @($Content | ConvertFrom-Json) + @([pscustomobject]@{ mod_name = 'BeamNGOrbitCamera'; mod_enabled = $true })
                            $Content = ConvertTo-Json -InputObject $Mods -Depth 10
                        }
                        $Bytes = [System.Text.Encoding]::UTF8.GetBytes($Content)
                        $OutputStream.Write($Bytes, 0, $Bytes.Length)
                    }
                    else { $InputStream.CopyTo($OutputStream) }
                }
                finally { $InputStream.Dispose(); $OutputStream.Dispose() }
            }
        }
        finally { $Distribution.Dispose() }
        foreach ($Entry in $Files.GetEnumerator()) {
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($Archive, $Entry.Value, $Entry.Key) | Out-Null
        }
    }
    finally { $Archive.Dispose() }
}
finally { $Stream.Dispose() }

Write-Host "Release archive: $ArchivePath" -ForegroundColor Green
