"""
Milestone 8 content setup (gathering and crafting). Run headless with the editor closed:

  UnrealEditor-Cmd.exe <path>/MMO.uproject -run=pythonscript -script="<path>/Tools/Python/setup_m8_crafting.py"

Idempotent. It
  1. creates gathered materials and crafted items (with icons) in /Game/MMO/Items,
  2. creates the recipes in /Game/MMO/Recipes (DA_Recipe_<RecipeId>),
  3. creates two gathering quests (Doran, Mirelle).
Nodes and crafting stations are placed by build_m3_zone.py (run it afterwards).
"""

import os
import sys

import unreal

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import setup_m2_items as m2  # noqa: E402
import setup_m4_quests as m4  # noqa: E402

LOG = []
R = unreal.MMOItemRarity
T = unreal.MMOItemType
S = unreal.MMOEquipmentSlot
P = unreal.MMOProfession
poly, ell, line = m2.polygon, m2.ellipse, m2.line


def log(message):
    LOG.append(str(message))
    unreal.log("[MMO setup] " + str(message))


def icon_shapes():
    copper = (196, 102, 48)
    return {
        "CopperOre": [
            poly([(20, 84), (34, 46), (62, 30), (96, 40), (112, 76), (98, 104), (54, 110), (26, 102)], (96, 90, 84)),
            poly([(44, 56), (60, 48), (66, 64), (52, 72)], copper),
            poly([(76, 60), (92, 58), (94, 76), (80, 80)], copper),
            poly([(56, 84), (72, 82), (70, 98), (56, 96)], copper),
        ],
        "CopperBar": [
            poly([(14, 74), (40, 50), (114, 50), (88, 74)], (232, 150, 90)),
            poly([(14, 74), (88, 74), (88, 96), (14, 96)], copper),
            poly([(88, 74), (114, 50), (114, 72), (88, 96)], (150, 74, 34)),
        ],
        "Thornleaf": [
            line(64, 116, 64, 40, 5, (70, 120, 50)),
            poly([(64, 70), (28, 48), (20, 30), (48, 40)], (110, 190, 80)),
            poly([(64, 56), (100, 36), (110, 18), (80, 30)], (110, 190, 80)),
            poly([(64, 90), (96, 82), (112, 64), (82, 70)], (90, 170, 70)),
            ell(64, 32, 10, 10, (240, 240, 220)),
        ],
        "Duskroot": [
            poly([(52, 20), (76, 20), (80, 60), (92, 100), (70, 116), (58, 116), (36, 100), (48, 60)], (90, 56, 40)),
            line(48, 94, 24, 112, 4, (90, 56, 40)),
            line(80, 94, 104, 112, 4, (90, 56, 40)),
            ell(64, 18, 18, 10, (120, 50, 150)),
            ell(56, 14, 6, 6, (170, 90, 200)),
        ],
        "EmptyVial": [
            poly([(54, 14), (74, 14), (74, 44), (96, 66), (98, 92), (86, 110), (64, 116), (42, 110), (30, 92), (32, 66), (54, 44)], (200, 214, 226)),
            poly([(50, 6), (78, 6), (78, 18), (50, 18)], (130, 90, 50)),
            ell(46, 80, 5, 12, (240, 248, 255)),
        ],
        "CopperBand": [
            ell(64, 70, 38, 38, copper),
            ell(64, 70, 26, 26, (40, 30, 26)),
            ell(64, 34, 10, 8, (90, 200, 160)),
        ],
        "CopperStuddedGloves": [
            m4.polygon_glove((120, 82, 52)),
            ell(46, 62, 5, 5, copper), ell(66, 62, 5, 5, copper), ell(86, 62, 5, 5, copper),
            ell(56, 80, 5, 5, copper), ell(76, 80, 5, 5, copper),
            poly([(34, 96), (98, 96), (98, 116), (34, 116)], copper),
        ],
        "CopperforgedBlade": [
            poly([(26, 108), (22, 104), (88, 34), (106, 18), (110, 22), (94, 40)], (220, 140, 80)),
            line(30, 100, 102, 26, 3, (255, 210, 160)),
            line(26, 78, 52, 104, 9, copper),
            line(14, 118, 32, 100, 9, (70, 46, 30)),
            ell(12, 120, 7, 7, copper),
        ],
        "HealingPotion": [
            poly([(52, 14), (76, 14), (76, 40), (100, 62), (104, 86), (94, 106), (64, 116), (34, 106), (24, 86), (28, 62), (52, 40)], (200, 210, 220)),
            poly([(30, 62), (98, 62), (102, 88), (92, 104), (64, 112), (36, 104), (26, 88)], (170, 20, 60)),
            ell(48, 80, 6, 12, (255, 140, 170)),
            poly([(50, 6), (78, 6), (78, 18), (50, 18)], (200, 170, 70)),
        ],
        "RoastedWolfHaunch": [
            ell(58, 62, 40, 30, (150, 76, 34), 0.5),
            ell(54, 58, 30, 20, (186, 104, 50), 0.5),
            line(84, 84, 108, 108, 12, (236, 230, 214)),
            ell(110, 104, 8, 8, (236, 230, 214)),
            ell(104, 112, 8, 8, (236, 230, 214)),
        ],
    }


ITEMS = [
    dict(id="CopperOre", name="Copper Ore", type=T.MATERIAL, category="Mining", desc="Greenish-orange ore from the hills around Thornwick.", stack=20, sell=2),
    dict(id="CopperBar", name="Copper Bar", type=T.MATERIAL, category="Smithing", desc="A bar of smelted copper, soft and easy to work.", stack=20, sell=6),
    dict(id="Thornleaf", name="Thornleaf", type=T.MATERIAL, category="Herb", desc="A prickly meadow herb with small white flowers. The base of most healing tonics.", stack=20, sell=2),
    dict(id="Duskroot", name="Duskroot", type=T.MATERIAL, category="Herb", desc="A purple-capped root that only grows in the shade of the Greywood.", stack=20, sell=4),
    dict(id="EmptyVial", name="Empty Vial", type=T.MATERIAL, category="Alchemy", desc="A small glass vial with a cork.", stack=20, sell=1, buy=4),
    dict(id="CopperBand", name="Copper Band", rarity=R.UNCOMMON, type=T.ARMOR, category="Ring", desc="A simple copper ring set with a chip of green stone.",
         stack=1, sell=25, slot=S.RING, armor=2, health=8),
    dict(id="CopperStuddedGloves", name="Copper-Studded Gloves", rarity=R.UNCOMMON, type=T.ARMOR, category="Leather Gloves",
         desc="Wolfhide gloves reinforced with copper studs across the knuckles.", stack=1, sell=40, slot=S.HANDS, armor=7),
    dict(id="CopperforgedBlade", name="Copperforged Blade", rarity=R.UNCOMMON, type=T.WEAPON, category="One-Handed Sword",
         desc="A bright copper sword with a wolf-fang inlay. Your own work.", stack=1, sell=90, slot=S.MAIN_HAND, damage=(15, 21)),
    dict(id="HealingPotion", name="Healing Potion", type=T.CONSUMABLE, category="Potion", desc="A deep red draught brewed with duskroot.",
         stack=10, sell=15, heal=120, cooldown=60, group="Potion", combat=True),
    dict(id="RoastedWolfHaunch", name="Roasted Wolf Haunch", type=T.CONSUMABLE, category="Food", desc="Charred on the outside, juicy within.",
         stack=20, sell=5, hot=140, duration=18, cooldown=2, group="Food", combat=False),
]

RECIPES = [
    dict(id="SmeltCopper", profession=P.SMITHING, skill=1, output="CopperBar", qty=1, time=2.0, ingredients=[("CopperOre", 2)]),
    dict(id="CopperBand", profession=P.SMITHING, skill=5, output="CopperBand", qty=1, time=2.5, ingredients=[("CopperBar", 3)]),
    dict(id="CopperStuddedGloves", profession=P.SMITHING, skill=10, output="CopperStuddedGloves", qty=1, time=3.0, ingredients=[("CopperBar", 4), ("WolfPelt", 2)]),
    dict(id="CopperforgedBlade", profession=P.SMITHING, skill=20, output="CopperforgedBlade", qty=1, time=3.5, ingredients=[("CopperBar", 8), ("WolfFang", 2)]),
    dict(id="BrewMinorHealingPotion", profession=P.ALCHEMY, skill=1, output="MinorHealingPotion", qty=1, time=2.0, ingredients=[("Thornleaf", 2), ("EmptyVial", 1)]),
    dict(id="BrewHealingPotion", profession=P.ALCHEMY, skill=15, output="HealingPotion", qty=1, time=2.5, ingredients=[("Duskroot", 2), ("Thornleaf", 1), ("EmptyVial", 1)]),
    dict(id="RoastWolfHaunch", profession=P.COOKING, skill=1, output="RoastedWolfHaunch", qty=1, time=2.0, ingredients=[("RawWolfMeat", 2)]),
]

QUESTS = [
    dict(id="AVeinOfCopper", title="A Vein of Copper", level=2, giver="Doran", turnin="Doran",
         desc="Wolf fangs are well and good, but a smith needs metal. There's copper in the rocks around the meadow and up on Sentinel's Rise; "
              "you'll know it by the orange glint.\n\nBring me six chunks of copper ore. Then use my forge yourself; two ore makes a bar, and a few bars make something worth wearing.",
         summary="Mine 6 Copper Ore from the copper veins around Whispering Meadow for Doran Ironmantle.",
         progress="Six chunks of copper. Look for the rocks with the orange glint.",
         complete="That's good ore. Keep the next batch for yourself; my forge is yours to use any time.",
         objectives=[(m4.O.COLLECT, "CopperOre", 6, "Copper Ore")], xp=150, coin=40, items=[("CopperBar", 2)]),
    dict(id="ThornleafHarvest", title="Thornleaf Harvest", level=1, giver="Mirelle", turnin="Mirelle",
         desc="My thornleaf beds were trampled by those wolves, and the whole village wants potions. Wild thornleaf grows all over the meadow; "
              "look for the white flowers.\n\nBring me six sprigs and I'll pay you in potions. Better yet, I'll let you use my table to brew your own.",
         summary="Gather 6 Thornleaf in Whispering Meadow for Mirelle Thistledown.",
         progress="Thornleaf has white flowers and catches on your sleeves. You'll know it.",
         complete="Lovely, fresh sprigs. Here, three potions for your trouble, and the table is yours whenever you need it.",
         objectives=[(m4.O.COLLECT, "Thornleaf", 6, "Thornleaf")], xp=120, coin=20, items=[("MinorHealingPotion", 3)]),
]


def setup_items():
    icons = icon_shapes()
    for spec in ITEMS:
        item = m2.create_or_load("DA_Item_" + spec["id"], "/Game/MMO/Items", unreal.MMOItemDefinition)
        item.set_editor_property("item_id", spec["id"])
        item.set_editor_property("display_name", spec["name"])
        item.set_editor_property("description", spec["desc"])
        item.set_editor_property("item_type", spec["type"])
        item.set_editor_property("rarity", spec.get("rarity", R.COMMON))
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
        stats.set_editor_property("max_health", float(spec.get("health", 0)))
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
        log("Item %-22s %s" % (spec["id"], item.get_path_name()))


def item(item_id):
    asset = unreal.load_asset("/Game/MMO/Items/DA_Item_" + item_id)
    if not asset:
        raise RuntimeError("missing item " + item_id)
    return asset


def setup_recipes():
    for spec in RECIPES:
        recipe = m2.create_or_load("DA_Recipe_" + spec["id"], "/Game/MMO/Recipes", unreal.MMORecipeDefinition)
        recipe.set_editor_property("recipe_id", spec["id"])
        recipe.set_editor_property("profession", spec["profession"])
        recipe.set_editor_property("required_skill", spec["skill"])
        recipe.set_editor_property("output", item(spec["output"]))
        recipe.set_editor_property("output_quantity", spec["qty"])
        recipe.set_editor_property("craft_time", float(spec["time"]))
        ingredients = []
        for item_id, quantity in spec["ingredients"]:
            ingredient = unreal.MMOIngredient()
            ingredient.set_editor_property("item", item(item_id))
            ingredient.set_editor_property("quantity", quantity)
            ingredients.append(ingredient)
        recipe.set_editor_property("ingredients", ingredients)
        unreal.EditorAssetLibrary.save_loaded_asset(recipe, False)
        log("Recipe %-24s %s" % (spec["id"], recipe.get_path_name()))


def setup_quests():
    saved = m4.QUESTS
    m4.QUESTS = QUESTS
    try:
        m4.setup_quests({})
    finally:
        m4.QUESTS = saved


def write_log():
    out = os.environ.get("MMO_SETUP_LOG")
    if out:
        with open(out, "w") as f:
            f.write("\n".join(LOG + m4.LOG))


try:
    setup_items()
    setup_recipes()
    setup_quests()
    write_log()
except Exception as error:
    log("ERROR: %s" % error)
    write_log()
    raise
