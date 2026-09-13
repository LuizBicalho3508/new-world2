import unreal

ROOTS = [
    "/Game/KiteDemo/Environments/Trees",
    "/Game/KiteDemo/Environments/Foliage",
    "/Game/KiteDemo/Environments/Rocks",
    "/Game/EuropeanBeech",
    "/Game/EuropeanHornbeam",
    "/Game/Megascans",
    "/Game/Quixel",
]

registry = unreal.AssetRegistryHelpers.get_asset_registry()
material_lib = unreal.MaterialEditingLibrary
usage = unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES

processed_meshes = 0
processed_materials = set()
failures = []


def persist_material_usage(material):
    if not material:
        return
    path = material.get_path_name()
    if path in processed_materials:
        return
    processed_materials.add(path)

    try:
        if isinstance(material, unreal.Material):
            material_lib.set_base_material_usage(material, usage, True)
        elif isinstance(material, unreal.MaterialInstance):
            setter = getattr(material_lib, "set_material_usage_override", None)
            if setter:
                setter(material, usage, True, True)
        unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    except Exception as exc:
        failures.append(f"material {path}: {exc}")


for root in ROOTS:
    try:
        assets = registry.get_assets_by_path(root, recursive=True)
    except Exception as exc:
        failures.append(f"registry {root}: {exc}")
        continue

    for data in assets:
        try:
            asset = data.get_asset()
        except Exception as exc:
            failures.append(f"load {data.package_name}: {exc}")
            continue

        if not isinstance(asset, unreal.StaticMesh):
            continue

        # Only touch assets that may actually be instanced by the V9 environment.
        name = asset.get_name().lower()
        path = asset.get_path_name().lower()
        if not any(token in (name + " " + path) for token in (
            "tree", "pine", "beech", "hornbeam", "oak", "grass", "fern",
            "plant", "foliage", "bush", "shrub", "rock", "boulder", "stone", "cliff"
        )):
            continue

        try:
            for slot in asset.get_editor_property("static_materials"):
                persist_material_usage(slot.material_interface)
            unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
            processed_meshes += 1
        except Exception as exc:
            failures.append(f"mesh {asset.get_path_name()}: {exc}")

unreal.log_warning(
    f"[ENV-REPAIR-V9] meshes={processed_meshes} materials={len(processed_materials)} failures={len(failures)}"
)
for message in failures[:30]:
    unreal.log_warning(f"[ENV-REPAIR-V9] {message}")

# ExecutePythonScript exits after the script; no explicit editor quit is needed.
