"""
Milestone 9 content setup (Rustvein Mine). Run headless with the editor closed:

  UnrealEditor-Cmd.exe <path>/MMO.uproject -run=pythonscript -script="<path>/Tools/Python/setup_m9_dungeon.py"

Idempotent. It
  1. creates the beetle material and the boss rewards (with icons) in /Game/MMO/Items,
  2. creates the Rustback and Rust Queen loot tables in /Game/MMO/Loot,
  3. creates Old Pell's two mine quests,
  4. synthesizes beetle and eruption sounds into /Game/MMO/Audio.
The mine interior, its creatures and portals are built by build_m3_zone.py (run it afterwards).
"""

import math
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
R = unreal.MMOItemRarity
T = unreal.MMOItemType
S = unreal.MMOEquipmentSlot
poly, ell, line = m2.polygon, m2.ellipse, m2.line


def log(message):
    LOG.append(str(message))
    unreal.log("[MMO setup] " + str(message))


def icon_shapes():
    rust = (170, 70, 24)
    dark = (60, 30, 18)
    return {
        "RustbackChitin": [
            poly([(20, 70), (40, 34), (88, 26), (112, 56), (100, 98), (54, 110), (24, 96)], rust),
            line(40, 50, 96, 88, 5, dark),
            line(30, 80, 70, 40, 4, dark),
            ell(80, 46, 8, 5, (230, 140, 70)),
        ],
        "MandibleCleaver": [
            poly([(22, 110), (18, 106), (80, 30), (104, 12), (112, 20), (94, 44)], (196, 120, 70)),
            poly([(80, 30), (104, 12), (116, 6), (110, 30), (94, 44)], (240, 200, 160)),
            line(26, 80, 52, 106, 9, dark),
            line(12, 118, 30, 100, 9, (40, 24, 16)),
            ell(46, 92, 6, 6, (255, 80, 20)),
        ],
        "ChitinplateGreaves": [
            poly([(36, 14), (72, 14), (74, 78), (110, 86), (114, 104), (108, 112), (34, 112), (32, 70)], rust),
            poly([(30, 104), (114, 104), (112, 116), (32, 116)], dark),
            poly([(36, 28), (72, 28), (72, 44), (36, 44)], dark),
            poly([(36, 56), (72, 56), (72, 70), (36, 70)], dark),
        ],
        "QueensCarapace": [
            poly([(34, 18), (52, 26), (76, 26), (94, 18), (112, 40), (100, 52), (98, 112), (30, 112), (28, 52), (16, 40)], rust),
            poly([(46, 40), (82, 40), (90, 76), (64, 104), (38, 76)], (210, 110, 50)),
            line(64, 40, 64, 104, 4, dark),
            ell(64, 60, 8, 8, (255, 90, 30)),
        ],
        "HeartOfTheRustvein": [
            ell(64, 70, 38, 38, (120, 60, 30)),
            ell(64, 70, 26, 26, (40, 26, 20)),
            ell(64, 32, 16, 13, (255, 70, 20)),
            ell(60, 28, 5, 4, (255, 200, 150)),
        ],
    }


ITEMS = [
    dict(id="RustbackChitin", name="Rustback Chitin", type=T.MATERIAL, category="Crafting Material",
         desc="A plate of rust-colored shell. Harder than it looks.", stack=20, sell=5),
    dict(id="MandibleCleaver", name="Mandible Cleaver", rarity=R.RARE, type=T.WEAPON, category="One-Handed Sword",
         desc="A blade ground from one of Grindmaw's mandibles.", flavor="\"It still bites.\"",
         stack=1, sell=320, slot=S.MAIN_HAND, damage=(19, 26), attack=2),
    dict(id="ChitinplateGreaves", name="Chitinplate Greaves", rarity=R.RARE, type=T.ARMOR, category="Heavy Boots",
         desc="Boots plated with overlapping queen-chitin.", stack=1, sell=240, slot=S.FEET, armor=14, health=10),
    dict(id="QueensCarapace", name="Queen's Carapace", rarity=R.RARE, type=T.ARMOR, category="Heavy Chest",
         desc="The Rust Queen's back plate, strapped into a breastplate.", stack=1, sell=300, slot=S.CHEST, armor=18, health=15),
    dict(id="HeartOfTheRustvein", name="Heart of the Rustvein", rarity=R.RARE, type=T.ARMOR, category="Ring",
         desc="A copper ring set with a glowing ember-stone from the mine's depths.",
         flavor="\"Pell swears it was his grandfather's. Pell swears a lot of things.\"",
         stack=1, sell=200, slot=S.RING, armor=3, health=20, attack=3),
]

LOOT = {
    "DA_Loot_Rustback": dict(entries=[("RustbackChitin", 0.7, 1, 2), ("CopperOre", 0.25, 1, 2), ("MinorHealingPotion", 0.06, 1, 1)],
                             currency=(0.6, 8, 22)),
    "DA_Loot_RustQueen": dict(entries=[("MandibleCleaver", 0.5, 1, 1), ("ChitinplateGreaves", 0.5, 1, 1), ("QueensCarapace", 0.5, 1, 1),
                                       ("RustbackChitin", 1.0, 3, 5), ("HealingPotion", 1.0, 2, 2)],
                              currency=(1.0, 150, 250)),
}

QUESTS = [
    dict(id="IntoTheRustvein", title="Into the Rustvein", level=4, giver="Pell", turnin="Pell", prereq="TheHowlingDen",
         desc="So you're the one who broke the wolf den. Then you're the one I've been waiting for.\n\n"
              "Those wolves didn't flee the Greywood for nothing. Something woke up in the Rustvein: beetles with backs like old iron, "
              "boiling up out of the deep tunnels. The boards across the entrance won't hold them forever. "
              "Go in, see how bad it is, and crack a few shells while you're there.",
         summary="Enter the Rustvein Mine, east past the Greywood, and slay 6 Rustback Skitterers.",
         progress="Still in one piece? Good. Mind the deep tunnels.",
         complete="Shells like iron and a nest down the main shaft... aye, it's what I feared. There's a queen down there.",
         objectives=[(m4.O.DISCOVER, "RustveinDepths", 1, "Enter the Rustvein Mine"), (m4.O.KILL, "Rustback", 6, "Rustback Skitterers slain")],
         xp=300, coin=80),
    dict(id="TheRustQueen", title="The Rust Queen", level=5, giver="Pell", turnin="Pell", prereq="IntoTheRustvein",
         desc="Forty years ago we broke into a cavern at the bottom of the Rustvein and heard something breathing. We sealed it and never spoke of it.\n\n"
              "She's awake now. Kill the queen and the brood will scatter. Watch the ground when she rears up; the old miners said the "
              "rock itself would burst into flame. Step out of the glow, and you'll live.",
         summary="Slay Grindmaw the Rust Queen at the bottom of the Rustvein Mine.",
         progress="She's at the bottom of the main shaft. Bring potions. Lots of potions.",
         complete="You did it. You actually did it. Here, take this. I've carried it since the day we sealed that cavern; it belongs with the one who finished the job.",
         objectives=[(m4.O.KILL, "RustQueen", 1, "Grindmaw the Rust Queen slain")], xp=700, coin=250, items=[("HeartOfTheRustvein", 1)]),
]


def setup_items():
    icons = icon_shapes()
    for spec in ITEMS:
        item = m2.create_or_load("DA_Item_" + spec["id"], "/Game/MMO/Items", unreal.MMOItemDefinition)
        item.set_editor_property("item_id", spec["id"])
        item.set_editor_property("display_name", spec["name"])
        item.set_editor_property("description", spec["desc"])
        item.set_editor_property("flavor_text", spec.get("flavor", ""))
        item.set_editor_property("item_type", spec["type"])
        item.set_editor_property("rarity", spec.get("rarity", R.COMMON))
        item.set_editor_property("category_text", spec["category"])
        item.set_editor_property("max_stack_size", spec["stack"])
        item.set_editor_property("sell_value", spec["sell"])
        item.set_editor_property("equipment_slot", spec.get("slot", S.NONE))
        damage = spec.get("damage", (0, 0))
        item.set_editor_property("weapon_damage_min", float(damage[0]))
        item.set_editor_property("weapon_damage_max", float(damage[1]))
        stats = unreal.MMOStatModifiers()
        stats.set_editor_property("armor", float(spec.get("armor", 0)))
        stats.set_editor_property("max_health", float(spec.get("health", 0)))
        stats.set_editor_property("attack_damage", float(spec.get("attack", 0)))
        item.set_editor_property("stats", stats)
        item.set_editor_property("icon", m2.import_icon(spec["id"], icons[spec["id"]]))
        item.set_editor_property("equipped_mesh", None)
        unreal.EditorAssetLibrary.save_loaded_asset(item, False)
        log("Item %-20s %s" % (spec["id"], item.get_path_name()))


def setup_loot():
    for name, spec in LOOT.items():
        table = m2.create_or_load(name, "/Game/MMO/Loot", unreal.MMOLootTable)
        entries = []
        for item_id, chance, qmin, qmax in spec["entries"]:
            entry = unreal.MMOLootEntry()
            entry.set_editor_property("item", unreal.load_asset("/Game/MMO/Items/DA_Item_" + item_id))
            entry.set_editor_property("drop_chance", chance)
            entry.set_editor_property("min_quantity", qmin)
            entry.set_editor_property("max_quantity", qmax)
            entries.append(entry)
        table.set_editor_property("entries", entries)
        chance, cmin, cmax = spec["currency"]
        table.set_editor_property("currency_chance", chance)
        table.set_editor_property("min_currency", cmin)
        table.set_editor_property("max_currency", cmax)
        unreal.EditorAssetLibrary.save_loaded_asset(table, False)
        log("Loot table %s (%d entries)" % (name, len(entries)))


def setup_quests():
    saved = m4.QUESTS
    m4.QUESTS = QUESTS
    try:
        # prerequisites from earlier milestones are looked up by id
        for spec in QUESTS:
            if spec.get("prereq") and spec["prereq"] not in [q["id"] for q in QUESTS]:
                spec["_prereq_asset"] = unreal.load_asset("/Game/MMO/Quests/DA_Quest_" + spec["prereq"])
        m4.setup_quests({})
        for spec in QUESTS:
            if spec.get("_prereq_asset"):
                quest = unreal.load_asset("/Game/MMO/Quests/DA_Quest_" + spec["id"])
                quest.set_editor_property("prerequisite", spec["_prereq_asset"])
                unreal.EditorAssetLibrary.save_loaded_asset(quest, False)
    finally:
        m4.QUESTS = saved


def make_sounds():
    rng = random.Random(909)
    s = {}
    chitter = []
    for k in range(14):
        m2.mix(chitter, m2.tone(0.025, rng.uniform(2200, 3400), None, (1.0, 0.5), decay=20), 0.03 * k + rng.uniform(0, 0.012), 0.6)
    m2.mix(chitter, m2.noise(0.4, rng, 5, 0.6), 0.0, 0.15)
    s["S_MMO_BeetleChitter"] = m2.normalize(chitter, 0.45)
    bite = m2.mix(m2.noise(0.12, rng, 12, 0.8), m2.tone(0.1, 300, 160, (1.0, 0.4), decay=10), 0.0, 0.6)
    s["S_MMO_BeetleBite"] = m2.normalize(bite, 0.5)
    death = m2.mix(m2.noise(0.5, rng, 4, 0.25), m2.tone(0.4, 900, 200, (1.0, 0.3), decay=5), 0.0, 0.4)
    for k in range(6):
        m2.mix(death, m2.tone(0.03, rng.uniform(1800, 2600), None, (1.0,), decay=20), 0.05 + 0.06 * k, 0.3)
    s["S_MMO_BeetleDeath"] = m2.normalize(death, 0.5)
    boom = m2.mix(m2.tone(0.9, 70, 40, (1.0, 0.6, 0.3), decay=4), m2.noise(0.8, rng, 3, 0.35), 0.0, 0.8)
    m2.mix(boom, m2.noise(0.6, rng, 2.5, 0.9), 0.05, 0.25)
    s["S_MMO_Eruption"] = m2.normalize(boom, 0.75)
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


def write_log():
    out = os.environ.get("MMO_SETUP_LOG")
    if out:
        with open(out, "w") as f:
            f.write("\n".join(LOG + m4.LOG))


try:
    setup_items()
    setup_loot()
    setup_quests()
    setup_audio()
    write_log()
except Exception as error:
    log("ERROR: %s" % error)
    write_log()
    raise
