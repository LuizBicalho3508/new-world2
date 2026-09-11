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

Write-Host "PERSONAGENS / CRIATURAS" -ForegroundColor Magenta
$results["Paragon Sevarog"] = Show-Pack "Paragon: Sevarog" @("paragonsevarog") -Required
$results["Paragon Rampage"] = Show-Pack "Paragon: Rampage" @("paragonrampage") -Required
$results["Paragon Khaimera"] = Show-Pack "Paragon: Khaimera" @("paragonkhaimera") -Required
$results["Paragon Countess"] = Show-Pack "Paragon: Countess" @("paragoncountess") -Required
$results["Paragon Revenant"] = Show-Pack "Paragon: Revenant" @("paragonrevenant") -Required
$results["Paragon Serath"] = Show-Pack "Paragon: Serath" @("paragonserath", "serath")
$results["Paragon Terra"] = Show-Pack "Paragon: Terra" @("paragonterra", "terra")
$results["Paragon Minions"] = Show-Pack "Paragon: Minions" @("paragonminions", "minion")
$results["Orc Warrior"] = Show-Pack "Orc warrior axe and shield" @("orc", "axe", "shield")
$results["Classic Knight"] = Show-Pack "Classic Medieval Knight Warrior" @("classicmedieval", "medievalknight", "knightwarrior")
$results["Medieval King"] = Show-Pack "Medieval King" @("medievalking", "medieval_king")
$results["Wasteland Warrior"] = Show-Pack "Wasteland Warrior" @("wastelandwarrior", "wasteland_warrior")

Write-Host ""
Write-Host "ARMAS REALISTAS / DARK FANTASY" -ForegroundColor Magenta
$results["Ethereal Bow"] = Show-Pack "Ethereal Recurve Bow" @("ethereal", "recurve")
$results["Necromancer Sword"] = Show-Pack "Necromancer Bone Sword" @("necromancer", "bone_sword", "bonesword")
$results["Sword Pack"] = Show-Pack "Free Sword Pack / Realistic Melee Weapons" @("freesword", "greatsword", "longsword", "long_sword", "sabre", "saber")
$results["Short Sword"] = Show-Pack "Short Sword" @("shortsword", "short_sword")
$results["Medieval Dagger"] = Show-Pack "Medieval Dagger" @("medievaldagger", "medieval_dagger")
$results["Daggers"] = Show-Pack "Daggers Pack" @("daggers", "dagger")
$results["Atris Swords"] = Show-Pack "Atris Swords" @("atris")
$results["Thornblade"] = Show-Pack "Thornblade Sword" @("thornblade")
$results["Dark Knight Longsword"] = Show-Pack "Dark Knight Longsword" @("darkknight", "dark_knight", "longsword")
$results["Fantasy Weapon Sample"] = Show-Pack "Free Fantasy Weapon Sample Pack" @("fantasy", "weapon", "sample")
$results["Iron Wood Shield"] = Show-Pack "Medieval Iron and Wood Shield" @("iron", "wood", "shield")
$results["Viking Shield"] = Show-Pack "Realistic Viking Shield" @("viking", "shield")
$results["Medieval Weapon Set"] = Show-Pack "Medieval weapon axe and shield set" @("medieval", "weapon", "shield")

Write-Host ""
Write-Host "ARMADURAS" -ForegroundColor Magenta
$results["Modular Armor"] = Show-Pack "Modular Medieval Armor" @("modular", "armor", "armour")
$results["Realistic Armor"] = Show-Pack "Realistic/PBR Medieval Armor" @("realistic", "pbr", "plate", "chainmail", "cuirass")

Write-Host ""
Write-Host "VFX / AUDIO / MUNDO" -ForegroundColor Magenta
$results["Atmospheric Music"] = Show-Pack "Atmoshpheric Worlds - FREE Game Music Pack" @("atmosh", "world", "music")
$results["Torch Fire"] = Show-Pack "Free Torch Fire" @("torch", "fire")
$results["Arrow Trail"] = Show-Pack "Free Arrow Trail" @("arrow", "trail")
$results["Desert Ruins"] = Show-Pack "Free Sample - Fantasy Desert Ruins" @("desert", "ruin", "temple")
$results["Sea Waves"] = Show-Pack "Procedural Sea Waves" @("sea", "wave", "ocean")
$results["Dark Statue"] = Show-Pack "Free Dark Fantasy Stone Statue, Pedestal, and Sword" @("statue", "pedestal", "dark")

Write-Host ""
Write-Host "FERRAMENTAS OPCIONAIS / NAO OBRIGATORIAS" -ForegroundColor DarkCyan
$results["Landscape Auto Material"] = Show-Pack "Advanced Landscape Auto Material (nao obrigatorio)" @("landscape", "automaterial", "auto_material")
$results["Bridge Creator"] = Show-Pack "Ultimate Bridge Creator (requer Houdini Engine)" @("ultimatebridge", "bridgecreator", "bridge_creator")

Write-Host ""
Write-Host ("Total de .uasset encontrados: {0}" -f $allAssets.Count) -ForegroundColor Cyan
$detected = @($results.GetEnumerator() | Where-Object { $_.Value }).Count
Write-Host ("Packs/grupos detectados: {0}/{1}" -f $detected, $results.Count) -ForegroundColor Cyan

if ($detected -lt $results.Count) {
    Write-Host ""
    Write-Host "IMPORTANTE:" -ForegroundColor Yellow
    Write-Host "Adicionar um item a Biblioteca da Epic/Fab nao copia automaticamente os .uasset para este projeto." -ForegroundColor Yellow
    Write-Host "No Unreal/Fab, use Add to Project / Adicionar ao Projeto para os packs desejados que ainda aparecem como FALTA/OPCIONAL." -ForegroundColor Yellow
    Write-Host "Depois execute este script novamente." -ForegroundColor Yellow
}
else {
    Write-Host ""
    Write-Host "Todos os grupos esperados foram encontrados dentro da pasta Content." -ForegroundColor Green
}

Write-Host ""
Write-Host "Politica visual do jogador: Realistic/PBR/4K/Medieval recebem prioridade; LowPoly/Stylized/Cartoon/Toon/Chibi ficam somente como fallback." -ForegroundColor DarkGray
Write-Host "Orc low-poly pode ser usado apenas como criatura/mob de fallback, nao como referencia visual do personagem do jogador." -ForegroundColor DarkGray
Write-Host "Ultimate Bridge Creator nao e dependencia do runtime: ele requer Houdini Engine e fica reservado para authoring/editor." -ForegroundColor DarkGray
Write-Host "Advanced Landscape Auto Material tambem fica opcional; o mundo continua com PCG/geracao C++ mesmo sem ele." -ForegroundColor DarkGray
Write-Host "O runtime faz autodeteccao via Asset Registry e possui fallback quando um asset nao estiver instalado." -ForegroundColor DarkGray
