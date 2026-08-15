# Changelog

## 0.4.0

Scenes join scripts: trigger any Home Assistant scene exactly like a script.

- Picker now lists scenes alongside scripts (one merged list, same
  OFF / ON / CONFIRM cycling, same 32-shortcut cap)
- Execution routes by type: `scene.turn_on` / `script.turn_on`, entity-id
  based, so renames / duplicates / same-named scene+script pairs work
- Type visible at a glance: the main list's second line leads with a
  symbol — `$` for scripts, a play triangle for scenes — then
  `·`-joined area, tags and icon name; the symbol is always followed
  by a `·` divider and overflow truncates at the end, so the beginning of
  the line stays fixed
- New "Info line" sub-menu setting (SELECT cycles, like Automatic close):
  choose which fields the main-screen line shows — None, T·A·Tg·C,
  T·A·Tg, T·A, A·Tg·C, T·Tg·C, `none - only name` (only the name, 24pt
  bold and centered, no icon/subtitle) or `none - big name` (icon plus
  left-aligned name, 24pt, vertically centered, no subtitle) — persisted
  on the watch
- Fixed: scene subtitle had no divider/space between the play symbol and
  the area text (symbol touched the text); scripts already had `$ · `
- Shortcut edit cards use a strict four-region label/value table: banner |
  top split (Type | Area) | state band | bottom split (Tags | Category) —
  muted bold labels, full-width rows, no fills, nothing overlaps the state
  band; full entity id as footer — same-named scene/script rows stay
  distinct
- Edit cards now respect dark/light mode: page, labels, values and footer
  follow the app theme instead of always being white
- The icon is only ever shown as the banner glyph — its mdi name is no
  longer rendered as text anywhere on the edit card
- Category row: HA categories (entity-registry scope mapping) are exposed
  only over WebSocket, which PebbleKit JS cannot use, so the row shows
  `—` until HA exposes them via REST/templates; the earlier values under
  "Category" were the entities' icon names, which is what made it confusing
- Category falls back to the HA domain default (script-text / palette)
  when an entity has no icon
- Scene/script pairs with the same key are independent shortcuts:
  launcher slots, reorder, removed-position restore and metadata refresh
  are all type-aware
- Old stored shortcuts migrate cleanly (all existing entries stay scripts)
- Fixed: browse parser no longer drops scene entities (was script-only)

## 0.3.0

Launch Home Assistant scripts from your wrist, one tap.

- Stored shortcuts -> instant launch, no browsing
- Picker auto-fetches all HA scripts (names, areas, labels, icons) and
  refreshes the stored metadata; update it any time from the sub-menu
- Per-shortcut OFF / ON / CONFIRM; the confirm screen is a quick step —
  execution then runs from the main screen exactly like without it
- Missing scripts show a red `!`; SELECT deletes them right from the list,
  BACK keeps them (HA may be temporarily unreachable)
- Robust against renamed / duplicated / deleted scripts: execution uses
  `script.turn_on` by entity id, so entity-id renames and duplicates work;
  unavailable ghosts are never listed
- Icon clustering: 79 glyphs cover about half of Material Design Icons via
  concept clusters (all `garage-*` -> one garage, `bed-*` -> home, ...)
- Automatic close (Never/3s/5s/10s/15s/30s), set on the watch
- Touch-ready; dark/light themes + accent color
- Reorder that only commits when you drop; order never touched by updates
- Up to 32 shortcuts (comfortable for a watch; keeps the app lean)
- Fixed: CONFIRM flow freeze, invisible icons, missing labels/tags,
  exec-feedback text, 400-on-ghost-script execution
- Built with AI, maintained with love
