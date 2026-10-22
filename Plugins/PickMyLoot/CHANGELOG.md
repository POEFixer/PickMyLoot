# Changelog

## 1.2.1

- Now maintained by POEFixer: new settings tab in the POEFixer style (cards, clear labels, explanations on every option).
- Prices now come from POEFixer's own price list: no separate downloads, and unique-item prices no longer leak onto plain bases.
- New league items appear in their category by themselves once you have seen one on the ground.
- Clicks now land correctly when the game runs in a window or on a second monitor.
- An item that cannot be picked (full inventory, hidden label) is no longer clicked forever: it is skipped after 4 tries.
- The cursor returns once after a whole pile instead of after every item, and Auto My Aim resumes right after picking ends.
- The enemy check no longer counts friendly or hidden monsters, and less work is done per frame.
- "An item panel is open" now really notices an opened inventory, stash or vendor window (it could miss one and click into it), and picking waits until it has checked.
- All reading of the game (ground items, monsters, inventories, prices) moved to a background thread: picking no longer costs frame time.
- Settings are saved right after a change and can no longer be lost or half-written.
- A settings file that cannot be read (a typo after a hand edit, or another program holding the file) is no longer replaced by the defaults: a broken file is kept as `settings.json.corrupt`, and nothing is saved until you change a setting.
- The start key is set with a click, then a key press (Esc cancels, Clear removes it); the left and right mouse buttons are refused.
- On a wide settings pane the item categories sit in two columns.

## 1.2.0

- One-click speed profiles: Slow, Medium, Fast, Instant and Custom.
- The enemy gate is opt-in by default.
