[CmdletBinding()]
param(
    [string]$RepoUrl = "https://github.com/LuizBicalho3508/new-world2.git",
    [string]$Destination = "$HOME\Documents\new-world2",
    [string]$UERoot = "",
    [switch]$SkipAssetCheck,
    [switch]$SkipWorldPartition
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Write-Section {
    param([string]$Text)
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor DarkCyan
    Write-Host " $Text" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor DarkCyan
}

function Refresh-Path {
    $machine = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $user = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machine;$user"
}

function Require-Winget {
    if (-not (Get-Command winget.exe -ErrorAction SilentlyContinue)) {
        throw "winget nao foi encontrado. Instale/atualize App Installer pela Microsoft Store e execute novamente."
    }
}

function Ensure-Git {
    if (Get-Command git.exe -ErrorAction SilentlyContinue) { return }
    Require-Winget
    Write-Host "Instalando Git..." -ForegroundColor Yellow
    & winget.exe install --id Git.Git -e --source winget --accept-source-agreements --accept-package-agreements --silent
    if ($LASTEXITCODE -ne 0) { throw "Falha ao instalar Git. Codigo: $LASTEXITCODE" }
    Refresh-Path
    if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
        $gitCommon = "$env:ProgramFiles\Git\cmd"
        if (Test-Path $gitCommon) { $env:Path = "$gitCommon;$env:Path" }
    }
    if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) { throw "Git nao ficou disponivel nesta sessao." }
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
        Write-Host "Toolchain C++ detectado." -ForegroundColor Green
        return
    }

    Require-Winget
    Write-Host "Instalando Visual Studio 2022 Build Tools + Game Development C++..." -ForegroundColor Yellow
    $override = '--wait --passive --norestart --add Microsoft.VisualStudio.Workload.NativeGame --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
    & winget.exe install --id Microsoft.VisualStudio.2022.BuildTools -e --source winget --accept-source-agreements --accept-package-agreements --override $override
    if ($LASTEXITCODE -ne 0) { throw "Falha ao instalar Visual Studio Build Tools. Codigo: $LASTEXITCODE" }
    Refresh-Path

    if (-not (Test-CppToolchain)) {
        throw "Build Tools foi instalado, mas o compilador C++ nao foi detectado. Abra Visual Studio Installer > Build Tools 2022 > Modify > Game development with C++."
    }
}

function Ensure-EpicLauncher {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe",
        "${env:ProgramFiles}\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe"
    )
    $launcher = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($launcher) { return $launcher }

    Require-Winget
    Write-Host "Instalando Epic Games Launcher..." -ForegroundColor Yellow
    & winget.exe install --id EpicGames.EpicGamesLauncher -e --source winget --accept-source-agreements --accept-package-agreements --silent
    if ($LASTEXITCODE -ne 0) { throw "Falha ao instalar Epic Games Launcher. Codigo: $LASTEXITCODE" }
    Start-Sleep -Seconds 2
    $launcher = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    return $launcher
}

function Test-UE58Root {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }
    $editor = Join-Path $Path "Engine\Binaries\Win64\UnrealEditor.exe"
    $build = Join-Path $Path "Engine\Build\BatchFiles\Build.bat"
    $versionFile = Join-Path $Path "Engine\Build\Build.version"
    if (-not (Test-Path $editor) -or -not (Test-Path $build) -or -not (Test-Path $versionFile)) { return $false }
    try {
        $v = Get-Content $versionFile -Raw | ConvertFrom-Json
        return ([int]$v.MajorVersion -eq 5 -and [int]$v.MinorVersion -eq 8)
    } catch { return $false }
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
            } catch {}
        }
    }

    $buildsKey = "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds"
    if (Test-Path $buildsKey) {
        try {
            $props = Get-ItemProperty $buildsKey
            foreach ($p in $props.PSObject.Properties) {
                if ($p.Name -notmatch '^PS' -and $p.Value -is [string]) { $candidates.Add([string]$p.Value) }
            }
        } catch {}
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

function Sync-Repository {
    Ensure-Git
    $parent = Split-Path -Parent $Destination
    if (-not (Test-Path $parent)) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }

    if (Test-Path (Join-Path $Destination ".git")) {
        $dirtyTracked = (& git.exe -C $Destination status --porcelain --untracked-files=no) -join "`n"
        if ($dirtyTracked) { throw "Ha alteracoes locais em arquivos versionados em $Destination. Commit/stash antes de atualizar." }
        & git.exe -C $Destination fetch origin
        if ($LASTEXITCODE -ne 0) { throw "git fetch falhou." }
        & git.exe -C $Destination checkout main
        if ($LASTEXITCODE -ne 0) { throw "git checkout main falhou." }
        & git.exe -C $Destination pull --ff-only origin main
        if ($LASTEXITCODE -ne 0) { throw "git pull falhou." }
    } else {
        if (Test-Path $Destination) {
            $existing = Get-ChildItem -Force $Destination -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($existing) { throw "$Destination existe e nao e um repositorio Git vazio." }
        }
        & git.exe clone $RepoUrl $Destination
        if ($LASTEXITCODE -ne 0) { throw "git clone falhou." }
    }

    & git.exe -C $Destination log -1 --oneline
}

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " NEW WORLD 2 - PRIMEIRA INSTALACAO / PRIMEIRO TESTE" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "Este script prepara Git, Build Tools, Epic Launcher, projeto, assets e inicia o build/play." -ForegroundColor DarkGray

Write-Section "1/6 - PREPARANDO FERRAMENTAS"
Require-Winget
Ensure-Git
Ensure-CppToolchain
$launcher = Ensure-EpicLauncher

Write-Section "2/6 - CLONANDO / ATUALIZANDO NEW WORLD 2"
Sync-Repository

Write-Section "3/6 - LOCALIZANDO UNREAL ENGINE 5.8"
$resolvedUERoot = Find-UE58Root $UERoot
if (-not $resolvedUERoot) {
    if ($launcher) { Start-Process $launcher }

    Write-Host ""
    Write-Host "A Unreal Engine 5.8 ainda NAO esta instalada." -ForegroundColor Yellow
    Write-Host "No Epic Games Launcher faca:" -ForegroundColor Yellow
    Write-Host "  1. Entre na sua conta Epic." -ForegroundColor White
    Write-Host "  2. Clique em Unreal Engine." -ForegroundColor White
    Write-Host "  3. Abra Library / Biblioteca." -ForegroundColor White
    Write-Host "  4. Em Engine Versions, clique no +." -ForegroundColor White
    Write-Host "  5. No menu da versao, selecione 5.8.x." -ForegroundColor White
    Write-Host "  6. Clique Install e aguarde terminar." -ForegroundColor White
    Write-Host "  7. Depois, volte ao PowerShell e execute ESTE MESMO SCRIPT novamente." -ForegroundColor White
    Write-Host ""
    Write-Host "O script parou de proposito aqui porque a Epic exige login/aceite/instalacao da Engine pelo Launcher." -ForegroundColor DarkGray
    exit 2
}
Write-Host "UE 5.8 encontrada em: $resolvedUERoot" -ForegroundColor Green

Write-Section "4/6 - VALIDANDO ASSETS FAB / EPIC"
if ($SkipAssetCheck) {
    Write-Host "Validacao dos assets ignorada por -SkipAssetCheck." -ForegroundColor Yellow
} else {
    $verifyScript = Join-Path $Destination "scripts\verify-fab-assets.ps1"
    if (Test-Path $verifyScript) {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $verifyScript -ProjectRoot $Destination
        if ($LASTEXITCODE -ne 0) { Write-Warning "A verificacao de assets retornou codigo $LASTEXITCODE, mas o build ainda pode prosseguir com fallbacks." }
    } else {
        Write-Warning "verify-fab-assets.ps1 nao encontrado. Continuando."
    }
}

Write-Host ""
Write-Host "Se algum pack que voce adicionou a Biblioteca aparecer como FALTA/OPCIONAL:" -ForegroundColor Yellow
Write-Host "abra o Fab/Epic e use Add to Project / Adicionar ao Projeto apontando para NewWorld2." -ForegroundColor Yellow
Write-Host "O jogo possui fallbacks e pode compilar mesmo sem todos os packs." -ForegroundColor DarkGray

Write-Section "5/6 - BUILD E PREPARACAO DO MAPA"
$buildScript = Join-Path $Destination "scripts\clone-build-run.ps1"
if (-not (Test-Path $buildScript)) { throw "clone-build-run.ps1 nao encontrado." }

$args = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $buildScript,
    "-Destination", $Destination,
    "-UERoot", $resolvedUERoot,
    "-SkipBuildToolsInstall"
)
if ($SkipWorldPartition) { $args += "-SkipWorldPartition" }

$hadShowUntracked = $false
$oldShowUntracked = $null
try {
    $oldShowUntracked = (& git.exe -C $Destination config --local --get status.showUntrackedFiles 2>$null | Select-Object -First 1)
    $hadShowUntracked = -not [string]::IsNullOrWhiteSpace([string]$oldShowUntracked)
    & git.exe -C $Destination config --local status.showUntrackedFiles no

    & powershell.exe @args
    if ($LASTEXITCODE -ne 0) {
        throw "Build/teste falhou com codigo $LASTEXITCODE. Copie o erro completo desta janela para o ChatGPT."
    }
}
finally {
    if ($hadShowUntracked) {
        & git.exe -C $Destination config --local status.showUntrackedFiles $oldShowUntracked
    } else {
        & git.exe -C $Destination config --local --unset status.showUntrackedFiles 2>$null
    }
}

Write-Section "6/6 - PRONTO"
Write-Host "O New World 2 foi compilado e o processo de teste foi iniciado." -ForegroundColor Green
Write-Host "Se a janela do jogo nao aparecer, verifique o log aberto pelo Unreal e Saved\Logs no projeto." -ForegroundColor Yellow
