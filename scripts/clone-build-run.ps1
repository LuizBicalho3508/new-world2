[CmdletBinding()]
param(
    [string]$RepoUrl = "https://github.com/LuizBicalho3508/new-world2.git",
    [string]$Destination = "$HOME\Documents\new-world2",
    [string]$UERoot = "",
    [switch]$SkipBuildToolsInstall
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
    if (Get-Command git.exe -ErrorAction SilentlyContinue) {
        return
    }

    Write-Step "INSTALANDO GIT"
    Require-Winget
    & winget.exe install --id Git.Git -e --source winget --accept-source-agreements --accept-package-agreements --silent
    if ($LASTEXITCODE -ne 0) {
        throw "Falha ao instalar Git pelo winget. Codigo: $LASTEXITCODE"
    }
    Refresh-ProcessPath

    if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
        $gitCommon = "${env:ProgramFiles}\Git\cmd"
        if (Test-Path $gitCommon) {
            $env:Path = "$gitCommon;$env:Path"
        }
    }

    if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
        throw "Git foi instalado, mas nao ficou disponivel nesta sessao. Feche o PowerShell, abra novamente e rode o script."
    }
}

function Get-VSWherePath {
    $candidate = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $candidate) {
        return $candidate
    }
    return $null
}

function Test-CppToolchain {
    $vswhere = Get-VSWherePath
    if (-not $vswhere) {
        return $false
    }

    $installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    return -not [string]::IsNullOrWhiteSpace(($installation | Out-String).Trim())
}

function Ensure-CppToolchain {
    if (Test-CppToolchain) {
        Write-Host "Toolchain C++ do Visual Studio encontrado." -ForegroundColor Green
        return
    }

    if ($SkipBuildToolsInstall) {
        throw "Toolchain C++ nao encontrado e -SkipBuildToolsInstall foi informado. Instale Visual Studio 2022 17.14+ ou Visual Studio 2026 com Game development with C++."
    }

    Write-Step "INSTALANDO VISUAL STUDIO 2022 BUILD TOOLS + C++"
    Require-Winget

    $override = '--wait --passive --norestart --add Microsoft.VisualStudio.Workload.NativeGame --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
    & winget.exe install --id Microsoft.VisualStudio.2022.BuildTools -e --source winget --accept-source-agreements --accept-package-agreements --override $override
    if ($LASTEXITCODE -ne 0) {
        throw "Falha ao instalar Visual Studio Build Tools. Codigo: $LASTEXITCODE"
    }

    Refresh-ProcessPath
    if (-not (Test-CppToolchain)) {
        throw "Build Tools foi instalado, mas o toolchain C++ ainda nao foi detectado. Abra o Visual Studio Installer, adicione 'Game development with C++' e rode novamente."
    }
}

function Test-UE58Root {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $false
    }

    $editor = Join-Path $Path "Engine\Binaries\Win64\UnrealEditor.exe"
    $buildBat = Join-Path $Path "Engine\Build\BatchFiles\Build.bat"
    $versionFile = Join-Path $Path "Engine\Build\Build.version"

    if (-not (Test-Path $editor) -or -not (Test-Path $buildBat) -or -not (Test-Path $versionFile)) {
        return $false
    }

    try {
        $version = Get-Content $versionFile -Raw | ConvertFrom-Json
        return ([int]$version.MajorVersion -eq 5 -and [int]$version.MinorVersion -eq 8)
    }
    catch {
        return $false
    }
}

function Find-UE58Root {
    param([string]$RequestedRoot)

    $candidates = [System.Collections.Generic.List[string]]::new()

    if (-not [string]::IsNullOrWhiteSpace($RequestedRoot)) {
        $candidates.Add($RequestedRoot)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:UE_ROOT)) {
        $candidates.Add($env:UE_ROOT)
    }

    foreach ($regPath in @(
        "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8",
        "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\5.8"
    )) {
        if (Test-Path $regPath) {
            try {
                $installed = (Get-ItemProperty $regPath -ErrorAction Stop).InstalledDirectory
                if ($installed) {
                    $candidates.Add([string]$installed)
                }
            }
            catch {}
        }
    }

    $buildsKey = "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds"
    if (Test-Path $buildsKey) {
        try {
            $props = Get-ItemProperty $buildsKey
            foreach ($prop in $props.PSObject.Properties) {
                if ($prop.Name -notmatch '^PS' -and $prop.Value -is [string]) {
                    $candidates.Add([string]$prop.Value)
                }
            }
        }
        catch {}
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
        if (Test-UE58Root $candidate) {
            return (Resolve-Path $candidate).Path
        }
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

        if ($launcher) {
            Start-Process $launcher
        }
    }

    Write-Host ""
    Write-Host "O projeto ja foi clonado, mas a Epic nao oferece uma instalacao suportada da UE 5.8 inteiramente por linha de comando pelo Launcher." -ForegroundColor Yellow
    Write-Host "No Epic Games Launcher: Unreal Engine > Library > instale Unreal Engine 5.8." -ForegroundColor Yellow
    Write-Host "Depois execute exatamente este mesmo script novamente; ele continuara do ponto em que parou." -ForegroundColor Yellow
}

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " NEW WORLD 2 - CLONE + BUILD + TESTE INICIAL (UE 5.8)" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

Write-Step "1/6 - VALIDANDO GIT"
Ensure-Git
Write-Host ((& git.exe --version) -join " ") -ForegroundColor Green

Write-Step "2/6 - CLONANDO/ATUALIZANDO REPOSITORIO"
$destinationParent = Split-Path -Parent $Destination
if (-not (Test-Path $destinationParent)) {
    New-Item -ItemType Directory -Force -Path $destinationParent | Out-Null
}

if (Test-Path (Join-Path $Destination ".git")) {
    $dirty = (& git.exe -C $Destination status --porcelain) -join "`n"
    if (-not [string]::IsNullOrWhiteSpace($dirty)) {
        throw "Existem alteracoes locais em $Destination. Commit/stash essas alteracoes antes de atualizar para evitar perda de trabalho."
    }

    & git.exe -C $Destination fetch origin
    if ($LASTEXITCODE -ne 0) { throw "Falha no git fetch." }
    & git.exe -C $Destination checkout main
    if ($LASTEXITCODE -ne 0) { throw "Falha ao mudar para main." }
    & git.exe -C $Destination pull --ff-only origin main
    if ($LASTEXITCODE -ne 0) { throw "Falha no git pull --ff-only." }
}
elseif (Test-Path $Destination) {
    $existing = Get-ChildItem -Force $Destination -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($existing) {
        throw "A pasta $Destination existe e nao e um repositorio Git vazio. Escolha outro -Destination."
    }
    & git.exe clone $RepoUrl $Destination
    if ($LASTEXITCODE -ne 0) { throw "Falha ao clonar $RepoUrl." }
}
else {
    & git.exe clone $RepoUrl $Destination
    if ($LASTEXITCODE -ne 0) { throw "Falha ao clonar $RepoUrl." }
}

$ProjectFile = Join-Path $Destination "NewWorld2.uproject"
if (-not (Test-Path $ProjectFile)) {
    throw "NewWorld2.uproject nao foi encontrado em $Destination. Confirme que a main contem o prototipo UE 5.8."
}

Write-Host "Repositorio pronto em: $Destination" -ForegroundColor Green

Write-Step "3/6 - LOCALIZANDO UNREAL ENGINE 5.8"
$resolvedUERoot = Find-UE58Root $UERoot
if (-not $resolvedUERoot) {
    Ensure-EpicLauncherAndExplainUEInstall
    exit 2
}

$EditorExe = Join-Path $resolvedUERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$BuildBat = Join-Path $resolvedUERoot "Engine\Build\BatchFiles\Build.bat"
Write-Host "UE 5.8: $resolvedUERoot" -ForegroundColor Green

Write-Step "4/6 - VALIDANDO TOOLCHAIN C++"
Ensure-CppToolchain

Write-Step "5/6 - COMPILANDO NEW WORLD 2 EDITOR"
Push-Location $Destination
try {
    & $BuildBat "NewWorld2Editor" "Win64" "Development" $ProjectFile "-WaitMutex" "-NoHotReloadFromIDE"
    if ($LASTEXITCODE -ne 0) {
        throw "A compilacao Unreal falhou com codigo $LASTEXITCODE. Revise a saida acima e Saved\Logs."
    }
}
finally {
    Pop-Location
}

Write-Step "6/6 - INICIANDO TESTE JOGAVEL"
$gameMap = "/Engine/Maps/Entry?game=/Script/NewWorld2.NWGameMode"
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
Write-Host "Controles:" -ForegroundColor Cyan
Write-Host "  WASD       mover"
Write-Host "  Mouse      camera"
Write-Host "  Espaco     pular"
Write-Host "  Shift      correr"
Write-Host "  Mouse Esq. ataque basico da arma ativa"
Write-Host "  Q          habilidade ofensiva 1"
Write-Host "  E          habilidade ofensiva 2"
Write-Host "  C          habilidade de cura"
Write-Host "  1 / 2      selecionar slot de arma"
Write-Host "  F          troca rapida entre as duas armas"
Write-Host "  Z / X      percorre familias dos slots 1 / 2 (debug)"
Write-Host "  R          gerar novo epoch/mundo imediatamente"
Write-Host ""
Write-Host "Dica de combo: use Q/E, pressione F e encaixe Q/E da segunda arma em ate 2,5 s." -ForegroundColor Yellow
Write-Host "O primeiro ataque de mobs contra as cidades comeca em ~20 s e repete a cada 55 s." -ForegroundColor Yellow
Write-Host "O mundo tambem evolui automaticamente a cada 180 segundos." -ForegroundColor Green
