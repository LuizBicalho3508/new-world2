[CmdletBinding()]
param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$contentRoot = Join-Path $ProjectRoot "Content"
if (-not (Test-Path $contentRoot)) {
    throw "Pasta Content nao encontrada em: $contentRoot"
}

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " NEW WORLD 2 - VERIFICACAO DE ASSETS FAB / EPIC" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "Projeto: $ProjectRoot"
Write-Host ""

$allAssets = Get-ChildItem -Path $contentRoot -Recurse -File -Filter *.uasset -ErrorAction SilentlyContinue
$allPaths = @($allAssets | ForEach-Object { $_.FullName.ToLowerInvariant() })

function Test-AnyPattern {
    param([string[]]$Patterns)
    foreach ($pattern in $Patterns) {
        $needle = $pattern.ToLowerInvariant()
        if ($allPaths | Where-Object { $_ -like "*$needle*" } | Select-Object -First 1) {
            return $true
        }
    }
    return $false
}

function Show-Pack {
    param(
        [string]$Name,
        [string[]]$Patterns,
        [switch]$Required
    )

    $ok = Test-AnyPattern $Patterns
    if ($ok) {
        Write-Host ("[OK]      {0}" -f $Name) -ForegroundColor Green
    }
    elseif ($Required) {
        Write-Host ("[FALTA]   {0}" -f $Name) -ForegroundColor Red
    }
    else {
        Write-Host ("[OPCIONAL]{0}" -f " $Name") -ForegroundColor Yellow
    }
    return $ok
}

$results = [ordered]@{}
$results["Paragon Sevarog"] = Show-Pack "Paragon: Sevarog" @("paragonsevarog") -Required
$results["Paragon Rampage"] = Show-Pack "Paragon: Rampage" @("paragonrampage") -Required
$results["Paragon Khaimera"] = Show-Pack "Paragon: Khaimera" @("paragonkhaimera") -Required
$results["Paragon Countess"] = Show-Pack "Paragon: Countess" @("paragoncountess") -Required
$results["Paragon Revenant"] = Show-Pack "Paragon: Revenant" @("paragonrevenant") -Required
$results["Ethereal Bow"] = Show-Pack "Ethereal Recurve Bow" @("ethereal", "recurve")
$results["Necromancer Sword"] = Show-Pack "Necromancer Bone Sword" @("necromancer", "bone_sword", "bonesword")
$results["Knight Shield"] = Show-Pack "Medieval Knight Shield" @("knight", "shield")
$results["Sword Pack"] = Show-Pack "Free Sword / Melee Weapon Pack" @("greatsword", "longsword", "long_sword", "dagger", "sabre", "saber")
$results["Modular Armor"] = Show-Pack "Modular Medieval Armor" @("modular", "armor", "armour")

Write-Host ""
Write-Host ("Total de .uasset encontrados: {0}" -f $allAssets.Count) -ForegroundColor Cyan
$detected = @($results.GetEnumerator() | Where-Object { $_.Value }).Count
Write-Host ("Packs/grupos detectados: {0}/{1}" -f $detected, $results.Count) -ForegroundColor Cyan

if ($detected -lt $results.Count) {
    Write-Host ""
    Write-Host "IMPORTANTE:" -ForegroundColor Yellow
    Write-Host "Adicionar um item a Biblioteca da Epic/Fab nao copia automaticamente os .uasset para este projeto." -ForegroundColor Yellow
    Write-Host "No Unreal/Fab, use Add to Project / Adicionar ao Projeto para os packs que ainda aparecem como FALTA/OPCIONAL." -ForegroundColor Yellow
    Write-Host "Depois execute este script novamente." -ForegroundColor Yellow
}
else {
    Write-Host ""
    Write-Host "Todos os grupos esperados foram encontrados dentro da pasta Content." -ForegroundColor Green
}

Write-Host ""
Write-Host "O runtime tambem faz autodeteccao via Asset Registry e possui fallback quando um asset nao estiver instalado." -ForegroundColor DarkGray
