[CmdletBinding()]
param(
    [string]$RepoUrl = "https://github.com/LuizBicalho3508/new-world2.git",
    [string]$Destination = "$HOME\Documents\new-world2",
    [string]$UERoot = "",
    [switch]$SkipBuildToolsInstall,
    [switch]$SkipWorldPartition
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

function Refresh-ProcessPath {
    $machine = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $user = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machine;$user"
}

function Require-Winget {
    if (-not (Get-Command winget.exe -ErrorAction SilentlyContinue)) {
        throw "winget nao foi encontrado. Atualize/instale o App Installer da Microsoft Store e execute este script novamente."
    }
}

function Ensure-Git {
    if (Get-Command git.exe -ErrorAction SilentlyContinue) { return }

    Write-Step "INSTALANDO GIT"
    Require-Winget
    & winget.exe install --id Git.Git -e --source winget --accept-source-agreements --accept-package-agreements --silent
    if ($LASTEXITCODE -ne 0) { throw "Falha ao instalar Git pelo winget. Codigo: $LASTEXITCODE" }
    Refresh-ProcessPath

    if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
        $gitCommon = "${env:ProgramFiles}\Git\cmd"
        if (Test-Path $gitCommon) { $env:Path = "$gitCommon;$env:Path" }
    }
    if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
        throw "Git foi instalado, mas nao ficou disponivel nesta sessao. Feche o PowerShell, abra novamente e rode o script."
    }
}

function Get-VSWherePath {
    $candidate = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $candidate) { return $candidate }
    return $null
}

function Test-CppToolchain {
    $vswhere = Get-VSWherePath
    if (-not $vswhere) { return $false }
    $installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    return -not [string]::IsNullOrWhiteSpace(($installation | Out-String).Trim())
}

function Ensure-CppToolchain {
    if (Test-CppToolchain) {
        Write-Host "Toolchain C++ do Visual Studio encontrado." -ForegroundColor Green
        return
    }

    if ($SkipBuildToolsInstall) {
        throw "Toolchain C++ nao encontrado. Instale Visual Studio 2022 17.14+ ou Visual Studio 2026 com Game development with C++."
    }

    Write-Step "INSTALANDO VISUAL STUDIO 2022 BUILD TOOLS + C++"
    Require-Winget
    $override = '--wait --passive --norestart --add Microsoft.VisualStudio.Workload.NativeGame --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
    & winget.exe install --id Microsoft.VisualStudio.2022.BuildTools -e --source winget --accept-source-agreements --accept-package-agreements --override $override
    if ($LASTEXITCODE -ne 0) { throw "Falha ao instalar Visual Studio Build Tools. Codigo: $LASTEXITCODE" }

    Refresh-ProcessPath
    if (-not (Test-CppToolchain)) {
        throw "Build Tools foi instalado, mas o toolchain C++ ainda nao foi detectado. Abra o Visual Studio Installer e adicione Game development with C++."
    }
}

function Test-UE58Root {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }

    $editor = Join-Path $Path "Engine\Binaries\Win64\UnrealEditor.exe"
    $buildBat = Join-Path $Path "Engine\Build\BatchFiles\Build.bat"
    $versionFile = Join-Path $Path "Engine\Build\Build.version"
    if (-not (Test-Path $editor) -or -not (Test-Path $buildBat) -or -not (Test-Path $versionFile)) { return $false }

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

    $buildsKey = "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds"
    if (Test-Path $buildsKey) {
        try {
            $props = Get-ItemProperty $buildsKey
            foreach ($prop in $props.PSObject.Properties) {
                if ($prop.Name -notmatch '^PS' -and $prop.Value -is [string]) { $candidates.Add([string]$prop.Value) }
            }
        }
        catch {}
    }

    foreach ($common in @(
        "C:\Program Files\Epic Games\UE_5.8",
        "C:\Epic Games\UE_5.8",
        "D:\Epic Games\UE_5.8",
        "D:\UE_5.8"
    )) { $candidates.Add($common) }

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (Test-UE58Root $candidate) { return (Resolve-Path $candidate).Path }
    }
    return $null
}

function Ensure-EpicLauncherAndExplainUEInstall {
    Write-Step "UNREAL ENGINE 5.8 NAO ENCONTRADA"

    if (Get-Command winget.exe -ErrorAction SilentlyContinue) {
        $launcherCandidates = @(
            "${env:ProgramFiles(x86)}\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe",
            "${env:ProgramFiles}\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe"
        )
        $launcher = $launcherCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
        if (-not $launcher) {
            Write-Host "Instalando Epic Games Launcher..." -ForegroundColor Yellow
            & winget.exe install --id EpicGames.EpicGamesLauncher -e --source winget --accept-source-agreements --accept-package-agreements --silent
            Refresh-ProcessPath
            $launcher = $launcherCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
        }
        if ($launcher) { Start-Process $launcher }
    }

    Write-Host ""
    Write-Host "O projeto ja foi clonado. Instale Unreal Engine 5.8 em Epic Games Launcher > Unreal Engine > Library." -ForegroundColor Yellow
    Write-Host "Depois execute exatamente este script novamente." -ForegroundColor Yellow
}

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " NEW WORLD 2 - CLONE + BUILD + WORLD PARTITION + TESTE" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

Write-Step "1/7 - VALIDANDO GIT"
Ensure-Git
Write-Host ((& git.exe --version) -join " ") -ForegroundColor Green

Write-Step "2/7 - CLONANDO/ATUALIZANDO REPOSITORIO"
$destinationParent = Split-Path -Parent $Destination
if (-not (Test-Path $destinationParent)) { New-Item -ItemType Directory -Force -Path $destinationParent | Out-Null }

if (Test-Path (Join-Path $Destination ".git")) {
    $dirty = (& git.exe -C $Destination status --porcelain) -join "`n"
    if ($dirty) { throw "Existem alteracoes locais versionadas em $Destination. Commit/stash antes de atualizar." }

    & git.exe -C $Destination fetch origin
    if ($LASTEXITCODE -ne 0) { throw "Falha no git fetch." }
    & git.exe -C $Destination checkout main
    if ($LASTEXITCODE -ne 0) { throw "Falha ao mudar para main." }
    & git.exe -C $Destination pull --ff-only origin main
    if ($LASTEXITCODE -ne 0) { throw "Falha no git pull --ff-only." }
}
elseif (Test-Path $Destination) {
    $existing = Get-ChildItem -Force $Destination -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($existing) { throw "A pasta $Destination existe e nao e um repositorio Git vazio." }
    & git.exe clone $RepoUrl $Destination
    if ($LASTEXITCODE -ne 0) { throw "Falha ao clonar $RepoUrl." }
}
else {
    & git.exe clone $RepoUrl $Destination
    if ($LASTEXITCODE -ne 0) { throw "Falha ao clonar $RepoUrl." }
}

$ProjectFile = Join-Path $Destination "NewWorld2.uproject"
if (-not (Test-Path $ProjectFile)) { throw "NewWorld2.uproject nao encontrado em $Destination." }
Write-Host "Repositorio pronto: $Destination" -ForegroundColor Green
& git.exe -C $Destination log -1 --oneline

Write-Step "3/7 - LOCALIZANDO UNREAL ENGINE 5.8"
$resolvedUERoot = Find-UE58Root $UERoot
if (-not $resolvedUERoot) {
    Ensure-EpicLauncherAndExplainUEInstall
    exit 2
}

$EditorExe = Join-Path $resolvedUERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$BuildBat = Join-Path $resolvedUERoot "Engine\Build\BatchFiles\Build.bat"
Write-Host "UE 5.8: $resolvedUERoot" -ForegroundColor Green

Write-Step "4/7 - VALIDANDO TOOLCHAIN C++"
Ensure-CppToolchain

Write-Step "5/7 - COMPILANDO NEW WORLD 2 EDITOR"
Push-Location $Destination
try {
    & $BuildBat "NewWorld2Editor" "Win64" "Development" $ProjectFile "-WaitMutex" "-NoHotReloadFromIDE"
    if ($LASTEXITCODE -ne 0) { throw "Compilacao Unreal falhou com codigo $LASTEXITCODE. Revise Saved\Logs." }
}
finally { Pop-Location }

$gameMap = "/Engine/Maps/Entry?game=/Script/NewWorld2.NWGameMode"

Write-Step "6/7 - PREPARANDO WORLD PARTITION"
if ($SkipWorldPartition) {
    Write-Host "World Partition ignorado por -SkipWorldPartition; usando mapa fallback." -ForegroundColor Yellow
}
else {
    $WorldPartitionScript = Join-Path $Destination "scripts\prepare-worldpartition.ps1"
    if (Test-Path $WorldPartitionScript) {
        $wpArgs = @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $WorldPartitionScript,
            "-ProjectRoot", $Destination,
            "-UERoot", $resolvedUERoot
        )
        & powershell.exe @wpArgs
        if ($LASTEXITCODE -eq 0 -and (Test-Path (Join-Path $Destination "Content\GeneratedWorld\NW2_OpenWorld.umap"))) {
            $gameMap = "/Game/GeneratedWorld/NW2_OpenWorld?game=/Script/NewWorld2.NWGameMode"
            Write-Host "World Partition ativado para este teste." -ForegroundColor Green
        }
        else {
            Write-Warning "Nao foi possivel preparar World Partition. O jogo abrira no mapa fallback; o codigo continua testavel."
        }
    }
}

Write-Step "7/7 - INICIANDO TESTE JOGAVEL"
$arguments = @(
    $ProjectFile,
    $gameMap,
    "-game",
    "-log",
    "-windowed",
    "-ResX=1280",
    "-ResY=720",
    "-ExecCmds=stat fps"
)
Start-Process -FilePath $EditorExe -ArgumentList $arguments -WorkingDirectory $Destination

Write-Host ""
Write-Host "TESTE INICIADO." -ForegroundColor Green
Write-Host ""
Write-Host "CONTROLES" -ForegroundColor Cyan
Write-Host "  WASD          mover"
Write-Host "  Mouse         camera"
Write-Host "  Espaco        pular"
Write-Host "  Shift         correr"
Write-Host "  Mouse Esq.    ataque basico"
Write-Host "  Mouse Dir.    bloquear / janela inicial de parry"
Write-Host "  Alt Esq.      dodge com i-frames"
Write-Host "  Q / E         habilidades ofensivas"
Write-Host "  C             cura da arma"
Write-Host "  1 / 2         selecionar armas"
Write-Host "  F             troca rapida / combo cross-weapon"
Write-Host "  Z / X         trocar familia das armas (debug)"
Write-Host "  G             coletar loot proximo"
Write-Host "  I             abrir/fechar inventario"
Write-Host "  Setas         selecionar item no inventario"
Write-Host "  Enter         equipar item selecionado"
Write-Host "  R             novo epoch do mundo"
Write-Host ""
Write-Host "HUD: vida, stamina, arma ativa, 3 habilidades e cooldowns." -ForegroundColor Green
Write-Host "Drops agora aparecem fisicamente no mundo e entram na mochila somente ao coletar." -ForegroundColor Green
Write-Host "Packs gratuitos instalados localmente sao detectados automaticamente sem serem enviados ao GitHub." -ForegroundColor Green
