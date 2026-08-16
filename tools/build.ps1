param(
    [ValidateSet("Development", "Shipping")]
    [string]$Mode = "Development",

    [string]$UE4SSDir = "",

    [switch]$Deploy,

    [string]$GameDir = "",

    [string]$GameModDll = ""
)

$ErrorActionPreference = "Stop"

function Write-SuccessBanner {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    $Border = "=" * 72

    Write-Host ""
    Write-Host $Border -ForegroundColor Green
    Write-Host ("[ SUCCESS ]  " + $Message) -ForegroundColor Green
    Write-Host $Border -ForegroundColor Green
    Write-Host ""
}

function Find-AcrDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $Current = if (Test-Path $Path -PathType Container) {
        (Resolve-Path $Path).Path
    }
    else {
        Split-Path $Path -Parent
    }

    while (-not [string]::IsNullOrWhiteSpace($Current)) {
        if ((Split-Path $Current -Leaf) -ieq "acr") {
            return $Current
        }

        $Parent = Split-Path $Current -Parent
        if ([string]::IsNullOrWhiteSpace($Parent) -or $Parent -eq $Current) {
            break
        }

        $Current = $Parent
    }

    throw "Could not locate the game's 'acr' directory from '$Path'. Pass -GameDir <Assetto Corsa Rally root>."
}

$RepoRoot = Split-Path $PSScriptRoot -Parent
$Target = "BeamNGOrbitCamera"
$CMakeConfig = "Game__Shipping__Win64"
$RepoPak = Join-Path $RepoRoot "Data\BeamNGOrbitModifier.pak"
$RepoUcas = Join-Path $RepoRoot "Data\BeamNGOrbitModifier.ucas"
$RepoUtoc = Join-Path $RepoRoot "Data\BeamNGOrbitModifier.utoc"

if ([string]::IsNullOrWhiteSpace($UE4SSDir)) {
    $UE4SSDir = Join-Path $RepoRoot "RE-UE4SS"
}

$UE4SSCMake = Join-Path $UE4SSDir "CMakeLists.txt"
if (-not (Test-Path $UE4SSCMake)) {
    throw "RE-UE4SS was not found at '$UE4SSDir'. Pass -UE4SSDir <path> or place the matching checkout in '$RepoRoot\RE-UE4SS'."
}

$Diagnostics = if ($Mode -eq "Development") { "ON" } else { "OFF" }
$BuildDir = Join-Path (Join-Path $RepoRoot "build") $Mode

Write-Host "BeamNG Orbit Camera - $Mode"
Write-Host "  Repository:  $RepoRoot"
Write-Host "  UE4SS:       $UE4SSDir"
Write-Host "  CMake config: $CMakeConfig"
Write-Host "  Diagnostics: $Diagnostics"
Write-Host "  Build dir:   $BuildDir"
Write-Host "  Camera PAK:  $(if (Test-Path $RepoPak) { $RepoPak } else { 'not present (optional)' })"
Write-Host "  Camera UCAS:  $(if (Test-Path $RepoUcas) { $RepoUcas } else { 'not present (optional)' })"
Write-Host "  Camera UTOC:  $(if (Test-Path $RepoUtoc) { $RepoUtoc } else { 'not present (optional)' })"

cmake `
    -S $RepoRoot `
    -B $BuildDir `
    -G "Visual Studio 17 2022" `
    "-DBEAMNG_ORBIT_CAMERA_UE4SS_DIR=$UE4SSDir" `
    "-DBEAMNG_ORBIT_CAMERA_ENABLE_DIAGNOSTICS=$Diagnostics"

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

# Patch archives can carry timestamps from another timezone. If Camera.hpp
# changes class layout but MSBuild considers an older .obj newer than the
# extracted header, different translation units can be linked with incompatible
# member offsets. Rebuild only this mod's intermediates every time; RE-UE4SS
# remains incremental.
$ModIntermediateDir = Join-Path $BuildDir "Source\$Target.dir"
if (Test-Path $ModIntermediateDir) {
    Write-Host "  Cleaning mod intermediates: $ModIntermediateDir"
    Remove-Item -Recurse -Force $ModIntermediateDir
}

cmake --build $BuildDir --config $CMakeConfig --target $Target
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$BuiltDll = Join-Path $BuildDir "Source\$CMakeConfig\$Target.dll"
if (-not (Test-Path $BuiltDll)) {
    $Candidate = Get-ChildItem `
        -Path $BuildDir `
        -Filter "$Target.dll" `
        -File `
        -Recurse `
        -ErrorAction SilentlyContinue | `
        Where-Object { $_.FullName -like "*$CMakeConfig*" } | `
        Select-Object -First 1

    if ($null -eq $Candidate) {
        throw "Build succeeded but '$Target.dll' was not found under '$BuildDir'."
    }

    $BuiltDll = $Candidate.FullName
}

Write-Host "Built: $BuiltDll"

if (-not $Deploy) {
    Write-SuccessBanner "BUILD COMPLETED ($Mode)"
    exit 0
}

if ([string]::IsNullOrWhiteSpace($GameModDll)) {
    if ([string]::IsNullOrWhiteSpace($GameDir)) {
        $GameDir = $env:ASSETO_CORSA_RALLY_HOME
    }

    if ([string]::IsNullOrWhiteSpace($GameDir)) {
        throw "-Deploy requires -GameDir <Assetto Corsa Rally root>, -GameModDll <path>, or the ASSETO_CORSA_RALLY_HOME environment variable."
    }

    $GameModDll = Join-Path $GameDir "acr\Binaries\Win64\ue4ss\Mods\BeamNGOrbitCamera\dlls\main.dll"
}

$DestinationDir = Split-Path $GameModDll -Parent
New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
Copy-Item -Force $BuiltDll $GameModDll

$RepoConfig = Join-Path $RepoRoot "Data\config.ini"
$ModRoot = Split-Path $DestinationDir -Parent
$GameDataDir = Join-Path $ModRoot "Data"
$GameConfig = Join-Path $GameDataDir "config.ini"

New-Item -ItemType Directory -Force -Path $GameDataDir | Out-Null

if ((Test-Path $RepoConfig) -and -not (Test-Path $GameConfig)) {
    Copy-Item $RepoConfig $GameConfig
    Write-Host "Created config: $GameConfig"
}
elseif (Test-Path $GameConfig) {
    Write-Host "Config preserved: $GameConfig"
}

if (Test-Path $RepoPak && Test-Path $RepoUcas && Test-Path $RepoUtoc) {
    $AcrDir = if (-not [string]::IsNullOrWhiteSpace($GameDir)) {
        $GameDirLeaf = Split-Path $GameDir -Leaf
        if ($GameDirLeaf -ieq "acr") {
            $GameDir
        }
        else {
            Join-Path $GameDir "acr"
        }
    }
    else {
        Find-AcrDirectory -Path $GameModDll
    }

    $PaksDir = Join-Path $AcrDir "Content\Paks"
    $GamePak = Join-Path $PaksDir "BeamNGOrbitModifier.pak"
    $GameUcas = Join-Path $PaksDir "BeamNGOrbitModifier.ucas"
    $GameUtoc = Join-Path $PaksDir "BeamNGOrbitModifier.utoc"

    New-Item -ItemType Directory -Force -Path $PaksDir | Out-Null
    Copy-Item -Force $RepoPak $GamePak
    Copy-Item -Force $RepoUcas $GameUcas
    Copy-Item -Force $RepoUtoc $GameUtoc

    Write-Host "Deployed camera modifier PAK: $GamePak"
    Write-Host "Deployed camera modifier UCAS: $GameUcas"
    Write-Host "Deployed camera modifier UTOC: $GameUtoc"
}
else {
    throw "Deploy requires the dedicated camera modifier PAK/UCAS/UTOC at '$RepoPak'."
}

Write-Host "Deployed: $GameModDll"
Write-SuccessBanner "BUILD + DEPLOY COMPLETED ($Mode)"
