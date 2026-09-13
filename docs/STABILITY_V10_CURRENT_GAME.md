# Stability V10 — current-game baseline

This pass freezes feature growth and stabilizes the current vertical slice before more systems are added.

## Regressions addressed from the 2026-09-13 BigLinux V9 playtest

- UE 5.8 invalid `/Engine/BasicShapes/Capsule` fallback removed from enemy/civilian paths and redirected for remaining legacy CDO references.
- Missing optional Sparrow assets are queried through Asset Registry before loading.
- Legacy Paragon XR/input compatibility is restored with XRBase + ResetVR/TurnRate/LookUpRate compatibility mappings.
- Runtime environment selection rejects editor icons, FX meshes, `Icon_Sock`, full-forest/impostor assets and other inappropriate meshes.
- Nanite is temporarily disabled in this Linux vertical-slice baseline because uncooked vendor vegetation triggered multi-minute mesh/8K texture builds at startup.
- Combat Niagara is restricted to a tiny Free_Magic/ArrowTrail whitelist. Runtime no longer mass-warms vendor Niagara systems below the map.
- Startup loading gate is PSO-only and bounded to a short window.
- Starter weapon state is now internally consistent: Greatsword + Daggers are actually equipped; Staff remains in the bag.
- Staff always receives a visible fallback if no suitable staff asset exists locally.
- Rigid torso/leg/glove armor fallbacks remain intentionally blocked; attaching a rigid mesh to a single bone caused the floating-armor bug. True visible modular armor still requires a compatible skeleton/retargeted armor set.

## Deliberate stability boundary

V10 does not attempt a risky skeleton migration. Greystone remains the active animated base when a complete neutral body + compatible AnimBlueprint is not installed. The next visual milestone should standardize the player skeleton and then migrate armor pieces as a coherent modular set.

## Test

Run `scripts/premium-v10-stability-test-biglinux.sh`. It performs an incremental UE 5.8 build, starts a clean non-profile playtest, and summarizes regressions from the generated runtime log.
