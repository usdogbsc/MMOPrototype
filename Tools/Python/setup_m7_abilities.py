"""
Milestone 7 content setup (player abilities). Run headless with the editor closed:

  UnrealEditor-Cmd.exe <path>/MMO.uproject -run=pythonscript -script="<path>/Tools/Python/setup_m7_abilities.py"

Idempotent. Creates the ability data assets in /Game/MMO/Abilities (DA_Ability_<AbilityId>) with icons in
/Game/MMO/Abilities/Icons, and synthesizes the ability impact / heal sounds into /Game/MMO/Audio.
UMMOAbilityComponent lists these four abilities by default.
"""

import os
import random
import struct
import sys
import wave

import unreal

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import setup_m2_items as m2  # noqa: E402

LOG = []
T = unreal.MMOAbilityTarget
poly, ell, line = m2.polygon, m2.ellipse, m2.line


def log(message):
    LOG.append(str(message))
    unreal.log("[MMO setup] " + str(message))


def icon_shapes():
    gold = (230, 180, 70)
    return {
        "RendingStrike": [
            poly([(0, 0), (128, 0), (128, 128), (0, 128)], (60, 18, 18)),
            line(22, 24, 104, 106, 10, (230, 230, 236)),
            line(40, 18, 112, 88, 4, (200, 30, 30)),
            line(16, 44, 86, 112, 4, (200, 30, 30)),
            ell(96, 104, 7, 10, (210, 20, 20)),
            ell(76, 114, 5, 7, (210, 20, 20)),
        ],
        "ShoulderBash": [
            poly([(0, 0), (128, 0), (128, 128), (0, 128)], (40, 46, 70)),
            ell(56, 70, 34, 30, (150, 156, 170)),
            ell(50, 64, 22, 18, (196, 200, 212)),
            line(90, 40, 116, 26, 5, gold),
            line(96, 64, 122, 64, 5, gold),
            line(90, 88, 116, 102, 5, gold),
        ],
        "SecondWind": [
            poly([(0, 0), (128, 0), (128, 128), (0, 128)], (20, 56, 34)),
            poly([(64, 20), (78, 50), (110, 52), (84, 72), (94, 104), (64, 86), (34, 104), (44, 72), (18, 52), (50, 50)], (110, 230, 120)),
            ell(64, 64, 14, 14, (220, 255, 220)),
        ],
        "CleavingArc": [
            poly([(0, 0), (128, 0), (128, 128), (0, 128)], (64, 40, 16)),
            line(18, 96, 40, 50, 9, (236, 214, 150)),
            line(40, 50, 64, 34, 9, (236, 214, 150)),
            line(64, 34, 88, 50, 9, (236, 214, 150)),
            line(88, 50, 110, 96, 9, (236, 214, 150)),
            line(60, 110, 64, 60, 6, (190, 196, 210)),
            ell(64, 112, 9, 9, (110, 76, 46)),
        ],
    }


ABILITIES = [
    dict(id="RendingStrike", name="Rending Strike", level=2, target=T.ENEMY, cooldown=6, mult=1.0, bonus=4, bleed=15, bleed_time=9,
         desc="A vicious cut: weapon damage plus 4, and the target bleeds for 15 damage over 9 sec."),
    dict(id="ShoulderBash", name="Shoulder Bash", level=3, target=T.ENEMY, cooldown=20, mult=0.5, stun=2.5,
         desc="Slam into the target for half weapon damage and stun it for 2.5 sec. Stunned enemies can't move or attack."),
    dict(id="SecondWind", name="Second Wind", level=4, target=T.SELF, cooldown=45, cast=1.5, heal=0.3,
         desc="Catch your breath and recover 30% of your maximum health. Moving interrupts it."),
    dict(id="CleavingArc", name="Cleaving Arc", level=5, target=T.ENEMIES_IN_FRONT, cooldown=10, mult=0.8, radius=320,
         desc="A wide swing that hits every enemy in front of you for 80% weapon damage. No target needed."),
]


def import_icon(ability_id, shapes):
    path = os.path.join(m2.WORK_DIR, "T_Ability_%s.png" % ability_id)
    m2.write_png(path, m2.render(shapes, outline=(30, 24, 16)))
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = "/Game/MMO/Abilities/Icons"
    task.destination_name = "T_Ability_%s" % ability_id
    task.automated = True
    task.replace_existing = True
    task.save = False
    m2.ASSET_TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset("/Game/MMO/Abilities/Icons/T_Ability_%s" % ability_id)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    return texture


def setup_abilities():
    icons = icon_shapes()
    for spec in ABILITIES:
        ability = m2.create_or_load("DA_Ability_" + spec["id"], "/Game/MMO/Abilities", unreal.MMOAbilityDefinition)
        ability.set_editor_property("ability_id", spec["id"])
        ability.set_editor_property("display_name", spec["name"])
        ability.set_editor_property("description", spec["desc"])
        ability.set_editor_property("required_level", spec["level"])
        ability.set_editor_property("target_type", spec["target"])
        ability.set_editor_property("cooldown", float(spec["cooldown"]))
        ability.set_editor_property("cast_time", float(spec.get("cast", 0)))
        ability.set_editor_property("area_radius", float(spec.get("radius", 300)))
        ability.set_editor_property("weapon_damage_multiplier", float(spec.get("mult", 0)))
        ability.set_editor_property("bonus_damage", float(spec.get("bonus", 0)))
        ability.set_editor_property("bleed_damage", float(spec.get("bleed", 0)))
        ability.set_editor_property("bleed_duration", float(spec.get("bleed_time", 0)))
        ability.set_editor_property("stun_duration", float(spec.get("stun", 0)))
        ability.set_editor_property("self_heal_fraction", float(spec.get("heal", 0)))
        ability.set_editor_property("icon", import_icon(spec["id"], icons[spec["id"]]))
        unreal.EditorAssetLibrary.save_loaded_asset(ability, False)
        log("Ability %-14s level %d  %s" % (spec["id"], spec["level"], ability.get_path_name()))


def make_sounds():
    rng = random.Random(707)
    s = {}
    # ability hit: heavier thump with a metallic edge
    hit = m2.mix(m2.tone(0.25, 120, 60, (1.0, 0.5), decay=7), m2.noise(0.14, rng, 9, 0.5), 0.0, 0.7)
    m2.mix(hit, m2.tone(0.2, 1700, 1500, (1.0, 0.3), decay=9), 0.01, 0.25)
    s["S_MMO_AbilityHit"] = m2.normalize(hit, 0.6)
    # heal: warm rising chord
    heal = []
    for k, f in enumerate([392.0, 493.88, 587.33, 783.99]):
        m2.mix(heal, m2.tone(1.0, f, f * 1.01, (1.0, 0.25), decay=3.5, attack=0.08), 0.06 * k, 0.4)
    s["S_MMO_Heal"] = m2.normalize(heal, 0.5)
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
            f.write("\n".join(LOG))


try:
    setup_abilities()
    setup_audio()
    write_log()
except Exception as error:
    log("ERROR: %s" % error)
    write_log()
    raise
