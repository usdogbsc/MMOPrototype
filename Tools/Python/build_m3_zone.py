"""
Milestone 3: builds the first adventure zone, /Game/MMO/Maps/Lvl_Thornwick. Run headless with the editor closed:

  UnrealEditor-Cmd.exe <path>/MMO.uproject -run=pythonscript -script="<path>/Tools/Python/build_m3_zone.py"

Re-running rebuilds the map from scratch (the map asset is replaced; shared materials/sounds are updated in place).

Layout (X = north, cm):
  Thornwick village  (0, 0)          start, buildings, forge, inn, farm, well, market
  Whispering Meadow  X 3000..11000   lone level-1 wolves, oaks, flowers, stone ruins
  Sentinel's Rise    (8200, 5400)    hill with a ruined watchtower
  Greywood           X 11000..20000  dense pines, wolf pairs, campsite, waterfall, Howling Den
  Fanghollow         (20200, 5500)   dark hollow, giant dead tree, Dire Wolf
  Rustvein Mine      (21700, -1800)  boarded mine entrance in the north cliff
"""

import math
import os
import random
import struct
import tempfile
import wave

import unreal

LOG = []
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
WORK_DIR = os.path.join(tempfile.gettempdir(), "mmo_m3_content")
os.makedirs(WORK_DIR, exist_ok=True)

MAP_PATH = "/Game/MMO/Maps/Lvl_Thornwick"
MAT_DIR = "/Game/MMO/World/Materials"
AUDIO_DIR = "/Game/MMO/Audio/Ambient"


def log(message):
    LOG.append(str(message))
    unreal.log("[MMO zone] " + str(message))


def load_or_create(name, folder, cls, factory):
    path = "%s/%s" % (folder, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    return ASSET_TOOLS.create_asset(name, folder, cls, factory)


# =====================================================================================================================
# Materials
# =====================================================================================================================

PALETTE = {
    # ground cover & plants
    "bark": (0.10, 0.06, 0.03), "bark_dark": (0.045, 0.03, 0.02), "dead_bark": (0.085, 0.075, 0.065),
    "leaf_oak": (0.09, 0.22, 0.035), "leaf_oak_light": (0.17, 0.33, 0.05), "leaf_pine": (0.025, 0.09, 0.035),
    "leaf_pine_dark": (0.015, 0.05, 0.025), "leaf_hollow": (0.02, 0.05, 0.035), "bush": (0.05, 0.15, 0.03),
    "grass_tuft": (0.13, 0.30, 0.045), "grass_dry": (0.30, 0.28, 0.07), "grass_dark": (0.05, 0.14, 0.03),
    "flower_red": (0.65, 0.04, 0.03), "flower_yellow": (0.85, 0.62, 0.04), "flower_white": (0.8, 0.8, 0.75),
    "flower_purple": (0.28, 0.08, 0.5),
    # stone & earth
    "rock": (0.16, 0.15, 0.14), "rock_dark": (0.06, 0.06, 0.06), "rock_moss": (0.09, 0.13, 0.06),
    "stone_light": (0.40, 0.38, 0.34), "stone_old": (0.25, 0.25, 0.23), "soil": (0.11, 0.065, 0.035),
    "gravel": (0.12, 0.11, 0.10), "bone": (0.72, 0.68, 0.58),
    # buildings
    "plaster": (0.5, 0.43, 0.32), "plaster_white": (0.55, 0.52, 0.46), "timber": (0.11, 0.065, 0.032),
    "planks": (0.27, 0.16, 0.075), "roof_red": (0.33, 0.065, 0.035), "roof_blue": (0.06, 0.11, 0.24),
    "roof_thatch": (0.42, 0.31, 0.11), "roof_slate": (0.09, 0.10, 0.115), "door": (0.08, 0.045, 0.022),
    "window": (0.02, 0.025, 0.035), "metal": (0.045, 0.045, 0.05), "rust": (0.20, 0.08, 0.03),
    "cloth_red": (0.45, 0.06, 0.05), "cloth_green": (0.07, 0.22, 0.07), "cloth_cream": (0.68, 0.6, 0.44),
    "cloth_blue": (0.08, 0.14, 0.4), "hay": (0.58, 0.44, 0.12), "crop": (0.18, 0.40, 0.05),
    "pumpkin": (0.8, 0.28, 0.03), "apple": (0.6, 0.05, 0.03), "ash": (0.03, 0.03, 0.03),
    # water & void
    "water": (0.025, 0.11, 0.2), "foam": (0.8, 0.85, 0.9), "void": (0.0, 0.0, 0.0),
}
ROUGHNESS = {"water": 0.05, "metal": 0.8, "foam": 0.6, "void": 1.0}
# scene luminance in daylight is ~0.5 nit (EV100 ~0.6), so emissives stay gentle
GLOW = {"glow_window": ((1.0, 0.62, 0.25), 0.9), "glow_ember": ((1.0, 0.22, 0.02), 5.0), "glow_lantern": ((1.0, 0.7, 0.3), 2.5),
        "glow_water": ((0.06, 0.32, 0.55), 0.45), "glow_foam": ((0.8, 0.9, 1.0), 0.6)}

MATS = {}


def build_materials():
    base = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    for key, rgb in PALETTE.items():
        mic = load_or_create("MI_MMO_" + key, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mic, base)
        MEL.set_material_instance_vector_parameter_value(mic, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        MEL.set_material_instance_scalar_parameter_value(mic, "Roughness", ROUGHNESS.get(key, 0.85))
        unreal.EditorAssetLibrary.save_loaded_asset(mic, False)
        MATS[key] = mic

    # emissive parent for windows, embers and lanterns
    glow = load_or_create("M_MMO_Glow", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(glow)
    color = MEL.create_material_expression(glow, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1, 0.7, 0.3, 1))
    intensity = MEL.create_material_expression(glow, unreal.MaterialExpressionScalarParameter, -500, 200)
    intensity.set_editor_property("parameter_name", "Intensity")
    intensity.set_editor_property("default_value", 8.0)
    multiply = MEL.create_material_expression(glow, unreal.MaterialExpressionMultiply, -250, 100)
    MEL.connect_material_expressions(color, "", multiply, "A")
    MEL.connect_material_expressions(intensity, "", multiply, "B")
    MEL.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.recompile_material(glow)
    unreal.EditorAssetLibrary.save_loaded_asset(glow, False)
    for key, (rgb, strength) in GLOW.items():
        mic = load_or_create("MI_MMO_" + key, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mic, glow)
        MEL.set_material_instance_vector_parameter_value(mic, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        MEL.set_material_instance_scalar_parameter_value(mic, "Intensity", strength)
        unreal.EditorAssetLibrary.save_loaded_asset(mic, False)
        MATS[key] = mic

    # terrain: vertex colors
    terrain = load_or_create("M_MMO_TerrainVertexColor", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(terrain)
    vertex = MEL.create_material_expression(terrain, unreal.MaterialExpressionVertexColor, -400, 0)
    MEL.connect_material_property(vertex, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = MEL.create_material_expression(terrain, unreal.MaterialExpressionConstant, -400, 200)
    rough.set_editor_property("r", 0.95)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(terrain)
    unreal.EditorAssetLibrary.save_loaded_asset(terrain, False)
    MATS["terrain"] = terrain
    log("Materials: %d palette + glow + terrain" % len(PALETTE))


# =====================================================================================================================
# Sounds (original, synthesized here)
# =====================================================================================================================

RATE = 32000


def _noise(n, rng, alpha):
    out, y = [], 0.0
    for _ in range(n):
        y += alpha * (rng.uniform(-1, 1) - y)
        out.append(y)
    return out


def _add(base, layer, at, gain=1.0):
    start = int(at * RATE)
    for i, v in enumerate(layer):
        j = start + i
        if j < len(base):
            base[j] += v * gain
        else:
            base[j % len(base)] += v * gain  # wrap so loops stay seamless
    return base


def _chirp(rng, f0, f1, dur, gain=1.0):
    n, out, phase = int(dur * RATE), [], 0.0
    for i in range(n):
        t = i / n
        phase += 2 * math.pi * (f0 + (f1 - f0) * t) / RATE
        out.append(math.sin(phase) * math.sin(math.pi * t) * gain)
    return out


def _tone(f, dur, decay, harmonics=(1.0,), attack=0.005):
    n = int(dur * RATE)
    return [sum(a * math.sin(2 * math.pi * f * (k + 1) * i / RATE) for k, a in enumerate(harmonics)) * min(1.0, i / (attack * RATE)) * math.exp(-decay * i / n) for i in range(n)]


def _normalize(samples, peak):
    m = max(1e-6, max(abs(v) for v in samples))
    return [v / m * peak for v in samples]


def _loop_wind(rng, seconds, alpha, depth, gust_rate):
    n = int(seconds * RATE)
    base = _noise(n + RATE, rng, alpha)
    out = []
    for i in range(n):
        t = i / RATE
        swell = 0.55 + depth * (0.5 + 0.5 * math.sin(2 * math.pi * gust_rate * t + 1.3 * math.sin(2 * math.pi * 0.07 * t)))
        out.append(base[i] * swell)
    # crossfade the tail into the head so the loop is seamless
    fade = RATE
    for i in range(fade):
        w = i / fade
        out[i] = out[i] * w + base[n + i] * swell * (1 - w)
    return out


def build_sounds():
    rng = random.Random(99)
    sounds = {}

    sounds["S_MMO_Amb_Wind"] = _normalize(_loop_wind(rng, 14, 0.04, 0.45, 0.09), 0.5)

    meadow = [v * 0.25 for v in _loop_wind(rng, 16, 0.03, 0.3, 0.06)]
    for _ in range(26):
        at, f = rng.uniform(0, 16), rng.uniform(2600, 4800)
        for k in range(rng.randint(2, 5)):
            _add(meadow, _chirp(rng, f, f * rng.uniform(1.1, 1.5), rng.uniform(0.05, 0.11), 0.7), at + k * 0.12)
    sounds["S_MMO_Amb_Meadow"] = _normalize(meadow, 0.45)

    village = [v * 0.18 for v in _loop_wind(rng, 16, 0.03, 0.2, 0.05)]
    for _ in range(12):
        at, f = rng.uniform(0, 16), rng.uniform(3000, 4200)
        for k in range(3):
            _add(village, _chirp(rng, f, f * 1.2, 0.07, 0.5), at + k * 0.1)
    for k in range(5):  # distant forge hammer
        at = 1.5 + k * 3.1
        for hit in range(3):
            _add(village, _tone(1650, 0.35, 9, (1.0, 0.5, 0.3)), at + hit * 0.55, 0.22)
    sounds["S_MMO_Amb_Village"] = _normalize(village, 0.4)

    woods = [v * 0.6 for v in _loop_wind(rng, 18, 0.015, 0.6, 0.05)]
    for _ in range(4):  # woodpecker
        at = rng.uniform(0, 18)
        for k in range(10):
            _add(woods, _tone(rng.uniform(900, 1100), 0.04, 20, (1.0, 0.4)), at + k * 0.06, 0.3)
    for _ in range(10):  # distant birds
        at, f = rng.uniform(0, 18), rng.uniform(2000, 3000)
        _add(woods, _chirp(rng, f, f * 0.8, 0.25, 0.25), at)
    sounds["S_MMO_Amb_Woods"] = _normalize(woods, 0.45)

    hollow_n = 18 * RATE
    hollow = [0.35 * math.sin(2 * math.pi * 55 * i / RATE) * (0.6 + 0.4 * math.sin(2 * math.pi * 0.11 * i / RATE)) + 0.25 * math.sin(2 * math.pi * 82.4 * i / RATE) for i in range(hollow_n)]
    hollow = [a + b * 0.4 for a, b in zip(hollow, _loop_wind(rng, 18, 0.01, 0.5, 0.04))]
    for at in (3.0, 11.5):  # distant howl
        n = int(2.6 * RATE)
        phase, howl = 0.0, []
        for i in range(n):
            t = i / n
            f = 420 + 180 * math.sin(math.pi * t) + 6 * math.sin(2 * math.pi * 5 * i / RATE)
            phase += 2 * math.pi * f / RATE
            howl.append((math.sin(phase) + 0.3 * math.sin(2 * phase)) * math.sin(math.pi * t) ** 1.5)
        _add(hollow, howl, at, 0.35)
    for _ in range(40):  # crickets
        at = rng.uniform(0, 18)
        for k in range(4):
            _add(hollow, _tone(4600, 0.03, 6), at + k * 0.045, 0.06)
    sounds["S_MMO_Amb_DeepWoods"] = _normalize(hollow, 0.45)

    mine = [v * 1.0 for v in _loop_wind(rng, 14, 0.006, 0.7, 0.03)]
    for _ in range(9):  # drips
        _add(mine, _tone(rng.uniform(1400, 2200), 0.25, 14, (1.0, 0.3)), rng.uniform(0, 14), 0.25)
    sounds["S_MMO_Amb_Mine"] = _normalize(mine, 0.45)

    # discovery fanfare (one-shot)
    fanfare = [0.0] * int(2.2 * RATE)
    for k, f in enumerate([392.0, 523.25, 659.25, 783.99]):
        _add(fanfare, _tone(f, 1.6, 3.5, (1.0, 0.45, 0.2)), 0.11 * k, 0.4)
    sounds["S_MMO_Discovery"] = _normalize(fanfare, 0.6)

    tasks = []
    for name, samples in sounds.items():
        path = os.path.join(WORK_DIR, name + ".wav")
        with wave.open(path, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32767)) for v in samples))
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = "/Game/MMO/Audio" if name == "S_MMO_Discovery" else AUDIO_DIR
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        tasks.append(task)
    ASSET_TOOLS.import_asset_tasks(tasks)
    for task in tasks:
        for path in task.imported_object_paths:
            wave_asset = unreal.load_asset(path)
            wave_asset.set_editor_property("looping", "Amb_" in path)
            unreal.EditorAssetLibrary.save_loaded_asset(wave_asset, False)
    log("Sounds: %s" % ", ".join(sounds))


def sound(name):
    return unreal.load_asset("%s/%s" % (AUDIO_DIR, name))


# =====================================================================================================================
# Dire Wolf loot
# =====================================================================================================================

def build_dire_loot():
    table = load_or_create("DA_Loot_DireWolf", "/Game/MMO/Loot", unreal.MMOLootTable, unreal.DataAssetFactory())
    entries = []
    for item_id, chance, qmin, qmax in [("WolfPelt", 1.0, 1, 2), ("WolfFang", 0.75, 1, 3), ("RawWolfMeat", 0.6, 1, 2),
                                         ("WornLeatherBoots", 0.15, 1, 1), ("Greyfang", 0.08, 1, 1)]:
        entry = unreal.MMOLootEntry()
        entry.set_editor_property("item", unreal.load_asset("/Game/MMO/Items/DA_Item_" + item_id))
        entry.set_editor_property("drop_chance", chance)
        entry.set_editor_property("min_quantity", qmin)
        entry.set_editor_property("max_quantity", qmax)
        entries.append(entry)
    table.set_editor_property("entries", entries)
    table.set_editor_property("currency_chance", 0.95)
    table.set_editor_property("min_currency", 25)
    table.set_editor_property("max_currency", 60)
    unreal.EditorAssetLibrary.save_loaded_asset(table, False)
    log("Dire Wolf loot table")


# =====================================================================================================================
# Level helpers
# =====================================================================================================================

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYL = unreal.load_asset("/Engine/BasicShapes/Cylinder")
CONE = unreal.load_asset("/Engine/BasicShapes/Cone")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere")
CHAMFER = unreal.load_asset("/Game/LevelPrototyping/Meshes/SM_ChamferCube")
TERRAIN = None
COUNT = [0]


def ground(x, y):
    return TERRAIN.get_height_at(x, y) if TERRAIN else 0.0


def part(mesh, loc, size, mat, rot=(0, 0, 0), folder="Props", collide=True, label=None, shadow=True):
    """Places a centered 100cm basic mesh: loc = center (x, y, z), size = (x, y, z) in cm, rot = (pitch, yaw, roll)."""
    actor = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), unreal.Rotator(rot[2], rot[0], rot[1]))
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(mesh)
    component.set_material(0, MATS[mat])
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    if not collide:
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    if not shadow:
        component.set_editor_property("cast_shadow", False)
    actor.set_folder_path(folder)
    if label:
        actor.set_actor_label(label)
    COUNT[0] += 1
    return actor


class Frame:
    """Local building frame: x forward (door side), y right, z up from the ground at the origin."""

    def __init__(self, x, y, yaw, z=None):
        self.x, self.y, self.yaw = x, y, yaw
        self.z = ground(x, y) if z is None else z
        self.c, self.s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def at(self, lx, ly, lz):
        return (self.x + lx * self.c - ly * self.s, self.y + lx * self.s + ly * self.c, self.z + lz)

    def put(self, mesh, lx, ly, lz, size, mat, pitch=0, yaw=0, roll=0, **kw):
        # rotator order (pitch, yaw, roll); unreal applies roll, pitch, then yaw (so local tilt, then building yaw)
        return part(mesh, self.at(lx, ly, lz), size, mat, (pitch, self.yaw + yaw, roll), **kw)


def gable_roof(f, w, d, top, roof_mat, wall_mat, folder, overhang=45):
    """Gable roof over a w (local x) by d (local y) footprint; ridge along local y, 45 degree pitch."""
    side = w / math.sqrt(2)
    f.put(CUBE, 0, 0, top, (side, d, side), wall_mat, pitch=45, folder=folder)  # gable prism (lower half hidden in walls)
    slab = (w / 2) * math.sqrt(2) + overhang
    for sign in (-1, 1):
        lx = sign * (w / 4 + 6)
        f.put(CUBE, lx, 0, top + w / 4 + 6, (slab, d + overhang * 2, 16), roof_mat, pitch=-45 * sign, folder=folder)
    f.put(CUBE, 0, 0, top + w / 2 + 8, (24, d + overhang * 2 + 4, 24), "timber", pitch=45, folder=folder)  # ridge cap


def house(x, y, yaw, w, d, h, roof, wall="plaster", folder="Village/House", chimney=True, two_story=False, door_mat="door"):
    f = Frame(x, y, yaw)
    base = 30
    f.put(CHAMFER, 0, 0, 5, (w + 50, d + 50, 60), "stone_old", folder=folder)
    f.put(CUBE, 0, 0, base + h / 2, (w, d, h), wall, folder=folder)
    for sx in (-1, 1):
        for sy in (-1, 1):
            f.put(CUBE, sx * w / 2, sy * d / 2, base + h / 2, (24, 24, h), "timber", folder=folder)
    if two_story:
        for sx in (-1, 1):
            f.put(CUBE, sx * (w / 2 + 2), 0, base + h * 0.5, (12, d, 18), "timber", folder=folder)
        for sy in (-1, 1):
            f.put(CUBE, 0, sy * (d / 2 + 2), base + h * 0.5, (w, 12, 18), "timber", folder=folder)
    # door on the front (+x) and windows
    f.put(CUBE, w / 2 + 4, 0, base + 105, (10, 120, 210), door_mat, folder=folder)
    f.put(CUBE, w / 2 + 8, 0, base + 215, (12, 150, 14), "timber", folder=folder)
    rows = [base + h * 0.55] if not two_story else [base + h * 0.27, base + h * 0.75]
    for z in rows:
        for wy in (-d * 0.3, d * 0.3):
            f.put(CUBE, w / 2 + 3, wy, z, (6, 80, 80), "glow_window", folder=folder, shadow=False)
            f.put(CUBE, w / 2 + 6, wy, z, (6, 90, 8), "timber", folder=folder)
        for wx in (-w * 0.22, w * 0.22):
            for side in (-1, 1):
                f.put(CUBE, wx, side * (d / 2 + 3), z, (70, 6, 70), "glow_window", folder=folder, shadow=False)
    gable_roof(f, w, d, base + h, roof, wall, folder)
    if chimney:
        f.put(CHAMFER, -w * 0.2, d * 0.28, base + h + w / 2 * 0.7, (70, 70, w / 2 + 160), "stone_old", folder=folder)
    return f


def barrel(x, y, folder="Village/Props", mat="planks"):
    z = ground(x, y)
    part(CYL, (x, y, z + 50), (70, 70, 100), mat, folder=folder)
    for band in (25, 75):
        part(CYL, (x, y, z + band), (74, 74, 8), "metal", folder=folder, collide=False)


def crate(x, y, size=80, yaw=0, folder="Village/Props", stack=0):
    z = ground(x, y)
    part(CUBE, (x, y, z + size / 2 + stack * size), (size, size, size), "planks", rot=(0, yaw, 0), folder=folder)


def lamp_post(x, y, folder="Village/Lamps"):
    z = ground(x, y)
    part(CYL, (x, y, z + 140), (16, 16, 280), "metal", folder=folder)
    part(CUBE, (x, y, z + 295), (30, 30, 34), "glow_lantern", folder=folder, shadow=False)
    part(CUBE, (x, y, z + 318), (40, 40, 8), "metal", folder=folder, collide=False)
    light = ACTORS.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z + 290))
    light.get_component_by_class(unreal.PointLightComponent).set_editor_property("intensity", 18.0)
    light.get_component_by_class(unreal.PointLightComponent).set_editor_property("light_color", unreal.Color(255, 180, 110, 255))
    light.get_component_by_class(unreal.PointLightComponent).set_editor_property("attenuation_radius", 700.0)
    light.get_component_by_class(unreal.PointLightComponent).set_editor_property("cast_shadows", False)
    light.set_folder_path(folder)


def fence_line(points, folder="Village/Fence", spacing=180):
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        length = math.hypot(bx - ax, by - ay)
        yaw = math.degrees(math.atan2(by - ay, bx - ax))
        posts = max(1, int(length // spacing))
        for i in range(posts + 1):
            t = i / posts
            px, py = ax + (bx - ax) * t, ay + (by - ay) * t
            part(CUBE, (px, py, ground(px, py) + 55), (16, 16, 110), "timber", folder=folder)
        mx, my = (ax + bx) / 2, (ay + by) / 2
        for rail in (45, 85):
            part(CUBE, (mx, my, ground(mx, my) + rail), (length, 8, 10), "planks", rot=(0, yaw, 0), folder=folder, collide=False)


def rock(x, y, size, mat="rock", folder="Landmarks/Rocks", yaw=None, tilt=12, rng=random):
    z = ground(x, y)
    yaw = rng.uniform(0, 360) if yaw is None else yaw
    part(CHAMFER, (x, y, z + size[2] * 0.3), size, mat, rot=(rng.uniform(-tilt, tilt), yaw, rng.uniform(-tilt, tilt)), folder=folder)


# =====================================================================================================================
# Terrain, light, atmosphere
# =====================================================================================================================

def make_struct(cls, **props):
    value = cls()
    for key, v in props.items():
        value.set_editor_property(key, v)
    return value


def V2(x, y):
    return unreal.Vector2D(x, y)


def build_terrain():
    global TERRAIN
    t = ACTORS.spawn_actor_from_class(unreal.MMOTerrain, unreal.Vector(0, 0, 0))
    t.set_actor_label("Terrain")
    t.set_editor_property("material", MATS["terrain"])
    t.set_editor_property("rock_color", unreal.LinearColor(0.075, 0.07, 0.065, 1.0))
    t.set_editor_property("main_road", [V2(-3600, 250), V2(0, 0), V2(2600, 150), V2(4600, 700), V2(7000, 1000), V2(9200, 300),
                                         V2(11200, 0), V2(13000, 600), V2(15000, 1200), V2(17000, 300), V2(19000, -600),
                                         V2(20600, -1500), V2(21450, -1800)])
    # spur up to the watchtower, a hidden trail past the den to the waterfall, and a trail into Fanghollow
    t.set_editor_property("side_road", [V2(7000, 1000), V2(7600, 2900), V2(8100, 4600)])
    t.set_editor_property("trail", [V2(13000, 600), V2(13500, -2400), V2(14700, -4300), V2(15900, -4700), V2(14700, -4300),
                                     V2(14100, -6000), V2(13650, -6450)])
    t.set_editor_property("trail2", [V2(17000, 300), V2(18100, 2500), V2(19300, 4300), V2(20000, 5400)])
    hills = [((8200, 5400), 2700, 650), ((5200, -4300), 2300, 260), ((9900, -3600), 2500, 380), ((3800, 3800), 1800, 200),
             ((14800, -2400), 3000, 520), ((17700, -5800), 2800, 750), ((19300, 2300), 2100, 320), ((12300, 4300), 2200, 450),
             ((20200, 5600), 2300, -260), ((13600, -8300), 2000, 900)]
    t.set_editor_property("hills", [make_struct(unreal.MMOTerrainHill, center=V2(*c), radius=r, height=h) for c, r, h in hills])
    flats = [((0, 0), 3300, 0.0, (0, 0, 0, 0)), ((300, 0), 1500, 0.0, (0.26, 0.21, 0.13, 0.85)),
             ((8200, 5400), 1000, 650.0, (0.22, 0.20, 0.15, 0.5)), ((21200, -1800), 950, 60.0, (0.17, 0.14, 0.10, 0.8))]
    t.set_editor_property("flat_areas", [make_struct(unreal.MMOTerrainFlat, center=V2(*c), radius=r, height=h, tint=unreal.LinearColor(*tint))
                                          for c, r, h, tint in flats])
    biomes = [((0, 0), 4500, (0.17, 0.33, 0.07), 0.0), ((7200, 0), 6500, (0.21, 0.40, 0.06), 90.0),
              ((15800, -1500), 6500, (0.075, 0.17, 0.04), 220.0), ((20200, 5500), 3600, (0.04, 0.09, 0.045), 150.0),
              ((20800, -1600), 2600, (0.16, 0.14, 0.09), 100.0)]
    t.set_editor_property("biomes", [make_struct(unreal.MMOTerrainBiome, center=V2(*c), radius=r, grass_color=unreal.LinearColor(*col, 1.0), roughness=rough)
                                      for c, r, col, rough in biomes])
    t.rebuild()
    TERRAIN = t

    # carve a level basin for the waterfall pool at whatever height the ridge foot ended up
    pool_height = ground(13650, -6200) - 30
    flat_list = list(t.get_editor_property("flat_areas"))
    flat_list.append(make_struct(unreal.MMOTerrainFlat, center=V2(13650, -6850), radius=750, height=pool_height, tint=unreal.LinearColor(0.12, 0.1, 0.07, 0.6)))
    t.set_editor_property("flat_areas", flat_list)
    t.rebuild()
    log("Terrain built; village height %.0f, mine pad %.0f, tower %.0f" % (ground(0, 0), ground(21200, -1800), ground(8200, 5400)))


def build_atmosphere():
    sun = ACTORS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(0, -42, 28))
    sun.set_actor_label("Sun")
    c = sun.get_component_by_class(unreal.DirectionalLightComponent)
    c.set_editor_property("intensity", 7.5)
    c.set_editor_property("light_color", unreal.Color(255, 234, 205, 255))
    c.set_editor_property("atmosphere_sun_light", True)
    c.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)

    ACTORS.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label("SkyAtmosphere")

    sky = ACTORS.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 2000))
    sky.set_actor_label("SkyLight")
    sc = sky.get_component_by_class(unreal.SkyLightComponent)
    sc.set_editor_property("real_time_capture", True)
    sc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sc.set_editor_property("intensity", 1.35)

    fog = ACTORS.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -200))
    fog.set_actor_label("HeightFog")
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property("fog_density", 0.006)
    fc.set_editor_property("fog_height_falloff", 0.2)
    fc.set_editor_property("start_distance", 3500.0)
    fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.035, 0.045, 0.06, 1.0))
    fc.set_editor_property("directional_inscattering_luminance", unreal.LinearColor(0.06, 0.04, 0.02, 1.0))

    try:
        ACTORS.spawn_actor_from_class(unreal.VolumetricCloud, unreal.Vector(0, 0, 0)).set_actor_label("Clouds")
    except Exception as error:
        log("No volumetric clouds: %s" % error)

    post = ACTORS.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    post.set_actor_label("PostProcess")
    post.set_editor_property("unbound", True)
    settings = post.settings
    for key, value in [("override_bloom_intensity", True), ("bloom_intensity", 0.55),
                       ("override_vignette_intensity", True), ("vignette_intensity", 0.3),
                       ("override_color_saturation", True), ("color_saturation", unreal.Vector4(1.12, 1.1, 1.05, 1.0)),
                       ("override_white_temp", True), ("white_temp", 6100.0),
                       ("override_auto_exposure_bias", True), ("auto_exposure_bias", 0.0),
                       # sunny meadow ~EV 0.6; let shade brighten only a little so forests stay moody
                       ("override_auto_exposure_min_brightness", True), ("auto_exposure_min_brightness", -1.8),
                       ("override_auto_exposure_max_brightness", True), ("auto_exposure_max_brightness", 2.5)]:
        try:
            settings.set_editor_property(key, value)
        except Exception as error:
            log("Post setting %s skipped: %s" % (key, error))
    post.set_editor_property("settings", settings)
    log("Atmosphere and lighting")


# =====================================================================================================================
# Village: Thornwick
# =====================================================================================================================

def build_village():
    rng = random.Random(5)
    plaza = (300, 0)

    def facing(x, y):
        return math.degrees(math.atan2(plaza[1] - y, plaza[0] - x))

    # inn (two storeys, west side of the square)
    inn = house(-1100, -900, facing(-1100, -900), 900, 1300, 620, "roof_red", wall="plaster_white", folder="Village/Inn", two_story=True)
    inn.put(CUBE, 640, 0, 330, (260, 1200, 14), "planks", folder="Village/Inn")  # porch roof
    for py in (-560, -200, 200, 560):
        inn.put(CUBE, 760, py, 170, (18, 18, 330), "timber", folder="Village/Inn")
    inn.put(CUBE, 860, 520, 300, (16, 16, 260), "timber", folder="Village/Inn")
    inn.put(CUBE, 860, 410, 410, (12, 230, 12), "timber", folder="Village/Inn")
    inn.put(CUBE, 862, 380, 335, (8, 110, 80), "cloth_red", folder="Village/Inn", collide=False)  # hanging sign
    inn.put(CYL, 868, 380, 338, (30, 30, 36), "hay", folder="Village/Inn", collide=False)       # mug emblem
    for i in range(3):
        barrel(*inn.at(700, -700 + i * 80, 0)[:2], folder="Village/Inn")

    # houses around the square
    house(-200, 1500, facing(-200, 1500), 620, 760, 360, "roof_blue", folder="Village/House_North")
    house(1700, 1650, facing(1700, 1650), 560, 680, 340, "roof_thatch", folder="Village/House_NorthEast")
    house(-1600, 900, facing(-1600, 900), 600, 700, 360, "roof_slate", folder="Village/House_West")
    house(1500, -1700, facing(1500, -1700), 680, 820, 400, "roof_thatch", wall="plaster_white", folder="Village/House_SouthEast")
    house(-400, -2100, facing(-400, -2100), 580, 700, 350, "roof_blue", folder="Village/House_South")

    # forge (east side): open shed with hearth, anvil and racks
    forge = Frame(1900, 300, facing(1900, 300))
    fold = "Village/Forge"
    for lx in (-300, 300):
        for ly in (-350, 350):
            forge.put(CUBE, lx, ly, 200, (30, 30, 400), "timber", folder=fold)
    forge.put(CUBE, 0, 0, 430, (760, 860, 18), "roof_slate", pitch=-10, folder=fold)
    forge.put(CHAMFER, -330, 0, 150, (60, 860, 300), "stone_old", folder=fold)  # back wall
    forge.put(CHAMFER, -180, -150, 60, (220, 240, 130), "stone_old", folder=fold)  # hearth
    forge.put(CUBE, -180, -150, 128, (170, 190, 10), "glow_ember", folder=fold, shadow=False)
    forge.put(CHAMFER, -230, -150, 420, (110, 110, 700), "stone_old", folder=fold)  # chimney
    forge.put(CUBE, 80, 120, 35, (45, 45, 70), "metal", folder=fold)  # anvil
    forge.put(CUBE, 80, 120, 80, (100, 40, 26), "metal", folder=fold)
    forge.put(CYL, 160, -240, 45, (90, 90, 90), "planks", folder=fold)  # quench barrel
    forge.put(CYL, 160, -240, 91, (80, 80, 4), "water", folder=fold, collide=False)
    forge.put(CUBE, -60, 330, 120, (16, 140, 16), "timber", folder=fold)  # weapon rack
    for i in range(4):
        forge.put(CUBE, -60, 280 + i * 30, 130, (6, 6, 150), "metal", pitch=0, roll=8, folder=fold, collide=False)
    ember = ACTORS.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*forge.at(-150, -150, 180)))
    ember.get_component_by_class(unreal.PointLightComponent).set_editor_property("intensity", 40.0)
    ember.get_component_by_class(unreal.PointLightComponent).set_editor_property("light_color", unreal.Color(255, 110, 40, 255))
    ember.get_component_by_class(unreal.PointLightComponent).set_editor_property("attenuation_radius", 900.0)
    ember.set_folder_path(fold)

    # well in the square
    wz = ground(*plaza)
    part(CYL, (plaza[0], plaza[1], wz + 45), (240, 240, 90), "stone_light", folder="Village/Well")
    part(CYL, (plaza[0], plaza[1], wz + 88), (190, 190, 6), "water", folder="Village/Well", collide=False)
    for py in (-110, 110):
        part(CUBE, (plaza[0], plaza[1] + py, wz + 170), (20, 20, 250), "timber", folder="Village/Well")
    part(CUBE, (plaza[0], plaza[1], wz + 300), (200, 260, 14), "roof_red", rot=(0, 0, 0), folder="Village/Well")
    part(CYL, (plaza[0], plaza[1], wz + 230), (16, 16, 220), "timber", rot=(0, 0, 90), folder="Village/Well", collide=False)

    # market stall and notice board
    stall = Frame(900, -950, facing(900, -950))
    for lx in (-120, 120):
        for ly in (-180, 180):
            stall.put(CUBE, lx, ly, 120, (14, 14, 240), "timber", folder="Village/Market")
    stall.put(CUBE, 0, 0, 245, (300, 420, 10), "cloth_red", pitch=-12, folder="Village/Market")
    stall.put(CUBE, 30, 0, 80, (120, 340, 14), "planks", folder="Village/Market")
    for i, mat in enumerate(["apple", "pumpkin", "crop", "apple"]):
        stall.put(SPHERE, 30, -120 + i * 80, 100, (38, 38, 30), mat, folder="Village/Market", collide=False)
    board = Frame(700, 900, facing(700, 900))
    for ly in (-80, 80):
        board.put(CUBE, 0, ly, 110, (14, 14, 220), "timber", folder="Village/Board")
    board.put(CUBE, 0, 0, 160, (8, 190, 110), "planks", folder="Village/Board")
    for i in range(3):
        board.put(CUBE, 6, -50 + i * 50, 170, (2, 34, 44), "cloth_cream", folder="Village/Board", collide=False)

    # farm and garden (south-west)
    farm_x, farm_y = -1900, -2700
    fz = ground(farm_x, farm_y)
    part(CUBE, (farm_x, farm_y, fz + 3), (1500, 1100, 10), "soil", folder="Village/Farm", collide=False)
    for row in range(7):
        ry = farm_y - 450 + row * 150
        for k in range(9):
            rx = farm_x - 640 + k * 160
            crop = "pumpkin" if (row == 2 and k % 2 == 0) else "crop"
            shape = SPHERE if crop == "pumpkin" else CONE
            part(shape, (rx, ry, fz + 30), (45, 45, 55), crop, folder="Village/Farm", collide=False, shadow=False)
    fence_line([(farm_x - 800, farm_y - 600), (farm_x + 800, farm_y - 600), (farm_x + 800, farm_y + 600), (farm_x + 200, farm_y + 600)], folder="Village/Farm")
    fence_line([(farm_x - 300, farm_y + 600), (farm_x - 800, farm_y + 600), (farm_x - 800, farm_y - 600)], folder="Village/Farm")
    sc = Frame(farm_x + 100, farm_y + 100, 30)
    sc.put(CUBE, 0, 0, 110, (12, 12, 220), "timber", folder="Village/Farm")
    sc.put(CUBE, 0, 0, 170, (12, 160, 12), "timber", folder="Village/Farm")
    sc.put(CUBE, 0, 0, 150, (30, 90, 70), "cloth_blue", folder="Village/Farm")
    sc.put(SPHERE, 0, 0, 225, (40, 40, 40), "hay", folder="Village/Farm")
    sc.put(CONE, 0, 0, 255, (60, 60, 40), "hay", folder="Village/Farm")
    for hx, hy in [(farm_x + 1050, farm_y - 300), (farm_x + 1100, farm_y + 50)]:
        part(CYL, (hx, hy, ground(hx, hy) + 70), (160, 160, 140), "hay", folder="Village/Farm")

    # village gate on the north road, palisade and lamps
    gate = Frame(3000, 150, 0)
    for gy in (-330, 330):
        gate.put(CUBE, 0, gy, 260, (40, 40, 520), "timber", folder="Village/Gate")
    gate.put(CUBE, 0, 0, 500, (36, 760, 36), "timber", folder="Village/Gate")
    gate.put(CUBE, 0, 0, 440, (8, 260, 70), "planks", folder="Village/Gate", collide=False)
    gate.put(CUBE, 0, -250, 400, (6, 50, 140), "cloth_red", folder="Village/Gate", collide=False)
    gate.put(CUBE, 0, 250, 400, (6, 50, 140), "cloth_red", folder="Village/Gate", collide=False)
    ring = []
    for deg in range(20, 160, 10):
        ring.append((3350 * math.cos(math.radians(deg)), 3350 * math.sin(math.radians(deg))))
    fence_line(ring, folder="Village/Palisade", spacing=200)
    ring2 = [(3350 * math.cos(math.radians(d)), 3350 * math.sin(math.radians(d))) for d in range(200, 345, 10)]
    fence_line(ring2, folder="Village/Palisade", spacing=200)
    for ang in (30, 100, 170, 240, 310):
        lamp_post(plaza[0] + 1150 * math.cos(math.radians(ang)), plaza[1] + 1150 * math.sin(math.radians(ang)))
    lamp_post(2900, -260)
    lamp_post(2900, 560)

    # clutter
    for (x, y) in [(1250, 450), (1300, 560), (-700, 1200), (900, 1350), (-1500, -300), (1050, -1350)]:
        crate(x, y, yaw=rng.uniform(0, 90))
    crate(1250, 450, stack=1, size=70, yaw=20)
    for (x, y) in [(-800, 1250), (1180, 620), (-1300, -1650), (2300, -700)]:
        barrel(x, y)
    for (x, y, yaw) in [(800, 300, 90), (-200, -700, 0)]:
        f = Frame(x, y, yaw)
        f.put(CUBE, 0, 0, 45, (60, 220, 10), "planks", folder="Village/Benches")
        for ly in (-90, 90):
            f.put(CUBE, 0, ly, 22, (50, 12, 44), "planks", folder="Village/Benches")
    log("Village built")


# =====================================================================================================================
# Landmarks
# =====================================================================================================================

def build_landmarks():
    rng = random.Random(17)

    # Sentinel's Rise: ruined watchtower on the hill
    tx, ty = 8200, 5400
    tz = ground(tx, ty)
    fold = "Landmarks/SentinelsRise"
    part(CYL, (tx, ty, tz + 125), (680, 680, 250), "stone_old", folder=fold)
    part(CYL, (tx, ty, tz + 450), (600, 600, 400), "stone_light", folder=fold)
    part(CYL, (tx, ty, tz + 820), (560, 560, 340), "stone_light", folder=fold)
    for k in range(12):
        if k in (3, 4, 9):
            continue  # broken battlements
        ang = math.radians(k * 30)
        h = rng.uniform(70, 130)
        part(CUBE, (tx + 255 * math.cos(ang), ty + 255 * math.sin(ang), tz + 990 + h / 2), (70, 50, h), "stone_light", rot=(0, k * 30, 0), folder=fold)
    door_ang = math.radians(225)
    part(CUBE, (tx + 300 * math.cos(door_ang), ty + 300 * math.sin(door_ang), tz + 130), (30, 140, 220), "door", rot=(0, 225, 0), folder=fold)
    for k in range(5):
        ang = math.radians(20 + k * 70)
        part(CUBE, (tx + 285 * math.cos(ang), ty + 285 * math.sin(ang), tz + 600 + (k % 2) * 200), (20, 18, 80), "window", rot=(0, 20 + k * 70, 0), folder=fold, collide=False)
    for k in range(9):
        ang = rng.uniform(0, 2 * math.pi)
        d = rng.uniform(420, 700)
        rock(tx + d * math.cos(ang), ty + d * math.sin(ang), (rng.uniform(70, 150), rng.uniform(60, 120), rng.uniform(40, 90)), "stone_light", fold, rng=rng)
    part(CUBE, (tx - 600, ty - 300, tz + 90), (500, 60, 180), "stone_old", rot=(0, 35, 0), folder=fold)  # broken wall
    part(CUBE, (tx - 380, ty - 650, tz + 55), (320, 60, 110), "stone_old", rot=(0, 70, 0), folder=fold)
    part(CYL, (tx + 220, ty + 220, tz + 1150), (12, 12, 300), "timber", rot=(0, 0, 12), folder=fold)
    part(CUBE, (tx + 260, ty + 220, tz + 1230), (4, 110, 70), "cloth_blue", rot=(0, 10, 12), folder=fold, collide=False)

    # broken wagon at the woods' edge
    wx, wy = 11350, 520
    w = Frame(wx, wy, 25)
    fold = "Landmarks/BrokenWagon"
    w.put(CUBE, 0, 0, 70, (330, 160, 22), "planks", roll=-12, folder=fold)
    for ly in (-85, 85):
        w.put(CUBE, 0, ly, 110, (330, 10, 70), "planks", roll=-12, folder=fold)
    w.put(CYL, 110, 95, 55, (110, 110, 12), "planks", roll=90, folder=fold)
    w.put(CYL, -110, 95, 55, (110, 110, 12), "planks", roll=90, folder=fold)
    w.put(CYL, -110, -95, 40, (110, 110, 12), "planks", roll=78, folder=fold)
    w.put(CYL, 260, -260, 8, (110, 110, 12), "planks", folder=fold)  # wheel lying in the grass
    w.put(CUBE, 230, 0, 30, (220, 14, 14), "timber", yaw=25, folder=fold)
    for (lx, ly, s) in [(-260, -150, 70), (-330, 40, 60), (-150, -260, 55)]:
        w.put(CUBE, lx, ly, s / 2, (s, s, s), "planks", yaw=rng.uniform(0, 90), folder=fold)
    barrel(*w.at(-200, 180, 0)[:2], folder=fold)

    # standing stones (old ruins) in the meadow
    rx, ry = 6300, -5800
    fold = "Landmarks/StandingStones"
    for k in range(8):
        ang = math.radians(k * 45 + 10)
        px, py = rx + 650 * math.cos(ang), ry + 650 * math.sin(ang)
        if k == 5:
            part(CYL, (px, py, ground(px, py) + 50), (90, 90, 380), "stone_old", rot=(90, k * 45, 0), folder=fold)  # fallen
            continue
        h = rng.uniform(140, 430) if k % 3 else rng.uniform(380, 460)
        part(CYL, (px, py, ground(px, py) + h / 2 - 10), (95, 95, h), "stone_old", folder=fold)
    part(CHAMFER, (rx, ry, ground(rx, ry) + 40), (240, 160, 90), "stone_light", folder=fold)
    a1, a2 = (rx + 650 * math.cos(math.radians(10)), ry + 650 * math.sin(math.radians(10))), (rx + 650 * math.cos(math.radians(55)), ry + 650 * math.sin(math.radians(55)))
    part(CUBE, ((a1[0] + a2[0]) / 2, (a1[1] + a2[1]) / 2, ground(rx, ry) + 470), (60, 560, 60), "stone_old",
         rot=(0, math.degrees(math.atan2(a2[1] - a1[1], a2[0] - a1[0])) - 90, 0), folder=fold)

    # abandoned campsite in Greywood
    cx, cy = 14550, 2950
    c = Frame(cx, cy, -30)
    fold = "Landmarks/Campsite"
    for sign in (-1, 1):
        c.put(CUBE, 0, sign * 70, 85, (260, 10, 200), "cloth_green", roll=sign * 38, folder=fold)
    c.put(CYL, 0, 0, 160, (8, 8, 280), "timber", pitch=90, folder=fold, collide=False)
    for k in range(8):
        ang = math.radians(k * 45)
        c.put(CHAMFER, 320 + 70 * math.cos(ang), 70 * math.sin(ang), 10, (30, 26, 22), "rock", folder=fold)
    c.put(CYL, 320, 0, 3, (110, 110, 6), "ash", folder=fold, collide=False)
    c.put(CYL, 320, 0, 15, (10, 10, 110), "bark_dark", roll=80, yaw=30, folder=fold, collide=False)
    c.put(CYL, 320, 0, 15, (10, 10, 110), "bark_dark", roll=80, yaw=-40, folder=fold, collide=False)
    c.put(CYL, 460, -160, 20, (40, 40, 220), "bark", roll=90, yaw=60, folder=fold)
    c.put(CYL, 180, 200, 20, (40, 40, 200), "bark", roll=90, yaw=-20, folder=fold)
    c.put(CYL, -40, 150, 12, (60, 60, 180), "cloth_red", roll=90, folder=fold, collide=False)  # bedroll
    c.put(CUBE, -160, -120, 30, (60, 45, 60), "planks", folder=fold)

    # Howling Den: rock outcrop with a dark mouth and old bones
    dx, dy = 16500, -4800
    d = Frame(dx, dy, 40)
    fold = "Landmarks/HowlingDen"
    for (lx, ly, lz, sx, sy, sz, yaw, p) in [(-200, -380, 140, 420, 300, 300, 10, 8), (-200, 380, 140, 420, 300, 300, -12, -6),
                                              (-260, 0, 330, 520, 900, 200, 0, 4), (-520, 0, 160, 300, 760, 340, 0, 0),
                                              (60, -560, 60, 260, 200, 140, 30, 12), (40, 600, 70, 240, 220, 160, -20, 10)]:
        d.put(CHAMFER, lx, ly, lz, (sx, sy, sz), "rock_dark", yaw=yaw, pitch=p, folder=fold)
    d.put(CUBE, -330, 0, 120, (60, 420, 240), "void", folder=fold)
    for k in range(7):
        d.put(CYL, rng.uniform(0, 400), rng.uniform(-300, 300), 6, (8, 8, rng.uniform(40, 80)), "bone", pitch=90, yaw=rng.uniform(0, 180), folder=fold, collide=False)
    d.put(SPHERE, 160, 120, 12, (26, 22, 22), "bone", folder=fold, collide=False)

    # waterfall tumbling off the western ridge into a pool
    pool = (13650, -6850)
    pz = ground(*pool)
    fold = "Landmarks/Waterfall"
    part(CYL, (pool[0], pool[1], pz + 4), (1000, 1000, 10), "glow_water", folder=fold, collide=False, shadow=False)
    for k in range(10):
        ang = math.radians(k * 36)
        rock(pool[0] + 560 * math.cos(ang), pool[1] + 560 * math.sin(ang), (rng.uniform(120, 220), rng.uniform(90, 160), rng.uniform(70, 140)), "rock_moss", fold, rng=rng)
    # cascade: water strips following the ridge slope from the top down to the pool
    top_y, bottom_y = -8500, pool[1] - 380
    steps = 6
    for k in range(steps):
        y_top = top_y + (bottom_y - top_y) * k / steps
        y_low = top_y + (bottom_y - top_y) * (k + 1) / steps
        z_top, z_low = ground(pool[0], y_top), ground(pool[0], y_low)
        length = math.hypot(y_low - y_top, z_top - z_low)
        drop = math.degrees(math.atan2(z_top - z_low, y_low - y_top))
        # long axis is the cube's X, turned to +Y (downhill) and pitched down by the slope
        part(CUBE, (pool[0], (y_top + y_low) / 2, (z_top + z_low) / 2 + 75), (length + 80, 300, 40), "glow_water",
             rot=(-drop, 90, 0), folder=fold, collide=False, shadow=False)
        part(CUBE, (pool[0] + 150, (y_top + y_low) / 2, (z_top + z_low) / 2 + 20), (length + 50, 60, 40), "rock_moss",
             rot=(-drop, 90, 0), folder=fold, collide=False)
    for k in range(9):
        part(SPHERE, (pool[0] + rng.uniform(-150, 150), pool[1] - 380 + rng.uniform(-80, 80), pz + 20), (rng.uniform(70, 130),) * 3, "glow_foam", folder=fold, collide=False, shadow=False)

    # giant dead tree in Fanghollow
    gx, gy = 20650, 6150
    gz = ground(gx, gy)
    fold = "Landmarks/GiantDeadTree"
    part(CYL, (gx, gy, gz + 1000), (440, 440, 2100), "dead_bark", rot=(0, 0, 3), folder=fold)
    for k in range(5):
        ang = k * 72 + rng.uniform(-15, 15)
        part(CUBE, (gx + 260 * math.cos(math.radians(ang)), gy + 260 * math.sin(math.radians(ang)), gz + 60), (380, 110, 90), "dead_bark", rot=(-25, ang, 0), folder=fold)
    for k in range(7):
        ang = k * 51 + rng.uniform(-10, 10)
        height = rng.uniform(1300, 2000)
        length = rng.uniform(500, 900)
        lift = rng.uniform(25, 55)
        cx_ = gx + math.cos(math.radians(ang)) * (length / 2) * math.cos(math.radians(lift))
        cy_ = gy + math.sin(math.radians(ang)) * (length / 2) * math.cos(math.radians(lift))
        part(CYL, (cx_, cy_, gz + height + (length / 2) * math.sin(math.radians(lift))), (60, 60, length), "dead_bark",
             rot=(90 - lift, ang, 0), folder=fold, collide=False)

    # Rustvein Mine entrance in the north cliff
    mx, my = 21650, -1800
    mz = ground(21300, my)
    m = Frame(mx, my, 180, z=mz)  # front faces south (-x)
    fold = "Landmarks/RustveinMine"
    m.put(CUBE, -350, 0, 220, (700, 520, 440), "void", folder=fold)  # the dark interior, set into the cliff
    for ly in (-260, 260):
        m.put(CUBE, 10, ly, 210, (50, 50, 420), "planks", folder=fold)
    m.put(CUBE, 10, 0, 430, (60, 620, 50), "planks", folder=fold)
    m.put(CUBE, 30, 0, 300, (8, 520, 30), "planks", roll=24, folder=fold)  # boards across the opening
    m.put(CUBE, 34, 0, 170, (8, 520, 30), "planks", roll=-18, folder=fold)
    for k in range(14):
        lx = 40 + k * 90
        for ly in (-70, 70):
            m.put(CUBE, lx, ly, 6, (90, 8, 8), "rust", folder=fold, collide=False)
        m.put(CUBE, lx, 0, 3, (14, 200, 6), "planks", folder=fold, collide=False)
    cart = Frame(*m.at(520, 0, 0)[:2], 180 + 8, z=mz)
    cart.put(CUBE, 0, 0, 75, (180, 110, 80), "rust", roll=4, folder=fold)
    for lx in (-60, 60):
        for ly in (-60, 60):
            cart.put(CYL, lx, ly, 22, (40, 40, 10), "metal", roll=90, folder=fold, collide=False)
    cart.put(SPHERE, 0, 0, 120, (120, 80, 50), "rock_dark", folder=fold, collide=False)
    m.put(CUBE, 120, 380, 120, (14, 14, 240), "timber", folder=fold)
    m.put(CUBE, 120, 380, 230, (24, 24, 30), "glow_lantern", folder=fold, shadow=False)
    sign = Frame(*m.at(300, -420, 0)[:2], 180, z=mz)
    sign.put(CUBE, 0, 0, 90, (12, 12, 180), "timber", folder=fold)
    sign.put(CUBE, 0, 0, 160, (6, 120, 20), "planks", roll=35, folder=fold)
    sign.put(CUBE, 0, 0, 160, (6, 120, 20), "planks", roll=-35, folder=fold)
    for k in range(10):
        lx, ly = rng.uniform(80, 600), rng.choice([-1, 1]) * rng.uniform(400, 900)
        rock(*m.at(lx, ly, 0)[:2], (rng.uniform(90, 200), rng.uniform(80, 160), rng.uniform(60, 140)), "rock_dark", fold, rng=rng)
    light = ACTORS.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*m.at(120, 380, 240)))
    light.get_component_by_class(unreal.PointLightComponent).set_editor_property("intensity", 25.0)
    light.get_component_by_class(unreal.PointLightComponent).set_editor_property("light_color", unreal.Color(255, 170, 90, 255))
    light.get_component_by_class(unreal.PointLightComponent).set_editor_property("attenuation_radius", 800.0)
    light.set_folder_path(fold)
    log("Landmarks built")


# =====================================================================================================================
# Vegetation (instanced scatter)
# =====================================================================================================================

def sp(mesh, mat, offset=(0, 0, 0), scale=(1, 1, 1), rot=(0, 0, 0), pawns=False, camera=False):
    return make_struct(unreal.MMOScatterPart, mesh=mesh, material=MATS[mat], offset=unreal.Vector(*offset),
                  rotation=unreal.Rotator(rot[2], rot[0], rot[1]), scale=unreal.Vector(*scale), blocks_pawns=pawns, blocks_camera=camera)


def oak(leaf="leaf_oak", light="leaf_oak_light"):
    return [sp(CYL, "bark", (0, 0, 160), (0.45, 0.45, 3.2), pawns=True),
            sp(SPHERE, leaf, (0, 0, 430), (3.8, 3.8, 2.7), camera=True),
            sp(SPHERE, light, (90, 60, 570), (2.7, 2.7, 2.1), camera=True),
            sp(SPHERE, leaf, (-110, -70, 520), (2.5, 2.5, 1.9))]


def pine(leaf="leaf_pine", dark="leaf_pine_dark", bark="bark_dark", lift=0.0):
    # lift raises the canopy on a taller trunk (deep woods), keeping it above the camera
    return [sp(CYL, bark, (0, 0, 150 + lift / 2), (0.34, 0.34, 3.0 + lift / 100), pawns=True),
            sp(CONE, leaf, (0, 0, 440 + lift), (3.6, 3.6, 3.0), camera=True),
            sp(CONE, leaf, (0, 0, 640 + lift), (2.7, 2.7, 2.6)),
            sp(CONE, dark, (0, 0, 810 + lift), (1.7, 1.7, 2.0))]


def dead_tree():
    return [sp(CYL, "dead_bark", (0, 0, 260), (0.4, 0.4, 5.2), pawns=True),
            sp(CYL, "dead_bark", (70, 0, 400), (0.15, 0.15, 2.2), rot=(40, 0, 0)),
            sp(CYL, "dead_bark", (-60, 30, 330), (0.12, 0.12, 1.8), rot=(-45, 30, 0))]


def rock_parts(mat="rock", small="rock_moss"):
    return [sp(CHAMFER, mat, (0, 0, 20), (1.5, 1.1, 0.85), pawns=True),
            sp(CHAMFER, small, (90, 50, 0), (0.6, 0.5, 0.45), pawns=True)]


def scatter(label, center, extent, count, parts, seed, spacing=300, road=400, exclusions=(), scale=(0.85, 1.25), tilt=3, up=0.8,
            cull=0, sink=5, elliptical=True, folder="Vegetation"):
    actor = ACTORS.spawn_actor_from_class(unreal.MMOScatter, unreal.Vector(center[0], center[1], 0))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    for key, value in [("extent", V2(*extent)), ("count", count), ("seed", seed), ("min_spacing", spacing), ("road_clearance", road),
                       ("exclusions", [unreal.Vector(*e) for e in exclusions]), ("scale_range", V2(*scale)), ("max_tilt", tilt),
                       ("min_ground_up", up), ("cull_distance", cull), ("sink", sink), ("elliptical", elliptical), ("parts", parts)]:
        actor.set_editor_property(key, value)
    actor.rebuild()
    log("Scatter %-24s %d placed" % (label, actor.get_placed_count()))
    return actor


WOLF_SPOTS = []


def build_vegetation():
    keep_clear = [(0, 0, 3700), (8200, 5400, 1300), (6300, -5800, 1000), (14550, 2950, 900), (16500, -4800, 1100),
                  (13650, -6850, 900), (20650, 6150, 900), (21300, -1800, 1500), (11350, 520, 600)]
    wolves = [(x, y, 350) for (x, y) in WOLF_SPOTS] + [(20300, 5700, 1200)]  # open clearing for the Dire Wolf fight
    ex = keep_clear + wolves

    # meadow
    scatter("Meadow_Oaks", (7200, 0), (4300, 8000), 30, oak(), 11, spacing=1000, road=700, exclusions=ex)
    scatter("Meadow_Rocks", (7200, 0), (4300, 8000), 45, rock_parts(), 12, spacing=500, road=500, exclusions=keep_clear, tilt=15, sink=25, scale=(0.6, 1.6))
    scatter("Meadow_Grass", (7200, 0), (4600, 8200), 5200, [sp(CONE, "grass_tuft", (0, 0, 14), (0.13, 0.13, 0.36))], 13, spacing=55, road=260, exclusions=keep_clear[1:3], tilt=12, cull=5500, scale=(0.7, 1.4))
    scatter("Meadow_GrassDry", (7200, 0), (4600, 8200), 1600, [sp(CONE, "grass_dry", (0, 0, 14), (0.12, 0.12, 0.42))], 14, spacing=70, road=260, tilt=12, cull=5000, scale=(0.7, 1.3))
    for i, color in enumerate(["flower_red", "flower_yellow", "flower_white", "flower_purple"]):
        scatter("Meadow_Flowers_%s" % color.split("_")[1], (7200, 0), (4300, 7800), 450, [sp(SPHERE, color, (0, 0, 18), (0.11, 0.11, 0.09)),
                sp(CYL, "grass_tuft", (0, 0, 8), (0.02, 0.02, 0.16))], 20 + i, spacing=80, road=260, tilt=8, cull=4000)

    # village greenery
    scatter("Village_Trees", (0, 0), (3300, 3300), 9, oak("leaf_oak_light", "leaf_oak"), 31, spacing=900, road=600,
            exclusions=[(300, 0, 1700), (-1100, -900, 900), (-200, 1500, 700), (1700, 1650, 650), (-1600, 900, 700), (1500, -1700, 750),
                        (-400, -2100, 650), (1900, 300, 700), (-1900, -2700, 1000), (3000, 150, 500)])
    scatter("Village_Grass", (0, 0), (3400, 3400), 1200, [sp(CONE, "grass_tuft", (0, 0, 12), (0.12, 0.12, 0.3))], 32, spacing=60, road=300,
            exclusions=[(300, 0, 1500), (-1900, -2700, 900)], tilt=10, cull=4000)

    # Greywood
    scatter("Greywood_Pines", (15800, -1500), (4400, 7300), 520, pine(), 41, spacing=330, road=450, exclusions=ex, scale=(0.8, 1.55), up=0.72)
    scatter("Greywood_Oaks", (15800, -1500), (4400, 7300), 70, oak("leaf_oak", "leaf_pine"), 42, spacing=700, road=500, exclusions=ex, scale=(0.9, 1.3))
    scatter("Greywood_Bushes", (15800, -1500), (4400, 7300), 600, [sp(SPHERE, "bush", (0, 0, 20), (1.3, 1.3, 0.7))], 43, spacing=150, road=300, exclusions=keep_clear, tilt=6, cull=6000, scale=(0.6, 1.4))
    scatter("Greywood_Rocks", (15800, -1500), (4400, 7300), 70, rock_parts("rock_moss", "rock"), 44, spacing=500, road=450, exclusions=keep_clear, tilt=18, sink=25, scale=(0.6, 1.8))
    scatter("Greywood_Grass", (15800, -1500), (4400, 7300), 2200, [sp(CONE, "grass_dark", (0, 0, 14), (0.13, 0.13, 0.4))], 45, spacing=70, road=260, tilt=12, cull=4500)

    # Fanghollow
    scatter("Fanghollow_Pines", (20200, 5500), (2300, 3100), 150, pine("leaf_hollow", "leaf_pine_dark", "bark_dark", lift=220), 51, spacing=380, road=400, exclusions=ex, scale=(1.0, 1.7), up=0.7)
    scatter("Fanghollow_DeadTrees", (20200, 5500), (2300, 3100), 22, dead_tree(), 52, spacing=500, road=400, exclusions=ex, scale=(0.8, 1.4), tilt=8)
    scatter("Fanghollow_Rocks", (20200, 5500), (2300, 3100), 30, rock_parts("rock_dark", "rock_moss"), 53, spacing=500, road=400, exclusions=keep_clear, tilt=18, sink=25, scale=(0.8, 2.0))

    # mine approach
    scatter("Mine_Boulders", (20700, -1700), (1100, 2400), 28, rock_parts("rock_dark", "rock"), 61, spacing=420, road=450, exclusions=[(21300, -1800, 900)], tilt=20, sink=30, scale=(1.2, 2.6), up=0.6)

    # rocky faces on the north cliff and the rim slopes
    scatter("Cliff_Rocks", (22300, 0), (1000, 8200), 150, [sp(CHAMFER, "rock_dark", (0, 0, 0), (4.0, 3.0, 2.6), pawns=True),
            sp(CHAMFER, "rock", (180, 120, 120), (2.2, 1.8, 1.6), pawns=True)], 81, spacing=350, road=0, exclusions=[(21650, -1800, 500)],
            tilt=25, sink=80, scale=(0.8, 1.6), up=0.0, elliptical=False, folder="Vegetation/Rocks")
    for i, (label, center, extent) in enumerate([("Rim_Rocks_West", (9500, -8300), (14500, 700)), ("Rim_Rocks_East", (9500, 8300), (14500, 700)),
                                                ("Rim_Rocks_South", (-4400, 0), (600, 9000))]):
        scatter(label, center, extent, 90, rock_parts("rock_dark", "rock_moss"), 90 + i, spacing=500, road=0, exclusions=[(13650, -7300, 1300)],
                tilt=25, sink=60, scale=(2.0, 3.5), up=0.0, elliptical=False, folder="Vegetation/Rocks")

    # tree line on the rim so the map edge reads as hills, not a wall
    edge = [("Edge_South", (-4100, 0), (1000, 9000)), ("Edge_West", (9500, -8000), (14500, 1100)), ("Edge_East", (9500, 8000), (14500, 1100)),
            ("Edge_CliffTop", (23200, 0), (900, 9000))]
    for i, (label, center, extent) in enumerate(edge):
        scatter(label, center, extent, 380, pine(), 70 + i, spacing=380, road=0, exclusions=[(13650, -7300, 1300)], scale=(1.0, 1.8), up=0.35, elliptical=False, tilt=6)


# =====================================================================================================================
# Creatures, zones, navigation, start
# =====================================================================================================================

def wolf(x, y, yaw=None, cls=None, level=None, health=None, damage=None, xp=None, wander=None, label=None, folder="Creatures"):
    rng = random.Random(int(x * 7 + y))
    cls = cls or unreal.MMOGreyWolf
    z = ground(x, y) + (80 if cls == unreal.MMODireWolf else 60)
    actor = ACTORS.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(0, 0, rng.uniform(0, 360) if yaw is None else yaw))
    if level is not None:
        actor.set_editor_property("creature_level", level)
    if damage is not None:
        actor.set_editor_property("attack_damage", float(damage))
    if xp is not None:
        actor.set_editor_property("xp_reward", xp)
    if wander is not None:
        actor.set_editor_property("wander_radius", float(wander))
    if health is not None:
        actor.get_component_by_class(unreal.MMOHealthComponent).set_editor_property("max_health", float(health))
    actor.set_folder_path(folder)
    if label:
        actor.set_actor_label(label)
    return actor


MEADOW_WOLVES = [(4900, -2300), (6600, 2700), (8700, -1700), (10300, 2300), (5600, -4500), (9800, -4700), (8900, 4100)]
WOODS_PAIRS = [((12700, -2600), (13100, -2950)), ((15300, 3500), (15700, 3800)), ((17500, -1600), (17900, -1250)), ((16100, -350), (16450, 100))]
WOODS_SINGLES = [(14200, -300), (18600, -3600), (12300, 2600)]
DEN_WOLVES = [(16150, -4450), (16700, -4500), (16450, -5150)]
HOLLOW = {"dire": (20300, 5700), "escort": (19500, 4700)}
MINE_GUARD = (20400, -2700)


def creature_spots():
    spots = list(MEADOW_WOLVES) + list(WOODS_SINGLES) + DEN_WOLVES + [HOLLOW["dire"], HOLLOW["escort"], MINE_GUARD]
    for a, b in WOODS_PAIRS:
        spots += [a, b]
    WOLF_SPOTS.extend(spots)


def build_creatures():
    for i, (x, y) in enumerate(MEADOW_WOLVES):
        wolf(x, y, label="GreyWolf_Meadow_%d" % i, folder="Creatures/Meadow")
    for i, pair in enumerate(WOODS_PAIRS):
        for j, (x, y) in enumerate(pair):
            wolf(x, y, level=2, health=75, damage=7, xp=50, wander=350, label="GreyWolf_Greywood_Pair%d_%d" % (i, j), folder="Creatures/Greywood")
    for i, (x, y) in enumerate(WOODS_SINGLES):
        wolf(x, y, level=2, health=75, damage=7, xp=50, wander=650, label="GreyWolf_Greywood_%d" % i, folder="Creatures/Greywood")
    for i, (x, y) in enumerate(DEN_WOLVES):
        wolf(x, y, level=3, health=90, damage=8, xp=60, wander=250, label="GreyWolf_Den_%d" % i, folder="Creatures/HowlingDen")
    wolf(*HOLLOW["escort"], level=3, health=90, damage=8, xp=60, wander=500, label="GreyWolf_Fanghollow", folder="Creatures/Fanghollow")
    wolf(*HOLLOW["dire"], cls=unreal.MMODireWolf, label="DireWolf_Fanghollow", folder="Creatures/Fanghollow")
    wolf(*MINE_GUARD, level=2, health=75, damage=7, xp=50, wander=500, label="GreyWolf_Mine", folder="Creatures/Mine")
    log("Creatures placed: %d" % (len(WOLF_SPOTS)))


ZONES = [
    ("Thornwick", "Thornwick", "Village", (0, 0), (3600, 3600), 0, 1, "S_MMO_Amb_Village", 0.5),
    ("WhisperingMeadow", "Whispering Meadow", "Level 1-2", (7000, 0), (4100, 8200), 25, 0, "S_MMO_Amb_Meadow", 0.55),
    ("SentinelsRise", "Sentinel's Rise", "Old Watchtower", (8200, 5300), (1500, 1500), 40, 2, "S_MMO_Amb_Meadow", 0.4),
    ("Greywood", "Greywood", "Level 2-3", (15800, -1400), (4700, 7800), 40, 0, "S_MMO_Amb_Woods", 0.6),
    ("HowlingDen", "Howling Den", "Wolf Lair", (16500, -4800), (1200, 1200), 30, 3, "S_MMO_Amb_DeepWoods", 0.4),
    ("Fanghollow", "Fanghollow", "Level 4  -  Dangerous", (20300, 5600), (2300, 3200), 60, 2, "S_MMO_Amb_DeepWoods", 0.6),
    ("RustveinMine", "Rustvein Mine", "Abandoned", (21100, -1800), (1300, 1700), 60, 2, "S_MMO_Amb_Mine", 0.6),
]


def build_zones():
    for zone_id, name, subtitle, center, extent, xp, priority, ambience, volume in ZONES:
        z = ground(*center)
        actor = ACTORS.spawn_actor_from_class(unreal.MMODiscoveryZone, unreal.Vector(center[0], center[1], z + 500))
        actor.set_actor_label("Zone_" + zone_id)
        actor.set_folder_path("Zones")
        actor.set_editor_property("location_id", zone_id)
        actor.set_editor_property("location_name", name)
        actor.set_editor_property("subtitle", subtitle)
        actor.set_editor_property("discovery_xp", xp)
        actor.set_editor_property("priority", priority)
        actor.set_editor_property("ambient_loop", sound(ambience))
        actor.set_editor_property("ambient_volume", volume)
        actor.set_extent(unreal.Vector(extent[0], extent[1], 3500))
    log("Discovery zones: %d" % len(ZONES))


def build_navigation_and_start():
    nav = ACTORS.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(9500, 0, 1200))
    nav.set_actor_label("NavMeshBounds")
    nav.set_actor_scale3d(unreal.Vector(146.0, 92.0, 38.0))
    start = ACTORS.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-1500, -250, ground(-1500, -250) + 100), unreal.Rotator(0, 0, 5))
    start.set_actor_label("PlayerStart")
    log("NavMesh bounds and PlayerStart")


def main():
    build_materials()
    build_sounds()
    build_dire_loot()

    # rebuild in place: reuse the map asset if it exists, clearing everything in it
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        LEVELS.load_level(MAP_PATH)
        existing = [a for a in ACTORS.get_all_level_actors() if not isinstance(a, (unreal.WorldSettings, unreal.Brush))]
        ACTORS.destroy_actors(existing)
        log("Cleared %d actors from the existing map" % len(existing))
    elif not LEVELS.new_level(MAP_PATH, False):
        raise RuntimeError("Could not create " + MAP_PATH)

    build_terrain()
    build_atmosphere()
    creature_spots()
    build_village()
    build_landmarks()
    build_vegetation()
    build_creatures()
    build_zones()
    build_navigation_and_start()

    LEVELS.save_current_level()
    log("Saved %s with %d static mesh actors" % (MAP_PATH, COUNT[0]))


try:
    main()
finally:
    out = os.environ.get("MMO_SETUP_LOG")
    if out:
        with open(out, "w") as f:
            f.write("\n".join(LOG))
