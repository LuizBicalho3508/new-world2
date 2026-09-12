#!/usr/bin/env bash
set -euo pipefail

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
printf "%b NEW WORLD 2 - VERIFICACAO DE ASSETS FAB / EPIC - LINUX%b\n" "$C_CYAN" "$C_RESET"
printf "%b============================================================%b\n" "$C_CYAN" "$C_RESET"
echo "Projeto: $PROJECT_ROOT"
echo

TMP_LIST="$(mktemp)"
trap 'rm -f "$TMP_LIST"' EXIT
find "$CONTENT_ROOT" -type f -iname '*.uasset' -print 2>/dev/null | tr '[:upper:]' '[:lower:]' > "$TMP_LIST"
ASSET_COUNT="$(wc -l < "$TMP_LIST" | tr -d ' ')"
DETECTED=0
TOTAL=0

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
    printf "%b[OK]       %-42s%b\n" "$C_GREEN" "$name" "$C_RESET"
  elif [[ "$mode" == "required" ]]; then
    printf "%b[FALTA]    %-42s%b\n" "$C_RED" "$name" "$C_RESET"
  else
    printf "%b[OPCIONAL] %-42s%b\n" "$C_YELLOW" "$name" "$C_RESET"
  fi
}

printf "%bPERSONAGENS / CRIATURAS%b\n" "$C_MAGENTA" "$C_RESET"
pack required "Paragon: Greystone" greystone paragon_greystone
pack optional "Paragon: Grux" paragongrux grux
pack optional "Paragon: Sparrow" paragonsparrow sparrow
pack optional "Paragon: Sevarog" paragonsevarog sevarog
pack optional "Paragon: Rampage" paragonrampage rampage
pack optional "Paragon: Khaimera" paragonkhaimera khaimera
pack optional "Paragon: Countess" paragoncountess countess
pack optional "Paragon: Revenant" paragonrevenant revenant
pack optional "Paragon: Serath" paragonserath serath
pack optional "Paragon: Terra" paragonterra terra
pack optional "Paragon: Minions" paragonminions minions
pack optional "Classic Medieval Knight Warrior" classicmedieval medievalknight knightwarrior
pack optional "Medieval King" medievalking medieval_king
pack optional "Wasteland Warrior" wastelandwarrior wasteland_warrior
pack optional "Orc Warrior Axe and Shield" orc warrior axe shield

echo
printf "%bARMAS / EQUIPAMENTOS%b\n" "$C_MAGENTA" "$C_RESET"
pack optional "Ethereal Recurve Bow" ethereal recurve
pack optional "Necromancer Bone Sword" necromancer bone_sword bonesword
pack optional "Free Sword Pack / Realistic Melee" freesword longsword long_sword sabre saber
pack optional "Short Sword" shortsword short_sword
pack optional "Medieval Dagger / Daggers" medievaldagger medieval_dagger daggers dagger
pack optional "Atris Swords" atris
pack optional "Medieval Iron/Wood Shield" shield iron wood
pack optional "Realistic Viking Shield" viking shield
pack optional "Thornblade Sword" thornblade
pack optional "Dark Knight Longsword" darkknight dark_knight longsword
pack optional "Free Fantasy Weapon Sample" fantasyweapon fantasy_weapon
pack optional "Modular/Realistic Armor" modular armor armour chainmail cuirass plate

echo
printf "%bVFX / AUDIO / MUNDO%b\n" "$C_MAGENTA" "$C_RESET"
pack optional "Free Arrow Trail" arrowtrail arrow_trail
pack optional "Free Torch Fire" torch fire flame
pack optional "Fantasy Desert Ruins" desert ruin temple
pack optional "Dark Fantasy Statue/Pedestal" statue pedestal
pack optional "Atmospheric Worlds Music" atmosh atmospheric worlds music
pack optional "Procedural Sea Waves" sea wave ocean water

echo
printf "%bTotal de .uasset encontrados: %s%b\n" "$C_CYAN" "$ASSET_COUNT" "$C_RESET"
printf "%bPacks/grupos detectados: %s/%s%b\n" "$C_CYAN" "$DETECTED" "$TOTAL" "$C_RESET"

echo
if (( DETECTED < TOTAL )); then
  printf "%bNo Linux, adicionar o item a Biblioteca Fab nao o copia para Content/.%b\n" "$C_YELLOW" "$C_RESET"
  printf "%bUse o plugin Fab para Linux dentro da UE 5.8 e escolha Add to Project.%b\n" "$C_YELLOW" "$C_RESET"
  printf "%bO jogo possui fallbacks e pode ser compilado/testado mesmo sem todos os packs.%b\n" "$C_GRAY" "$C_RESET"
else
  printf "%bTodos os grupos catalogados foram localizados em Content/.%b\n" "$C_GREEN" "$C_RESET"
fi
