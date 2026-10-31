# Pick My Loot

Current version: 1.2.6

Requires POE2Fixer 364 or newer.

Pick My Loot picks up the ground items you want. Tick the items in a searchable
list by category, or let it grab anything worth more than a value you set, and
press the start key in game: it picks up matching items near you, nearest
first, by moving the cursor onto each item's label and clicking it.

## How to use it

1. Install **Pick My Loot** from the Plugin Manager (POEFixer's Plugins tab),
   which also keeps it up to date, and tick *Enabled* on its tab.
2. Set the *Start / stop key* at the top (F5 by default): any keyboard key, or
   mouse button 4 or 5 (the left and right buttons cannot be used, the plugin
   clicks with the left button itself).
3. Under **What to pick**, open a category and tick the items you want. Use
   the search box, *Select shown* and *Clear shown* for long lists.
4. In game, press the start key. Press it again to stop. The status chip shows
   what the plugin is doing and how many wanted items are left.

## Settings

**Start** - the *Start / stop key*, and *Put the cursor back when done*: after
the last item of a pile the cursor returns to where it was.

**What to pick** - every category (Currency, Essences, Runes, Soul Cores,
Catalysts, Delirium, Incursion, Idols, Omens, Vault Keys, Fragments, Abyss,
Uncut Gems, Lineage Gems, Verisium, Vaal, Expedition, Waystones, Tablets) has
a checklist with the current price of each item. New league items join their
category by themselves once you have seen one on the ground.

**Value pick**
- *Also pick anything worth at least* - picks any item priced at or above
  *Minimum value*, even if you never ticked it.
- *Tolerance* - lowers the minimum a little (1.00 with 10% picks anything worth
  0.90 or more).
- *Currency* - Divine, Exalted or Chaos Orb, used here and in the lists.

Prices come from POEFixer's own price list (Settings -> Prices); the plugin
makes no internet requests of its own.

**Speed and range**
- *Speed* - Slow, Medium, Fast or Instant use tested timings; *Custom* unlocks
  *Check every*, *Pause after a click* and *Hover before click*.
- *Pickup range* - how far (in map cells) an item may lie.
- *Click height* - how far above the item's ground position the click lands,
  to hit its name label. Raise it if pickups often need a second click.

**Stop picking while**
- *An item panel is open* (inventory, stash, vendor; the full-screen map does
  not stop picking), *In town or a hideout*, *An enemy is near* (with *Enemy
  range*), *You hold the left mouse button*.
- Picking always stops while the POEFixer menu is open, during loading screens,
  while the game window is not in front, while the game is paused and while you
  are dead.

**Display**
- *Show status on screen* - the status chip, with corner, size and opacity.
- *Show pickup range ring* / *Show enemy range ring* - the ranges drawn on the
  ground, with their colours.

**Diagnostics** (collapsed) - the items in range, why one is or is not picked,
and the state of prices and of the link with Auto My Aim.

Pick My Loot tells Auto My Aim when it uses the cursor, so the two plugins
never fight over it.

## Known limits

- The item lists use the English game names. With POEFixer 371 or later the
  plugin recognises items in every game language (POEFixer names each item on
  the ground in English), and an item seen on the ground this session is listed
  as the game words it. An older POEFixer matches only the English client.
- The game's loot filter decides which labels are shown. An item whose label
  the filter hides, or that does not fit in your inventory, cannot be picked:
  after 4 clicks without success it is skipped until you change area or restart
  picking (the settings show a *Retry them* button).
- Clicks aim at the label above the item; very large loot filter fonts may need
  a higher *Click height*.

## Install

Install it from POE2Fixer → Plugins → Plugin Manager (one click, updates included), or download `PickMyLoot.zip` from [Releases](https://github.com/POEFixer/PickMyLoot/releases/latest) and extract it into the POE2Fixer folder (it creates `Plugins\PickMyLoot\`). Then enable the plugin in the Plugins tab.

## Build from source

This repository is the plugin folder from the POE2Fixer source tree (`Plugins/PickMyLoot/`) together with the host files it compiles against, kept at the same relative paths:

- `POEFixer/plugin_sdk/` - the POE2Fixer Plugin SDK v6 headers (`PluginAbi.h`, `PluginSDK.h`)
- `POEFixer/imgui/` - the Dear ImGui sources the DLL compiles against
- `tools/plugin_i18n/gen.py` - the translation tool: after editing `Plugins/PickMyLoot/lang/en.json` run `python tools/plugin_i18n/gen.py Plugins/PickMyLoot` (it rewrites the embedded English header); `--gate` checks every language file and the keys the sources use

1. Install Visual Studio 2022 with the "Desktop development with C++" workload (MSVC v143, Windows 10/11 SDK).
2. Open `PickMyLoot.sln`, select **Release | x64** and build the solution.
3. The plugin is written to `x64\Release\Plugins\PickMyLoot\PickMyLoot.dll` (Release builds carry no debug information, see `Directory.Build.targets`).
4. Copy `PickMyLoot.dll` into `Plugins\PickMyLoot\` next to the POE2Fixer executable, together with `Plugins\PickMyLoot\lang` from this repository (same folder names).

## Credits

Originally created by Omer Faruk ARPA (https://github.com/omrfarukarpa/poefixer-pickmyloot).
Maintained by POEFixer since October 2026.
Licensed under GPL-3.0, see LICENSE.
