"""
Milestone 2 content setup (items, loot table, icons, UI sounds). Run headless with the editor closed:

  UnrealEditor-Cmd.exe <path>/MMO.uproject -run=pythonscript -script="<path>/Tools/Python/setup_m2_items.py"

Idempotent: safe to re-run; existing assets are updated in place. It
  1. draws simple original item icons (PNG) and imports them to /Game/MMO/Items/Icons,
  2. creates/updates the item definition data assets in /Game/MMO/Items (DA_Item_<ItemId>),
  3. creates/updates the Grey Wolf loot table /Game/MMO/Loot/DA_Loot_GreyWolf,
  4. synthesizes placeholder item/UI sounds into /Game/MMO/Audio.
"""

import math
import os
import random
import struct
import tempfile
import wave
import zlib

import unreal

LOG = []
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
WORK_DIR = os.path.join(tempfile.gettempdir(), "mmo_m2_content")
os.makedirs(WORK_DIR, exist_ok=True)


def log(message):
    LOG.append(str(message))
    unreal.log("[MMO setup] " + str(message))


# ---------------------------------------------------------------------------------------------------------------------
# 1. Icons: tiny vector rasterizer (supersampled) -> PNG
# ---------------------------------------------------------------------------------------------------------------------

SIZE = 128
SS = 3  # supersampling per axis


def point_in_polygon(x, y, poly):
    inside = False
    j = len(poly) - 1
    for i in range(len(poly)):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi + 1e-9) + xi:
            inside = not inside
        j = i
    return inside


def dist_to_segment(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy + 1e-9)))
    cx, cy = ax + t * dx, ay + t * dy
    return math.hypot(px - cx, py - cy)


class Shape:
    def __init__(self, test, color):
        self.test = test
        self.color = color


def polygon(points, color):
    return Shape(lambda x, y: point_in_polygon(x, y, points), color)


def ellipse(cx, cy, rx, ry, color, angle=0.0):
    ca, sa = math.cos(angle), math.sin(angle)

    def test(x, y):
        dx, dy = x - cx, y - cy
        u, v = dx * ca + dy * sa, -dx * sa + dy * ca
        return (u / rx) ** 2 + (v / ry) ** 2 <= 1.0
    return Shape(test, color)


def line(ax, ay, bx, by, width, color):
    return Shape(lambda x, y: dist_to_segment(x, y, ax, ay, bx, by) <= width * 0.5, color)


def render(shapes, outline=(20, 16, 12)):
    """Draws shapes back-to-front with a 2px dark outline around the silhouette."""
    n = SIZE * SS
    cover = [[None] * n for _ in range(n)]
    for sy in range(n):
        y = (sy + 0.5) / SS
        for sx in range(n):
            x = (sx + 0.5) / SS
            for shape in shapes:
                if shape.test(x, y):
                    cover[sy][sx] = shape.color
    # dilated silhouette for the outline
    filled = [[cover[y][x] is not None for x in range(n)] for y in range(n)]
    r = 2 * SS
    edge = [[False] * n for _ in range(n)]
    for y in range(n):
        for x in range(n):
            if filled[y][x]:
                continue
            for dy in range(-r, r + 1, SS):
                yy = y + dy
                if 0 <= yy < n:
                    for dx in range(-r, r + 1, SS):
                        xx = x + dx
                        if 0 <= xx < n and filled[yy][xx]:
                            edge[y][x] = True
                            break
                if edge[y][x]:
                    break
    pixels = bytearray()
    for py in range(SIZE):
        pixels.append(0)  # PNG filter type
        for px in range(SIZE):
            rs = gs = bs = a = 0
            for oy in range(SS):
                for ox in range(SS):
                    c = cover[py * SS + oy][px * SS + ox]
                    if c is not None:
                        rs += c[0]; gs += c[1]; bs += c[2]; a += 255
                    elif edge[py * SS + oy][px * SS + ox]:
                        rs += outline[0]; gs += outline[1]; bs += outline[2]; a += 230
            k = SS * SS
            alpha = a // k
            if alpha > 0:
                count = max(1, round(a / 255))
                pixels += bytes((min(255, rs // count), min(255, gs // count), min(255, bs // count), alpha))
            else:
                pixels += bytes((0, 0, 0, 0))
    return bytes(pixels)


def write_png(path, raw):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def icon_shapes():
    grey = (128, 126, 122)
    return {
        "WolfPelt": [
            polygon([(24, 40), (40, 26), (64, 22), (88, 26), (104, 40), (112, 30), (116, 46), (104, 58), (106, 84), (116, 98), (102, 100), (90, 108), (64, 112), (38, 108), (26, 100), (12, 98), (22, 84), (24, 58), (12, 46), (16, 30)], (118, 112, 104)),
            polygon([(44, 44), (64, 38), (84, 44), (88, 70), (84, 96), (64, 102), (44, 96), (40, 70)], (150, 144, 134)),
            line(64, 40, 64, 100, 6, (96, 90, 84)),
        ],
        "RawWolfMeat": [
            ellipse(62, 66, 44, 34, (238, 228, 214), 0.35),
            ellipse(62, 66, 38, 28, (178, 44, 52), 0.35),
            ellipse(56, 60, 16, 10, (206, 84, 92), 0.35),
            line(92, 84, 112, 100, 12, (236, 232, 220)),
            ellipse(112, 96, 8, 8, (236, 232, 220)),
            ellipse(108, 106, 8, 8, (236, 232, 220)),
        ],
        "WolfFang": [
            polygon([(42, 20), (86, 20), (84, 40), (76, 66), (66, 92), (56, 114), (54, 92), (50, 66), (44, 40)], (238, 230, 208)),
            polygon([(42, 20), (86, 20), (84, 34), (44, 34)], (198, 168, 128)),
            line(60, 42, 58, 96, 4, (255, 252, 240)),
        ],
        "WornLeatherBoots": [
            polygon([(36, 14), (72, 14), (74, 78), (110, 86), (114, 104), (108, 112), (34, 112), (32, 70)], (124, 82, 48)),
            polygon([(30, 104), (114, 104), (112, 116), (32, 116)], (66, 42, 26)),
            polygon([(34, 14), (74, 14), (74, 26), (34, 26)], (150, 104, 64)),
            line(46, 40, 62, 40, 4, (90, 58, 34)),
            line(46, 52, 62, 52, 4, (90, 58, 34)),
        ],
        "Greyfang": [
            polygon([(20, 112), (16, 108), (90, 34), (110, 14), (114, 18), (94, 38)], (178, 196, 222)),
            line(28, 104, 100, 30, 3, (236, 244, 255)),
            line(30, 74, 58, 102, 9, (70, 78, 96)),
            line(14, 116, 34, 96, 9, (54, 40, 32)),
            ellipse(12, 118, 7, 7, (234, 228, 206)),
            ellipse(44, 88, 5, 5, (120, 200, 255)),
        ],
        "TrainingSword": [
            polygon([(22, 110), (18, 106), (88, 36), (104, 22), (108, 26), (94, 40)], grey),
            line(32, 76, 58, 102, 8, (120, 86, 52)),
            line(16, 114, 34, 96, 9, (98, 70, 44)),
            ellipse(14, 116, 6, 6, (110, 80, 50)),
        ],
    }


def import_icon(item_id, shapes):
    path = os.path.join(WORK_DIR, "T_Icon_%s.png" % item_id)
    write_png(path, render(shapes))
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = "/Game/MMO/Items/Icons"
    task.destination_name = "T_Icon_%s" % item_id
    task.automated = True
    task.replace_existing = True
    task.save = False
    ASSET_TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset("/Game/MMO/Items/Icons/T_Icon_%s" % item_id)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    return texture


# ---------------------------------------------------------------------------------------------------------------------
# 2. Item definitions
# ---------------------------------------------------------------------------------------------------------------------

R = unreal.MMOItemRarity
T = unreal.MMOItemType
S = unreal.MMOEquipmentSlot

ITEMS = [
    dict(id="WolfPelt", name="Wolf Pelt", rarity=R.COMMON, type=T.MATERIAL, category="Crafting Material",
         desc="A coarse pelt taken from a Grey Wolf.", stack=20, sell=3),
    dict(id="RawWolfMeat", name="Raw Wolf Meat", rarity=R.COMMON, type=T.MATERIAL, category="Cooking Material",
         desc="Fresh meat from a Grey Wolf.", stack=20, sell=2),
    dict(id="WolfFang", name="Wolf Fang", rarity=R.COMMON, type=T.MATERIAL, category="Crafting Material",
         desc="A sharp fang useful for future crafting.", stack=20, sell=5),
    dict(id="WornLeatherBoots", name="Worn Leather Boots", rarity=R.UNCOMMON, type=T.ARMOR, category="Leather Boots",
         desc="Scuffed boots that have already walked a few hard roads before yours.", stack=1, sell=45,
         slot=S.FEET, armor=8),
    dict(id="Greyfang", name="Greyfang", rarity=R.RARE, type=T.WEAPON, category="One-Handed Sword",
         desc="A keen, grey-edged blade with a wolf's fang set into the hilt.",
         flavor="\"The old hunter's pack still circles the hills. Carry this, and they will know you have met before.\"",
         stack=1, sell=250, slot=S.MAIN_HAND, damage=(17, 22),
         mesh_color=unreal.LinearColor(0.55, 0.62, 0.75, 1.0)),
    dict(id="TrainingSword", name="Training Sword", rarity=R.COMMON, type=T.WEAPON, category="One-Handed Sword",
         desc="A dull practice blade handed to every new adventurer. It does the job, mostly.", stack=1, sell=1,
         slot=S.MAIN_HAND, damage=(10, 14), mesh_color=unreal.LinearColor(0.22, 0.2, 0.18, 1.0)),
]


def create_or_load(name, folder, cls):
    path = "%s/%s" % (folder, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return ASSET_TOOLS.create_asset(name, folder, cls, factory)


def setup_items():
    icons = icon_shapes()
    assets = {}
    for spec in ITEMS:
        item = create_or_load("DA_Item_" + spec["id"], "/Game/MMO/Items", unreal.MMOItemDefinition)
        item.set_editor_property("item_id", spec["id"])
        item.set_editor_property("display_name", spec["name"])
        item.set_editor_property("description", spec["desc"])
        item.set_editor_property("flavor_text", spec.get("flavor", ""))
        item.set_editor_property("item_type", spec["type"])
        item.set_editor_property("rarity", spec["rarity"])
        item.set_editor_property("category_text", spec["category"])
        item.set_editor_property("max_stack_size", spec["stack"])
        item.set_editor_property("sell_value", spec["sell"])
        item.set_editor_property("equipment_slot", spec.get("slot", S.NONE))
        damage = spec.get("damage", (0, 0))
        item.set_editor_property("weapon_damage_min", float(damage[0]))
        item.set_editor_property("weapon_damage_max", float(damage[1]))
        stats = unreal.MMOStatModifiers()
        stats.set_editor_property("armor", float(spec.get("armor", 0)))
        item.set_editor_property("stats", stats)
        item.set_editor_property("icon", import_icon(spec["id"], icons[spec["id"]]))

        # no weapon art yet: leave the equipped mesh empty (a box placeholder looked like a stray rectangle).
        # Assign a real sword mesh here (or in the asset) and it will attach to the weapon_r bone automatically.
        item.set_editor_property("equipped_mesh", None)

        unreal.EditorAssetLibrary.save_loaded_asset(item, False)
        assets[spec["id"]] = item
        log("Item %-18s %s" % (spec["id"], item.get_path_name()))
    return assets


# ---------------------------------------------------------------------------------------------------------------------
# 3. Grey Wolf loot table
# ---------------------------------------------------------------------------------------------------------------------

LOOT = [
    ("WolfPelt", 0.80, 1, 1),
    ("RawWolfMeat", 0.55, 1, 2),
    ("WolfFang", 0.30, 1, 2),
    ("WornLeatherBoots", 0.06, 1, 1),
    ("Greyfang", 0.015, 1, 1),
    ("MinorHealingPotion", 0.04, 1, 1),
]


def setup_loot_table(items):
    table = create_or_load("DA_Loot_GreyWolf", "/Game/MMO/Loot", unreal.MMOLootTable)
    entries = []
    for item_id, chance, qmin, qmax in LOOT:
        entry = unreal.MMOLootEntry()
        item = items.get(item_id) or unreal.load_asset("/Game/MMO/Items/DA_Item_" + item_id)
        if not item:
            continue
        entry.set_editor_property("item", item)
        entry.set_editor_property("drop_chance", chance)
        entry.set_editor_property("min_quantity", qmin)
        entry.set_editor_property("max_quantity", qmax)
        entries.append(entry)
    table.set_editor_property("entries", entries)
    table.set_editor_property("currency_chance", 0.7)
    table.set_editor_property("min_currency", 3)
    table.set_editor_property("max_currency", 12)
    unreal.EditorAssetLibrary.save_loaded_asset(table, False)
    log("Loot table %s with %d entries" % (table.get_path_name(), len(entries)))


# ---------------------------------------------------------------------------------------------------------------------
# 4. Placeholder sounds (original, synthesized here)
# ---------------------------------------------------------------------------------------------------------------------

RATE = 44100


def tone(seconds, f0, f1=None, harmonics=(1.0,), decay=4.0, attack=0.004):
    f1 = f1 or f0
    n, out, phase = int(seconds * RATE), [], 0.0
    for i in range(n):
        t = i / n
        f = f0 * (f1 / f0) ** t
        phase += 2 * math.pi * f / RATE
        v = sum(a * math.sin(phase * (k + 1)) for k, a in enumerate(harmonics))
        out.append(v * min(1.0, (i / RATE) / attack) * math.exp(-decay * t))
    return out


def noise(seconds, rng, decay=6.0, lowpass=0.3):
    n, out, y = int(seconds * RATE), [], 0.0
    for i in range(n):
        y += lowpass * (rng.uniform(-1, 1) - y)
        out.append(y * math.exp(-decay * i / n))
    return out


def mix(base, layer, offset=0.0, gain=1.0):
    start = int(offset * RATE)
    if len(base) < start + len(layer):
        base.extend([0.0] * (start + len(layer) - len(base)))
    for i, v in enumerate(layer):
        base[start + i] += v * gain
    return base


def normalize(samples, peak):
    m = max(1e-6, max(abs(v) for v in samples))
    return [v / m * peak for v in samples]


def make_sounds():
    rng = random.Random(4242)
    s = {}
    # soft "pop" of an item going into the bag
    s["S_MMO_LootPickup"] = normalize(mix(tone(0.12, 620, 900, (1.0, 0.3), decay=7), noise(0.06, rng, 9, 0.2), 0.0, 0.5), 0.45)
    # rare: rising sparkle chime
    chime = []
    for k, f in enumerate([659.25, 880.0, 1174.66, 1567.98]):
        mix(chime, tone(0.9, f, f, (1.0, 0.35, 0.12), decay=4.5), 0.07 * k, 0.45)
    sparkle = []
    for k in range(10):
        mix(sparkle, tone(0.08, rng.uniform(2500, 4200), None, (1.0,), decay=10), 0.05 + 0.06 * k, 0.18)
    s["S_MMO_RareLoot"] = normalize(mix(chime, sparkle), 0.7)
    # coins: a few short metallic pings
    coins = []
    for k in range(5):
        mix(coins, tone(0.12, rng.uniform(2600, 3600), None, (1.0, 0.5, 0.25), decay=12), 0.035 * k + rng.uniform(0, 0.02), 0.5)
    s["S_MMO_Coins"] = normalize(coins, 0.5)
    # equip: leather/metal "shing"
    shing = mix(noise(0.18, rng, 7, 0.6), tone(0.35, 1900, 2100, (1.0, 0.4), decay=6), 0.02, 0.35)
    s["S_MMO_Equip"] = normalize(mix(shing, tone(0.12, 160, 90, (1.0,), decay=8), 0.0, 0.5), 0.55)
    # error: low double blip
    s["S_MMO_Error"] = normalize(mix(tone(0.08, 220, 200, (1.0, 0.3), decay=5)[:], tone(0.08, 180, 160, (1.0, 0.3), decay=5), 0.1), 0.4)
    # window open/close: soft swoosh up / down
    def swoosh(rising):
        sw = noise(0.2, rng, 3, 0.15)
        n = len(sw)
        return [v * (math.sin(math.pi * i / n) ** 2) for i, v in enumerate(sw)]
    s["S_MMO_WindowOpen"] = normalize(mix(swoosh(True), tone(0.12, 500, 760, (1.0,), decay=6), 0.04, 0.25), 0.35)
    s["S_MMO_WindowClose"] = normalize(mix(swoosh(False), tone(0.12, 700, 460, (1.0,), decay=6), 0.02, 0.25), 0.3)
    return s


def setup_audio():
    tasks = []
    for name, samples in make_sounds().items():
        path = os.path.join(WORK_DIR, name + ".wav")
        with wave.open(path, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32767)) for v in samples))
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = "/Game/MMO/Audio"
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        tasks.append(task)
    ASSET_TOOLS.import_asset_tasks(tasks)
    for task in tasks:
        log("Sound %s" % list(task.imported_object_paths))


def main():
    items = setup_items()
    setup_loot_table(items)
    setup_audio()
    out = os.environ.get("MMO_SETUP_LOG")
    if out:
        with open(out, "w") as f:
            f.write("\n".join(LOG))


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        log("ERROR: %s" % error)
        out = os.environ.get("MMO_SETUP_LOG")
        if out:
            with open(out, "w") as f:
                f.write("\n".join(LOG))
        raise
