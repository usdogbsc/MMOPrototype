"""
Milestone 1B content setup. Run headless with the editor closed:

  UnrealEditor-Cmd.exe <path>/MMO.uproject -run=pythonscript -script="<path>/Tools/Python/setup_m1b_assets.py"

Idempotent: safe to re-run. It
  1. creates /Game/MMO/Animations/A_MMO_PlayerBasicAttack (copy of MM_Attack_01, root motion off, MMO Melee Hit notify
     on Epic's authored hit frame taken from AM_ComboAttack),
  2. synthesizes original placeholder combat sounds into /Game/MMO/Audio,
  3. places a NavMeshBoundsVolume covering the Lvl_ThirdPerson play area.
"""

import math
import os
import random
import struct
import tempfile
import wave

import unreal

LOG = []


def log(message):
    LOG.append(str(message))
    unreal.log("[MMO setup] " + str(message))


# ---------------------------------------------------------------------------------------------------------------------
# 1. Player attack animation with a hit-frame notify
# ---------------------------------------------------------------------------------------------------------------------

SOURCE_ATTACK = "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01"
PLAYER_ATTACK = "/Game/MMO/Animations/A_MMO_PlayerBasicAttack"
# Epic's DoAttackTrace notify for MM_Attack_01 in AM_ComboAttack (section Melee01) sits at 0.4667s
PLAYER_ATTACK_HIT_TIME = 0.4667
NOTIFY_TRACK = "MMOCombat"


def setup_player_attack():
    if not unreal.EditorAssetLibrary.does_asset_exist(PLAYER_ATTACK):
        unreal.EditorAssetLibrary.duplicate_asset(SOURCE_ATTACK, PLAYER_ATTACK)
        log("Duplicated %s -> %s" % (SOURCE_ATTACK, PLAYER_ATTACK))

    anim = unreal.load_asset(PLAYER_ATTACK)
    # in-place swing so the player keeps full movement control during auto-attack
    anim.set_editor_property("enable_root_motion", False)
    anim.set_editor_property("force_root_lock", True)

    tracks = [str(t) for t in unreal.AnimationLibrary.get_animation_notify_track_names(anim)]
    if NOTIFY_TRACK in tracks:
        unreal.AnimationLibrary.remove_animation_notify_events_by_track(anim, NOTIFY_TRACK)
    else:
        unreal.AnimationLibrary.add_animation_notify_track(anim, NOTIFY_TRACK)

    unreal.AnimationLibrary.add_animation_notify_event(anim, NOTIFY_TRACK, PLAYER_ATTACK_HIT_TIME, unreal.MMOAnimNotify_MeleeHit)
    unreal.EditorAssetLibrary.save_asset(PLAYER_ATTACK, only_if_is_dirty=False)
    log("Player attack: root motion off, MMO Melee Hit at %.4fs (length %.3fs)" % (PLAYER_ATTACK_HIT_TIME, unreal.AnimationLibrary.get_sequence_length(anim)))


# ---------------------------------------------------------------------------------------------------------------------
# 2. Synthesized placeholder sounds (original, generated here)
# ---------------------------------------------------------------------------------------------------------------------

RATE = 44100


def silence(seconds):
    return [0.0] * int(seconds * RATE)


def mix(base, layer, offset_seconds=0.0, gain=1.0):
    start = int(offset_seconds * RATE)
    if len(base) < start + len(layer):
        base.extend([0.0] * (start + len(layer) - len(base)))
    for i, v in enumerate(layer):
        base[start + i] += v * gain
    return base


def env(i, n, attack=0.005, release=None, curve=4.0):
    t = i / RATE
    a = min(1.0, t / attack) if attack > 0 else 1.0
    if release is None:
        return a * math.exp(-curve * i / max(1, n))
    return a * max(0.0, 1.0 - t / release)


def lowpass(samples, cutoff):
    out, y = [], 0.0
    alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / RATE)
    for v in samples:
        y += alpha * (v - y)
        out.append(y)
    return out


def highpass(samples, cutoff):
    low = lowpass(samples, cutoff)
    return [a - b for a, b in zip(samples, low)]


def noise(seconds, rng):
    return [rng.uniform(-1.0, 1.0) for _ in range(int(seconds * RATE))]


def sweep(seconds, f0, f1, shape="sine", vibrato=0.0, vib_rate=0.0):
    n, out, phase = int(seconds * RATE), [], 0.0
    for i in range(n):
        t = i / n
        f = f0 * (f1 / f0) ** t
        f *= 1.0 + vibrato * math.sin(2 * math.pi * vib_rate * i / RATE)
        phase += 2 * math.pi * f / RATE
        if shape == "saw":
            out.append(((phase / math.pi) % 2.0) - 1.0)
        else:
            out.append(math.sin(phase))
    return out


def shaped(samples, curve=4.0, attack=0.005):
    n = len(samples)
    return [v * env(i, n, attack, curve=curve) for i, v in enumerate(samples)]


def normalize(samples, peak):
    m = max(1e-6, max(abs(v) for v in samples))
    return [v / m * peak for v in samples]


def make_sounds():
    rng = random.Random(1337)
    sounds = {}

    # swing: soft filtered whoosh
    whoosh = noise(0.24, rng)
    n = len(whoosh)
    whoosh = [v * math.sin(math.pi * i / n) ** 2 for i, v in enumerate(whoosh)]
    sounds["S_MMO_Swing"] = normalize(highpass(lowpass(whoosh, 2200), 300), 0.35)

    # melee impact: low thump + crack
    thump = shaped(sweep(0.18, 120, 50), curve=6)
    crack = shaped(highpass(noise(0.06, rng), 1500), curve=10, attack=0.001)
    crunch = shaped(lowpass(noise(0.12, rng), 1200), curve=7, attack=0.001)
    impact = mix(mix(thump[:], crack, 0.0, 0.7), crunch, 0.0, 0.6)
    sounds["S_MMO_MeleeImpact"] = normalize(impact, 0.8)

    # wolf bite: two teeth snaps + short snarl
    snap = shaped(highpass(noise(0.03, rng), 2500), curve=12, attack=0.0005)
    snarl = shaped(lowpass(sweep(0.16, 190, 150, "saw"), 1100), curve=5, attack=0.01)
    bite = mix(mix(snarl[:], snap, 0.0, 0.9), snap, 0.055, 0.8)
    sounds["S_MMO_WolfBite"] = normalize(bite, 0.75)

    # wolf growl on aggro: rumbling low saw
    growl = lowpass(sweep(0.75, 80, 95, "saw", vibrato=0.08, vib_rate=23), 700)
    n = len(growl)
    growl = [v * min(1.0, i / (0.1 * RATE)) * min(1.0, (n - i) / (0.2 * RATE)) * (0.7 + 0.3 * math.sin(2 * math.pi * 11 * i / RATE)) for i, v in enumerate(growl)]
    sounds["S_MMO_WolfGrowl"] = normalize(growl, 0.6)

    # wolf death: descending yelp then a short whimper
    yelp = shaped(mix(sweep(0.45, 950, 380, vibrato=0.03, vib_rate=9), sweep(0.45, 1900, 760), 0.0, 0.25), curve=3, attack=0.01)
    whimper = shaped(sweep(0.35, 520, 300, vibrato=0.05, vib_rate=7), curve=4, attack=0.03)
    sounds["S_MMO_WolfDeath"] = normalize(mix(yelp, whimper, 0.42, 0.5), 0.6)

    # player hurt: dull thud with a voiced grunt colour
    grunt = shaped(lowpass(sweep(0.2, 150, 110, "saw"), 600), curve=5, attack=0.008)
    thud = shaped(sweep(0.15, 140, 60), curve=7)
    sounds["S_MMO_PlayerHurt"] = normalize(mix(grunt, thud, 0.0, 0.8), 0.6)

    # player death: long descending tone
    sounds["S_MMO_PlayerDeath"] = normalize(shaped(lowpass(sweep(1.1, 320, 70, "saw"), 900), curve=2.5, attack=0.02), 0.6)

    # level up: bright bell arpeggio
    chime = silence(1.4)
    for k, f in enumerate([523.25, 659.25, 783.99, 1046.5]):
        n = int(1.0 * RATE)
        bell = [(math.sin(2 * math.pi * f * i / RATE) + 0.4 * math.sin(2 * math.pi * 2 * f * i / RATE) + 0.15 * math.sin(2 * math.pi * 3 * f * i / RATE)) * math.exp(-4.0 * i / n) for i in range(n)]
        mix(chime, bell, 0.09 * k, 0.5)
    sounds["S_MMO_LevelUp"] = normalize(chime, 0.7)

    # auto-attack toggle ticks
    def tick(f):
        return shaped(sweep(0.035, f, f), curve=8, attack=0.001)
    sounds["S_MMO_AutoAttackOn"] = normalize(mix(tick(1100)[:], tick(1600), 0.045), 0.4)
    sounds["S_MMO_AutoAttackOff"] = normalize(mix(tick(1500)[:], tick(950), 0.045), 0.4)
    return sounds


def write_wav(path, samples):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32767)) for v in samples))


def setup_audio():
    folder = os.path.join(tempfile.gettempdir(), "mmo_m1b_audio")
    os.makedirs(folder, exist_ok=True)
    tasks = []
    for name, samples in make_sounds().items():
        path = os.path.join(folder, name + ".wav")
        write_wav(path, samples)
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = "/Game/MMO/Audio"
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for task in tasks:
        log("Imported %s -> %s" % (os.path.basename(task.filename), list(task.imported_object_paths)))


# ---------------------------------------------------------------------------------------------------------------------
# 3. NavMesh bounds for the test map
# ---------------------------------------------------------------------------------------------------------------------

TEST_MAP = "/Game/ThirdPerson/Lvl_ThirdPerson"
NAV_VOLUME_LABEL = "MMO_NavMeshBounds"


def setup_navmesh():
    unreal.EditorLoadingAndSavingUtils.load_map(TEST_MAP)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()

    existing = [a for a in actors if isinstance(a, unreal.NavMeshBoundsVolume)]
    if existing:
        log("NavMeshBoundsVolume already present (%s); leaving it" % ", ".join(a.get_actor_label() for a in existing))
        return

    # cover every static mesh in the level, with a margin
    lo, hi, count = None, None, 0
    for actor in actors:
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        origin, extent = actor.get_actor_bounds(False)
        if extent.x > 20000 or extent.y > 20000:
            continue  # skip sky spheres and similar
        a, b = origin - extent, origin + extent
        lo = a if lo is None else unreal.Vector(min(lo.x, a.x), min(lo.y, a.y), min(lo.z, a.z))
        hi = b if hi is None else unreal.Vector(max(hi.x, b.x), max(hi.y, b.y), max(hi.z, b.z))
        count += 1

    if count < 5:
        lo, hi = unreal.Vector(-6000, -6000, -500), unreal.Vector(6000, 6000, 2000)
        log("Only %d static meshes loaded; using default bounds" % count)

    margin = unreal.Vector(500, 500, 300)
    lo, hi = lo - margin, hi + margin
    center = (lo + hi) * 0.5
    extent = (hi - lo) * 0.5

    volume = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.NavMeshBoundsVolume, center)
    volume.set_actor_label(NAV_VOLUME_LABEL)
    # the factory builds a 200uu cube brush; scale it to the bounds
    volume.set_actor_scale3d(unreal.Vector(extent.x / 100.0, extent.y / 100.0, extent.z / 100.0))
    try:
        volume.set_editor_property("is_spatially_loaded", False)
    except Exception as error:
        log("Could not mark volume as always loaded: %s" % error)

    vol_origin, vol_extent = volume.get_actor_bounds(False)
    log("Placed %s from %d meshes: center %s extent %s (actor bounds extent %s)" % (NAV_VOLUME_LABEL, count, center, extent, vol_extent))

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("Saved map packages")


def main():
    for step in (setup_player_attack, setup_audio, setup_navmesh):
        try:
            step()
        except Exception as error:
            log("ERROR in %s: %s" % (step.__name__, error))
            raise
    out = os.environ.get("MMO_SETUP_LOG")
    if out:
        with open(out, "w") as f:
            f.write("\n".join(LOG))


main()
