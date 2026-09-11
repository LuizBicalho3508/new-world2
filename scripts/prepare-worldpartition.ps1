[CmdletBinding()]
param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$UERoot = "",
    [switch]$ForceRecreate
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor DarkCyan
    Write-Host $Message -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor DarkCyan
}

function Test-UE58Root {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }

    $editorCmd = Join-Path $Path "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    $versionFile = Join-Path $Path "Engine\Build\Build.version"
    if (-not (Test-Path $editorCmd) -or -not (Test-Path $versionFile)) { return $false }

    try {
        $version = Get-Content $versionFile -Raw | ConvertFrom-Json
        return ([int]$version.MajorVersion -eq 5 -and [int]$version.MinorVersion -eq 8)
    }
    catch { return $false }
}

function Find-UE58Root {
    param([string]$RequestedRoot)

    $candidates = [System.Collections.Generic.List[string]]::new()
    if ($RequestedRoot) { $candidates.Add($RequestedRoot) }
    if ($env:UE_ROOT) { $candidates.Add($env:UE_ROOT) }

    foreach ($regPath in @(
        "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8",
        "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\5.8"
    )) {
        if (Test-Path $regPath) {
            try {
                $installed = (Get-ItemProperty $regPath -ErrorAction Stop).InstalledDirectory
                if ($installed) { $candidates.Add([string]$installed) }
            }
            catch {}
        }
    }

    foreach ($common in @(
        "C:\Program Files\Epic Games\UE_5.8",
        "C:\Epic Games\UE_5.8",
        "D:\Epic Games\UE_5.8",
        "D:\UE_5.8"
    )) {
        $candidates.Add($common)
    }

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (Test-UE58Root $candidate) { return (Resolve-Path $candidate).Path }
    }
    return $null
}

$ProjectRoot = (Resolve-Path $ProjectRoot).Path
$ProjectFile = Join-Path $ProjectRoot "NewWorld2.uproject"
if (-not (Test-Path $ProjectFile)) {
    throw "NewWorld2.uproject nao encontrado em $ProjectRoot"
}

$ResolvedUERoot = Find-UE58Root $UERoot
if (-not $ResolvedUERoot) {
    throw "Unreal Engine 5.8 nao encontrada. Informe -UERoot ou instale UE 5.8 pelo Epic Games Launcher."
}

$EditorCmd = Join-Path $ResolvedUERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$SourceMap = Join-Path $ResolvedUERoot "Engine\Content\Maps\Entry.umap"
if (-not (Test-Path $SourceMap)) {
    $SourceMap = Get-ChildItem (Join-Path $ResolvedUERoot "Engine\Content\Maps") -Filter "Entry.umap" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $SourceMap -or -not (Test-Path $SourceMap)) {
    throw "Mapa base Entry.umap da Unreal nao foi encontrado."
}

$GeneratedDir = Join-Path $ProjectRoot "Content\GeneratedWorld"
$MapFile = Join-Path $GeneratedDir "NW2_OpenWorld.umap"
$MapIni = Join-Path $GeneratedDir "NW2_OpenWorld.ini"
$Marker = Join-Path $ProjectRoot "Saved\NW2_WorldPartition.ready"

if ($ForceRecreate) {
    Write-Step "REMOVENDO WORLD PARTITION LOCAL ANTERIOR"
    Remove-Item $GeneratedDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $ProjectRoot "Content\__ExternalActors__\GeneratedWorld") -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $ProjectRoot "Content\__ExternalObjects__\GeneratedWorld") -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $Marker -Force -ErrorAction SilentlyContinue
}

if (Test-Path $Marker -and Test-Path $MapFile) {
    Write-Host "World Partition local ja preparado: /Game/GeneratedWorld/NW2_OpenWorld" -ForegroundColor Green
    exit 0
}

Write-Step "PREPARANDO MAPA BASE LOCAL"
New-Item -ItemType Directory -Force -Path $GeneratedDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $Marker -Parent) | Out-Null

if (-not (Test-Path $MapFile)) {
    Copy-Item $SourceMap $MapFile -Force
}

@'
[/Script/UnrealEd.WorldPartitionConvertCommandlet]
EditorHashClass=Class'/Script/Engine.WorldPartitionEditorSpatialHash'
RuntimeHashClass=Class'/Script/Engine.WorldPartitionRuntimeSpatialHash'
HLODLayerAssetsPath=
DefaultHLODLayerName=

[/Script/Engine.WorldPartitionEditorSpatialHash]
CellSize=25600
WorldImage=None
'@ | Set-Content -Path $MapIni -Encoding UTF8

Write-Step "CONVERTENDO NW2_OPENWORLD PARA WORLD PARTITION"
Push-Location $GeneratedDir
try {
    & $EditorCmd `
        $ProjectFile `
        "-run=WorldPartitionConvertCommandlet" `
        "NW2_OpenWorld.umap" `
        "-AllowCommandletRendering" `
        "-SCCProvider=None" `
        "-Verbose" `
        "-Unattended" `
        "-NoSplash"

    if ($LASTEXITCODE -ne 0) {
        throw "WorldPartitionConvertCommandlet falhou com codigo $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

if (-not (Test-Path $MapFile)) {
    throw "A conversao terminou sem o mapa esperado: $MapFile"
}

"UE58 World Partition preparado em $(Get-Date -Format o)" | Set-Content -Path $Marker -Encoding UTF8

Write-Host ""
Write-Host "WORLD PARTITION PRONTO." -ForegroundColor Green
Write-Host "Mapa: /Game/GeneratedWorld/NW2_OpenWorld" -ForegroundColor Green
Write-Host "Celula do editor hash: 256 m (25600 cm)." -ForegroundColor Green
Write-Host "O PlayerController da Unreal funciona como streaming source durante o jogo." -ForegroundColor Green
