"""
Milestone 4 content setup (quest reward items + the Thornwick starter quests). Run headless with the editor closed:

  UnrealEditor-Cmd.exe <path>/MMO.uproject -run=pythonscript -script="<path>/Tools/Python/setup_m4_quests.py"

Idempotent: existing assets are updated in place. It
  1. creates the quest reward items (with icons) in /Game/MMO/Items,
  2. creates the quest definitions in /Game/MMO/Quests (DA_Quest_<QuestId>).
NPCs that give these quests are placed by build_m3_zone.py.
"""

import os
import sys

import unreal

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import setup_m2_items as m2  # noqa: E402  (icon rasterizer + item helpers)

LOG = []


def log(message):
    LOG.append(str(message))
    unreal.log("[MMO setup] " + str(message))


R = unreal.MMOItemRarity
T = unreal.MMOItemType
S = unreal.MMOEquipmentSlot
poly, ell, line = m2.polygon, m2.ellipse, m2.line


def icon_shapes():
    leather = (132, 92, 58)
    return {
        "StitchedWolfhideGloves": [
            polygon_glove(leather),
            line(40, 62, 92, 62, 3, (86, 58, 36)),
            line(46, 74, 46, 108, 3, (196, 180, 150)),
            line(66, 74, 66, 108, 3, (196, 180, 150)),
            line(86, 74, 86, 108, 3, (196, 180, 150)),
            poly([(34, 96), (98, 96), (98, 116), (34, 116)], (118, 112, 104)),
        ],
        "WardensLeatherCap": [
            poly([(20, 84), (28, 54), (46, 34), (64, 28), (82, 34), (100, 54), (108, 84)], (112, 76, 44)),
            poly([(12, 84), (116, 84), (110, 100), (18, 100)], (84, 56, 32)),
            line(64, 30, 64, 84, 4, (150, 108, 66)),
            ell(98, 52, 6, 14, (60, 120, 70), 0.6),
            line(100, 40, 112, 22, 3, (60, 120, 70)),
        ],
        "WardensShortsword": [
            poly([(30, 104), (26, 100), (86, 40), (100, 28), (104, 32), (90, 46)], (196, 204, 214)),
            line(36, 96, 96, 36, 3, (244, 248, 255)),
            line(30, 78, 54, 102, 9, (84, 92, 104)),
            line(18, 116, 36, 98, 9, (70, 52, 36)),
            ell(16, 118, 7, 7, (190, 160, 90)),
        ],
        "DirefangPendant": [
            line(30, 14, 64, 52, 3, (190, 170, 110)),
            line(98, 14, 64, 52, 3, (190, 170, 110)),
            poly([(50, 50), (78, 50), (76, 68), (70, 92), (64, 116), (58, 92), (52, 68)], (236, 226, 204)),
            poly([(50, 50), (78, 50), (78, 60), (50, 60)], (120, 30, 30)),
            ell(64, 55, 7, 7, (230, 40, 40)),
            line(60, 66, 62, 104, 3, (255, 250, 240)),
        ],
    }


def polygon_glove(color):
    return poly([(34, 100), (34, 60), (30, 48), (24, 34), (32, 30), (42, 46), (44, 24), (52, 22), (56, 44), (60, 16), (68, 16),
                 (70, 44), (76, 20), (84, 22), (84, 46), (92, 30), (100, 34), (96, 60), (98, 100)], color)


ITEMS = [
    dict(id="StitchedWolfhideGloves", name="Stitched Wolfhide Gloves", rarity=R.UNCOMMON, type=T.ARMOR, category="Leather Gloves",
         desc="Brenna's own needlework: warm wolfhide with the fur turned inward.", stack=1, sell=30,
         slot=S.HANDS, armor=4, health=5),
    dict(id="WardensLeatherCap", name="Warden's Leather Cap", rarity=R.COMMON, type=T.ARMOR, category="Leather Helm",
         desc="A sturdy cap issued to the Thornwick watch, with a sprig of thornleaf in the band.", stack=1, sell=18,
         slot=S.HEAD, armor=5),
    dict(id="WardensShortsword", name="Warden's Shortsword", rarity=R.UNCOMMON, type=T.WEAPON, category="One-Handed Sword",
         desc="A well-kept blade from the watch armory. Better balanced than it looks.", stack=1, sell=60,
         slot=S.MAIN_HAND, damage=(14, 19)),
    dict(id="DirefangPendant", name="Direfang Pendant", rarity=R.RARE, type=T.ARMOR, category="Amulet",
         desc="The great beast's fang, bound in bronze wire by Doran Ironmantle.",
         flavor="\"It still feels warm. Doran swears that's just the forge.\"",
         stack=1, sell=120, slot=S.AMULET, health=15, attack=2),
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
        assets[spec["id"]] = item
        log("Item %-24s %s" % (spec["id"], item.get_path_name()))
    return assets


# ---------------------------------------------------------------------------------------------------------------------
# Quests
# ---------------------------------------------------------------------------------------------------------------------

O = unreal.MMOQuestObjectiveType

QUESTS = [
    dict(id="WolvesAtTheGate", title="Wolves at the Gate", level=1, giver="Hollis", turnin="Hollis",
         desc="Another lamb taken from the Ashdown pen last night, and tracks right up to the east gate. The Grey Wolves in "
              "Whispering Meadow are getting bold, and I've only got two pairs of eyes on the wall.\n\n"
              "Thin the pack out there for me. Five of them should teach the rest to keep their distance.",
         summary="Warden Hollis wants you to slay 5 Grey Wolves in Whispering Meadow, east of the village gate.",
         progress="Still hearing howls from the meadow. Keep at it.",
         complete="That's the quietest the meadow's been all month. You've a steady hand, friend. Take this, the village can spare it.",
         objectives=[(O.KILL, "GreyWolf", 5, "Grey Wolves slain")], xp=150, coin=30, items=[("MinorHealingPotion", 2)]),
    dict(id="PeltsForTheHearth", title="Pelts for the Hearth", level=1, giver="Brenna", turnin="Brenna",
         desc="Winter comes early to Thornwick, and half the beds upstairs still have summer blankets on them. "
              "If you're out among the wolves anyway, bring me back some good pelts. I'll stitch you something warm for your trouble.",
         summary="Brenna Ashdown at the Thornwick inn needs 4 Wolf Pelts for winter blankets.",
         progress="No pelts yet? The wolves in the meadow have the thickest coats.",
         complete="Oh, these are lovely and thick. Here, I made these from the last batch. Try them on.",
         objectives=[(O.COLLECT, "WolfPelt", 4, "Wolf Pelts")], xp=120, coin=40, items=[("StitchedWolfhideGloves", 1)]),
    dict(id="EyesOnTheWild", title="Eyes on the Wild", level=2, giver="Hollis", turnin="Hollis", prereq="WolvesAtTheGate",
         desc="The wolves didn't get bold on their own. Something is pushing them toward us. I need someone to scout further out.\n\n"
              "Climb to the old watchtower on Sentinel's Rise and look over the land, then walk the edge of the Greywood. "
              "Come back and tell me what you saw.",
         summary="Scout Sentinel's Rise and the Greywood for Warden Hollis.",
         progress="Seen the Rise yet? The tower's north-east of the meadow, the Greywood past the wagon road.",
         complete="Dens in the Greywood, and worse further east... Aye, that matches what the woodcutters said. Here, you've earned a proper cap.",
         objectives=[(O.DISCOVER, "SentinelsRise", 1, "Scout Sentinel's Rise"), (O.DISCOVER, "Greywood", 1, "Scout the Greywood")],
         xp=120, coin=0, items=[("WardensLeatherCap", 1)]),
    dict(id="TheHowlingDen", title="The Howling Den", level=3, giver="Hollis", turnin="Hollis", prereq="EyesOnTheWild",
         desc="The pack has a den in the rocks at the south end of the Greywood. As long as it stands, the meadow will keep filling up with wolves.\n\n"
              "Go to the Howling Den and slay the wolves that guard it. Watch yourself; den wolves fight harder than the strays.",
         summary="Slay 3 Den Wolves at the Howling Den in the southern Greywood.",
         progress="The den still stands? Be careful in there.",
         complete="You cleared the den? Then the meadow will be quiet for a season at least. Take this blade from the armory; you'll want it where you're headed.",
         objectives=[(O.KILL, "DenWolf", 3, "Den Wolves slain")], xp=250, coin=60, items=[("WardensShortsword", 1)]),
    dict(id="FangsForTheForge", title="Fangs for the Forge", level=2, giver="Doran", turnin="Doran",
         desc="Wolf fang makes the best rivet-punches you'll find this side of the mountains. Hard, sharp, and they don't chip. "
              "Bring me five and I'll pay you better than the traders do.",
         summary="Doran Ironmantle, the village smith, wants 5 Wolf Fangs.",
         progress="Fangs, friend. Five of them. The forge doesn't wait.",
         complete="Good teeth, these. Here's your coin, and my thanks.",
         objectives=[(O.COLLECT, "WolfFang", 5, "Wolf Fangs")], xp=140, coin=50),
    dict(id="TheBeastOfFanghollow", title="The Beast of Fanghollow", level=4, giver="Doran", turnin="Doran", prereq="TheHowlingDen",
         desc="Hollis tells me you broke the den. Then you should know what drove those wolves out of the east.\n\n"
              "There's a Dire Wolf in Fanghollow, past the giant dead tree. Twice the size of any wolf you've seen, and it took "
              "my brother's mule last spring. Kill it, bring me proof, and I'll make you something worth wearing.",
         summary="Slay the Dire Wolf that prowls Fanghollow, east of the Greywood.",
         progress="Is the beast dead? No? Then rest up and try again. It isn't going anywhere.",
         complete="By the forge... that's its fang? Give me a moment with it. There. Wear it proud, Thornwick won't forget this.",
         objectives=[(O.KILL, "DireWolf", 1, "Dire Wolf slain")], xp=400, coin=150, items=[("DirefangPendant", 1)]),
]


def setup_quests(items):
    quests = {}
    for spec in QUESTS:
        quest = m2.create_or_load("DA_Quest_" + spec["id"], "/Game/MMO/Quests", unreal.MMOQuestDefinition)
        quest.set_editor_property("quest_id", spec["id"])
        quest.set_editor_property("title", spec["title"])
        quest.set_editor_property("description", spec["desc"])
        quest.set_editor_property("summary", spec["summary"])
        quest.set_editor_property("progress_text", spec["progress"])
        quest.set_editor_property("completion_text", spec["complete"])
        quest.set_editor_property("recommended_level", spec["level"])
        quest.set_editor_property("giver_id", spec["giver"])
        quest.set_editor_property("turn_in_id", spec["turnin"])
        quest.set_editor_property("prerequisite", quests.get(spec.get("prereq")))
        objectives = []
        for kind, target, count, text in spec["objectives"]:
            objective = unreal.MMOQuestObjective()
            objective.set_editor_property("type", kind)
            objective.set_editor_property("target_id", target)
            objective.set_editor_property("count", count)
            objective.set_editor_property("description", text)
            objectives.append(objective)
        quest.set_editor_property("objectives", objectives)
        quest.set_editor_property("reward_xp", spec["xp"])
        quest.set_editor_property("reward_currency", spec["coin"])
        rewards = []
        for item_id, quantity in spec.get("items", []):
            reward = unreal.MMOItemReward()
            # items from other setup scripts (e.g. potions from Milestone 6) are loaded by id
            item = items.get(item_id) or unreal.load_asset("/Game/MMO/Items/DA_Item_" + item_id)
            if not item:
                log("WARNING: reward item %s does not exist yet (run setup_m6_economy.py)" % item_id)
                continue
            reward.set_editor_property("item", item)
            reward.set_editor_property("quantity", quantity)
            rewards.append(reward)
        quest.set_editor_property("reward_items", rewards)
        unreal.EditorAssetLibrary.save_loaded_asset(quest, False)
        quests[spec["id"]] = quest
        log("Quest %-22s %s" % (spec["id"], quest.get_path_name()))
    return quests


def main():
    items = setup_items()
    setup_quests(items)


def write_log():
    out = os.environ.get("MMO_SETUP_LOG")
    if out:
        with open(out, "w") as f:
            f.write("\n".join(LOG + m2.LOG))


if __name__ == "__main__":
    try:
        main()
        write_log()
    except Exception as error:
        log("ERROR: %s" % error)
        write_log()
        raise
