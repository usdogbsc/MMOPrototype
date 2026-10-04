"""
Milestone 6 content setup (consumables, vendor goods, sounds). Run headless with the editor closed:

  UnrealEditor-Cmd.exe <path>/MMO.uproject -run=pythonscript -script="<path>/Tools/Python/setup_m6_economy.py"

Idempotent. It
  1. creates potions, food and vendor gear (with icons) in /Game/MMO/Items,
  2. synthesizes drink / eat sounds into /Game/MMO/Audio,
  3. re-runs the Milestone 2 loot table and Milestone 4 quest setup so potions appear in loot and rewards.
Vendors and the herbalist are placed by build_m3_zone.py (run it afterwards).
"""

import os
import random
import struct
import sys
import wave

import unreal

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import setup_m2_items as m2  # noqa: E402
import setup_m4_quests as m4  # noqa: E402

LOG = []


def log(message):
    LOG.append(str(message))
    unreal.log("[MMO setup] " + str(message))


R = unreal.MMOItemRarity
T = unreal.MMOItemType
S = unreal.MMOEquipmentSlot
poly, ell, line = m2.polygon, m2.ellipse, m2.line


def icon_shapes():
    return {
        "MinorHealingPotion": [
            poly([(52, 14), (76, 14), (76, 40), (100, 62), (104, 86), (94, 106), (64, 116), (34, 106), (24, 86), (28, 62), (52, 40)], (200, 210, 220)),
            poly([(32, 72), (96, 72), (100, 88), (92, 104), (64, 112), (36, 104), (28, 88)], (200, 30, 40)),
            ell(48, 84, 6, 10, (255, 140, 140)),
            poly([(50, 6), (78, 6), (78, 18), (50, 18)], (130, 90, 50)),
        ],
        "HeartyBread": [
            ell(64, 72, 50, 34, (176, 110, 50)),
            ell(64, 64, 46, 28, (214, 150, 74)),
            line(36, 52, 46, 74, 4, (150, 90, 40)),
            line(58, 46, 66, 72, 4, (150, 90, 40)),
            line(80, 48, 88, 72, 4, (150, 90, 40)),
        ],
        "ThornwickMeatPie": [
            ell(64, 80, 52, 26, (150, 90, 44)),
            ell(64, 68, 50, 28, (206, 146, 72)),
            ell(64, 66, 38, 18, (226, 172, 96)),
            line(50, 58, 58, 74, 3, (160, 100, 50)),
            line(70, 58, 78, 74, 3, (160, 100, 50)),
            ell(64, 50, 8, 6, (120, 30, 20)),
        ],
        "IronShortsword": [
            poly([(28, 106), (24, 102), (90, 36), (106, 22), (110, 26), (96, 42)], (168, 174, 184)),
            line(32, 98, 100, 30, 3, (220, 226, 236)),
            line(28, 76, 54, 102, 8, (90, 92, 98)),
            line(16, 116, 34, 98, 9, (88, 60, 38)),
            ell(14, 118, 6, 6, (120, 120, 126)),
        ],
        "PaddedLeatherVest": [
            poly([(34, 18), (52, 26), (76, 26), (94, 18), (112, 40), (100, 52), (98, 112), (30, 112), (28, 52), (16, 40)], (126, 86, 52)),
            poly([(52, 26), (64, 46), (76, 26)], (70, 46, 28)),
            line(64, 46, 64, 110, 3, (80, 52, 30)),
            line(40, 64, 88, 64, 3, (100, 68, 40)),
            line(40, 86, 88, 86, 3, (100, 68, 40)),
        ],
        "SturdyLeatherLeggings": [
            poly([(34, 14), (94, 14), (98, 60), (90, 116), (70, 116), (64, 52), (58, 116), (38, 116), (30, 60)], (110, 74, 44)),
            poly([(34, 14), (94, 14), (94, 26), (34, 26)], (80, 52, 30)),
            line(46, 60, 44, 108, 3, (86, 58, 34)),
            line(82, 60, 84, 108, 3, (86, 58, 34)),
        ],
    }


ITEMS = [
    dict(id="MinorHealingPotion", name="Minor Healing Potion", rarity=R.COMMON, type=T.CONSUMABLE, category="Potion",
         desc="A small vial of red tonic brewed by Mirelle Thistledown.", stack=10, sell=6, buy=25,
         heal=60, cooldown=60, group="Potion", combat=True),
    dict(id="HeartyBread", name="Hearty Bread", rarity=R.COMMON, type=T.CONSUMABLE, category="Food",
         desc="Fresh from the Thornfire Inn's oven. Still warm.", stack=20, sell=2, buy=10,
         hot=80, duration=18, cooldown=2, group="Food", combat=False),
    dict(id="ThornwickMeatPie", name="Thornwick Meat Pie", rarity=R.COMMON, type=T.CONSUMABLE, category="Food",
         desc="Brenna's famous pie. Nobody asks what is in it.", stack=20, sell=10, buy=40,
         hot=170, duration=20, cooldown=2, group="Food", combat=False),
    dict(id="IronShortsword", name="Iron Shortsword", rarity=R.COMMON, type=T.WEAPON, category="One-Handed Sword",
         desc="Plain, honest ironwork from Doran's forge.", stack=1, sell=70, buy=280, slot=S.MAIN_HAND, damage=(12, 17)),
    dict(id="PaddedLeatherVest", name="Padded Leather Vest", rarity=R.COMMON, type=T.ARMOR, category="Leather Chest",
         desc="Thick, quilted leather. Smells faintly of the tannery.", stack=1, sell=60, buy=240, slot=S.CHEST, armor=10),
    dict(id="SturdyLeatherLeggings", name="Sturdy Leather Leggings", rarity=R.COMMON, type=T.ARMOR, category="Leather Legs",
         desc="Reinforced at the knee for long days on the road.", stack=1, sell=50, buy=200, slot=S.LEGS, armor=7),
]


def setup_items():
    icons = icon_shapes()
    assets = {}
    for spec in ITEMS:
        item = m2.create_or_load("DA_Item_" + spec["id"], "/Game/MMO/Items", unreal.MMOItemDefinition)
        item.set_editor_property("item_id", spec["id"])
        item.set_editor_property("display_name", spec["name"])
        item.set_editor_property("description", spec["desc"])
        item.set_editor_property("flavor_text", spec.get("flavor", ""))
        item.set_editor_property("item_type", spec["type"])
        item.set_editor_property("rarity", spec["rarity"])
        item.set_editor_property("category_text", spec["category"])
        item.set_editor_property("max_stack_size", spec["stack"])
        item.set_editor_property("sell_value", spec["sell"])
        item.set_editor_property("buy_price", spec.get("buy", 0))
        item.set_editor_property("equipment_slot", spec.get("slot", S.NONE))
        damage = spec.get("damage", (0, 0))
        item.set_editor_property("weapon_damage_min", float(damage[0]))
        item.set_editor_property("weapon_damage_max", float(damage[1]))
        stats = unreal.MMOStatModifiers()
        stats.set_editor_property("armor", float(spec.get("armor", 0)))
        item.set_editor_property("stats", stats)
        item.set_editor_property("heal_amount", float(spec.get("heal", 0)))
        item.set_editor_property("heal_over_time", float(spec.get("hot", 0)))
        item.set_editor_property("effect_duration", float(spec.get("duration", 0)))
        item.set_editor_property("cooldown_group", spec.get("group", ""))
        item.set_editor_property("cooldown", float(spec.get("cooldown", 0)))
        item.set_editor_property("usable_in_combat", spec.get("combat", True))
        item.set_editor_property("icon", m2.import_icon(spec["id"], icons[spec["id"]]))
        item.set_editor_property("equipped_mesh", None)
        unreal.EditorAssetLibrary.save_loaded_asset(item, False)
        assets[spec["id"]] = item
        log("Item %-24s %s" % (spec["id"], item.get_path_name()))
    return assets


def make_sounds():
    rng = random.Random(606)
    s = {}
    # drink: cork pop, then three soft gulps
    drink = m2.mix(m2.tone(0.05, 900, 500, (1.0,), decay=12), m2.noise(0.04, rng, 10, 0.5), 0.0, 0.6)
    for k in range(3):
        m2.mix(drink, m2.tone(0.12, 180 + k * 15, 120, (1.0, 0.4), decay=9), 0.18 + k * 0.17, 0.7)
        m2.mix(drink, m2.noise(0.08, rng, 8, 0.08), 0.2 + k * 0.17, 0.25)
    s["S_MMO_Drink"] = m2.normalize(drink, 0.5)
    # eat: a few crunchy bites
    eat = []
    for k in range(3):
        m2.mix(eat, m2.noise(0.09, rng, 14, 0.7), 0.05 + k * 0.22, 0.8)
        m2.mix(eat, m2.tone(0.06, 140, 90, (1.0,), decay=10), 0.05 + k * 0.22, 0.4)
    s["S_MMO_Eat"] = m2.normalize(eat, 0.45)
    return s


def setup_audio():
    tasks = []
    for name, samples in make_sounds().items():
        path = os.path.join(m2.WORK_DIR, name + ".wav")
        with wave.open(path, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(m2.RATE)
            w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32767)) for v in samples))
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = "/Game/MMO/Audio"
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        tasks.append(task)
    m2.ASSET_TOOLS.import_asset_tasks(tasks)
    for task in tasks:
        log("Sound %s" % list(task.imported_object_paths))


def main():
    setup_items()
    setup_audio()
    # grey wolf loot (now with the occasional potion) and quest rewards that reference the new items
    m2.setup_loot_table({})
    m4.main()
    log("Loot table and quests refreshed")


def write_log():
    out = os.environ.get("MMO_SETUP_LOG")
    if out:
        with open(out, "w") as f:
            f.write("\n".join(LOG + m2.LOG + m4.LOG))


try:
    main()
    write_log()
except Exception as error:
    log("ERROR: %s" % error)
    write_log()
    raise
