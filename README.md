# Thornwick — a classic MMO, reinvented (Unreal Engine 5.8 prototype)

A single-player prototype of a classic, slower-paced fantasy MMORPG: tab-target combat with auto-attack,
quests from villagers, loot and gear, consumables and merchants, abilities learned by level, gathering
and crafting professions, and a small dungeon with a boss. All content (names, quests, items, art made
from basic shapes, synthesized sounds) is original.

## Playing

Launch the packaged game (`Builds/…/Windows/MMO.exe`) or press Play in the editor
(map `/Game/MMO/Maps/Lvl_Thornwick`). The title screen offers **Continue** (your character is saved
automatically), **New Adventure**, **Settings** and **Quit**. A small guide panel walks you through the
first steps; it can be turned off in Settings.

| Input | Action |
| --- | --- |
| W A S D, Space | Move, jump |
| Hold a mouse button + drag | Turn the camera (the cursor is free otherwise) |
| Mouse wheel | Zoom |
| Left-click | Select a target |
| Right-click | Attack / loot / talk / gather / use what is under the cursor |
| F | Talk, loot or use whatever is nearby |
| Tab | Next target |
| 1 | Auto attack on / off |
| 2 – 9 | Hotbar: abilities, potions, food (drag items or abilities onto it) |
| B or I / C / K / L | Backpack / character & professions / abilities / quest log |
| Esc | Close windows, then clear the target, then open the game menu |

## What is in the world

- **Thornwick** village: Warden Hollis, Brenna Ashdown (inn, food), Doran Ironmantle (forge, gear),
  Mirelle Thistledown (herbalist, potions) and Old Pell; quest chains with kill / collect / explore goals.
- **Whispering Meadow, Sentinel's Rise, Greywood, the Howling Den, Fanghollow** (the Dire Wolf).
- **Rustvein Mine**: through the boarded entrance in the east cliff. Rustback beetles, and
  **Grindmaw the Rust Queen** — step out of the glowing circles, deal with her brood at 60% and 30%,
  and finish her before the enrage.
- Professions: Mining, Herbalism, Smithing (Thornwick Forge), Alchemy (Mirelle's table), Cooking
  (the cookfire by the inn).

## Project layout

- `Source/MMO` — all gameplay code (C++): `Combat/` (health, XP, auto-attack, abilities, cooldowns),
  `Items/` (inventory, equipment, loot, hotbar, vendors), `Quests/`, `Professions/`, `Creatures/`
  (wolves, rustbacks, the boss, AI), `NPC/`, `World/` (terrain, scatter, zones, nodes, stations,
  portals, telegraphs), `Save/`, `Settings/`, `UI/` (all widgets are built in C++), `Tests/`.
- `Tools/Python` — editor scripts that generate content (items, icons, sounds, quests, abilities,
  recipes, the zone and the mine). Run them headless with the editor closed, in milestone order
  (`setup_m2_items.py` … `setup_m9_dungeon.py`), then `build_m3_zone.py` to rebuild the map.

## Building, testing and packaging

```
# editor / game targets
Engine/Build/BatchFiles/Build.bat MMOEditor Win64 Development -Project=<path>/MMO.uproject
Engine/Build/BatchFiles/Build.bat MMO Win64 Shipping -Project=<path>/MMO.uproject

# automation tests (22)
UnrealEditor-Cmd.exe MMO.uproject -ExecCmds="Automation RunTests MMO; Quit" -unattended -nullrhi

# end-to-end gameplay self-test (Thornwick: ~260 checks; add /Game/ThirdPerson/Lvl_ThirdPerson for the arena)
UnrealEditor.exe MMO.uproject -game -MMONoSave -unattended -nullrhi -ExecCmds="mmo.selftest quit"
#   add "shots" (and drop -nullrhi) to save screenshots to Saved/Screenshots

# package
Engine/Build/BatchFiles/RunUAT.bat BuildCookRun -project=<path>/MMO.uproject -noP4 -platform=Win64
    -clientconfig=Shipping -build -cook -stage -pak -iostore -archive -archivedirectory=<out>
```

Launch options: `-MMONoSave` (don't load or write the character), `-MMONoTitle` (skip the title screen).
Development console commands: `mmo.selftest`, `mmo.tour [names]`, `mmo.goto <place>`, `mmo.give <item> [n]`,
`mmo.currency <copper>`, `mmo.items`, `mmo.save`, `mmo.load`, `mmo.newgame`, `mmo.quitafter <s>`,
`mmo.screenshot <delay> <name>`.

Saves live in `Saved/SaveGames` (`MMO_Character_0.sav`, `MMO_Settings.sav`).
