#!/usr/bin/env bash
set -Eeuo pipefail

PROJECT_ROOT="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
CONTENT_ROOT="$PROJECT_ROOT/Content"

if [[ ! -d "$CONTENT_ROOT" ]]; then
  echo "ERRO: pasta Content nao encontrada: $CONTENT_ROOT" >&2
  exit 1
fi

if [[ -t 1 ]]; then
  C_GREEN='\033[0;32m'; C_YELLOW='\033[0;33m'; C_RED='\033[0;31m'; C_CYAN='\033[0;36m'; C_MAGENTA='\033[0;35m'; C_GRAY='\033[0;90m'; C_RESET='\033[0m'
else
  C_GREEN=''; C_YELLOW=''; C_RED=''; C_CYAN=''; C_MAGENTA=''; C_GRAY=''; C_RESET=''
fi

printf "%b============================================================%b\n" "$C_CYAN" "$C_RESET"
printf "%b NEW WORLD 2 - AUDITORIA FAB / EPIC PREMIUM V7 - LINUX%b\n" "$C_CYAN" "$C_RESET"
printf "%b============================================================%b\n" "$C_CYAN" "$C_RESET"
echo "Projeto: $PROJECT_ROOT"
echo

TMP_LIST="$(mktemp)"
trap 'rm -f "$TMP_LIST"' EXIT
find "$CONTENT_ROOT" -type f -iname '*.uasset' -print 2>/dev/null | tr '[:upper:]' '[:lower:]' > "$TMP_LIST"
ASSET_COUNT="$(wc -l < "$TMP_LIST" | tr -d ' ')"
DETECTED=0
TOTAL=0
RECOMMENDED_MISSING=0

has_any() {
  local pattern
  for pattern in "$@"; do
    if grep -Fqi -- "$pattern" "$TMP_LIST"; then return 0; fi
  done
  return 1
}

pack() {
  local mode="$1"; shift
  local name="$1"; shift
  TOTAL=$((TOTAL + 1))
  if has_any "$@"; then
    DETECTED=$((DETECTED + 1))
    printf "%b[OK]         %-46s%b\n" "$C_GREEN" "$name" "$C_RESET"
  elif [[ "$mode" == "required" ]]; then
    printf "%b[FALTA]      %-46s%b\n" "$C_RED" "$name" "$C_RESET"
  elif [[ "$mode" == "recommended" ]]; then
    RECOMMENDED_MISSING=$((RECOMMENDED_MISSING + 1))
    printf "%b[RECOMENDO]  %-46s%b\n" "$C_YELLOW" "$name" "$C_RESET"
  else
    printf "%b[OPCIONAL]   %-46s%b\n" "$C_GRAY" "$name" "$C_RESET"
  fi
}

printf "%bPERSONAGEM / ANIMACAO%b\n" "$C_MAGENTA" "$C_RESET"
pack required    "Paragon: Greystone (base atual)" greystone paragon_greystone
pack recommended "Game Animation Sample" gameanimationsample game_animation_sample pose_search
pack recommended "Classic Medieval Knight Warrior" classic_medieval classicmedieval knight_warrior knightwarrior
pack optional    "Medieval King" medievalking medieval_king
pack optional    "Wasteland Warrior" wastelandwarrior wasteland_warrior
pack optional    "Crimson Knight Warrior" crimson_knight crimsonknight
pack optional    "Paragon: Terra" paragonterra terra
pack optional    "Paragon: Serath" paragonserath serath
pack optional    "Paragon: Sparrow" paragonsparrow sparrow

echo
printf "%bINIMIGOS / BOSSES - PRIORIDADE V7%b\n" "$C_MAGENTA" "$C_RESET"
pack recommended "Paragon: Minions" paragonminions minions
pack recommended "Paragon: Grux" paragongrux grux
pack recommended "Paragon: Khaimera" paragonkhaimera khaimera
pack recommended "Paragon: Rampage" paragonrampage rampage
pack recommended "Paragon: Sevarog" paragonsevarog sevarog
pack recommended "Paragon: Revenant" paragonrevenant revenant
pack recommended "Paragon: Countess" paragoncountess countess
pack optional    "Orc Warrior Axe and Shield" orc warrior axe shield

echo
printf "%bARMAS / EQUIPAMENTOS%b\n" "$C_MAGENTA" "$C_RESET"
pack recommended "Ethereal Recurve Bow" ethereal recurve longbow
pack recommended "Free Sword Pack / Realistic Melee" free_sword freesword longsword long_sword sabre saber
pack recommended "Medieval Dagger / Daggers" medievaldagger medieval_dagger daggers dagger
pack recommended "Realistic / Modular Armor" modular_armor modulararmor armor armour cuirass gauntlet
pack optional    "Necromancer Bone Sword" necromancer bone_sword bonesword
pack optional    "Short Sword" shortsword short_sword
pack optional    "Atris Swords" atris
pack optional    "Medieval Iron/Wood Shield" medieval shield iron wood
pack optional    "Realistic Viking Shield" viking shield
pack optional    "Thornblade Sword" thornblade
pack optional    "Dark Knight Longsword" darkknight dark_knight
pack optional    "Free Fantasy Weapon Sample" fantasyweapon fantasy_weapon
pack optional    "Free Melee Weapon Pack" meleeweapon melee_weapon tesseract

echo
printf "%bDUNGEON / MUNDO - PRIORIDADE VISUAL%b\n" "$C_MAGENTA" "$C_RESET"
pack recommended "Soul: Cave" soul_cave soulcave soul cave
pack recommended "Dungeon Environment / 135+ Assets" dungeon_environment dungeonenvironment packdev
pack recommended "Dark Fantasy Gothic Environment" gothic_environment gothicenvironment gothic kitbash
pack recommended "European Beech" european_beech europeanbeech beech
pack recommended "European Hornbeam" european_hornbeam europeanhornbeam hornbeam
pack optional    "ElderBoom Medieval Village" elderboom medieval_village medievalvillage
pack optional    "Fantasy Desert Ruins" desert_ruins desertruins lost_desert_temple
pack optional    "Dark Fantasy Statue/Pedestal" stone_statue statue pedestal
pack optional    "Open World Demo Collection" openworlddemocollection open_world_demo
pack optional    "Ultimate Bridge Creator" ultimate_bridge bridgecreator
pack optional    "Procedural Sea Waves" procedural_sea sea_waves ocean waves
pack optional    "Deformable Snow System" deformablesnowsystem deformable_snow

echo
printf "%bVFX / AUDIO%b\n" "$C_MAGENTA" "$C_RESET"
pack recommended "Free Arrow Trail" arrowtrail arrow_trail
pack recommended "Free Magic Niagara" free_magic free_magic_hit free_magic_aura
pack optional    "Niagara Examples Pack" niagaraexamples niagara_examples
pack optional    "Free Torch Fire" torch_fire torch fire flame
pack optional    "MagicCircle VFX" magiccircle magic_circle
pack optional    "Free Spline VFX" splinevfx spline_vfx
pack optional    "Realistic Sword SFX" sword_sound sword sound effects
pack optional    "Atmospheric Worlds Music" atmosphericworlds atmospheric_worlds atmosh

echo
printf "%bTotal de .uasset encontrados: %s%b\n" "$C_CYAN" "$ASSET_COUNT" "$C_RESET"
printf "%bPacks/grupos detectados: %s/%s%b\n" "$C_CYAN" "$DETECTED" "$TOTAL" "$C_RESET"
printf "%bRecomendados ainda ausentes: %s%b\n" "$C_CYAN" "$RECOMMENDED_MISSING" "$C_RESET"

echo
if (( RECOMMENDED_MISSING > 0 )); then
  printf "%bIMPORTANTE: item na Library Fab nao significa asset em Content/.%b\n" "$C_YELLOW" "$C_RESET"
  printf "%bAbra UE 5.8 -> Fab -> Library e use Add to Project nos itens [RECOMENDO].%b\n" "$C_YELLOW" "$C_RESET"
  printf "%bO GitHub guarda apenas codigo/config; os .uasset de fornecedor ficam locais.%b\n" "$C_GRAY" "$C_RESET"
else
  printf "%bTodos os grupos recomendados pelo Premium V7 foram reconhecidos.%b\n" "$C_GREEN" "$C_RESET"
fi

exit 0
