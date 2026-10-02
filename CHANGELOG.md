# Changelog

## 0.8.0

The menu is two pages now, and the label filter loads itself.

- 3-dot menu split in two: **Shortcuts** (Pick shortcuts / Change Order /
  Update metadata / Label filter) and **Settings** (Automatic close /
  Info line / Vibrations) — everything that shapes the shortcut list sits
  together, the display and haptic settings sit together
- The label list is fetched when the Shortcuts page opens (labels only: one
  request, no entity dump) instead of waiting for a manual picker or
  "Update metadata" run, and it is cached in flash — so the row cycles your
  labels on the first open, right after a restart, and once settings are
  saved
- Picker row renamed to "Pick shortcuts" so the page and its first row are
  not both called "Shortcuts"; the row reads "Fetching labels..." while the
  list is on its way
- A failed label fetch replies with an empty list rather than nothing, so the
  row falls back to All instead of claiming to fetch forever
- Nothing to migrate: the stored label filter keeps working

## 0.7.0

Label filter: show only the shortcuts you tagged for the watch.

- New "Label filter" sub-menu row (SELECT cycles All → every Home Assistant
  label the last fetch saw); the picker then lists only scripts/scenes
  carrying the chosen label, and blank lists everything, as before
- The offered labels are the ones in use by scripts/scenes — the label
  registry itself is websocket-only — so the row fills up after a fetch
  (picker or "Update metadata") and cycles All ↔ the stored label until then
- The 32-shortcut cap now applies after filtering, so untagged entities can
  no longer crowd tagged ones out of the list
- Labels longer than 23 characters are not offered in the cycle: the row
  would have to clip the name, and a clipped name matches nothing, so the
  picker would empty with no visible reason
- A stored label that HA later renames or deletes stays on the row for one
  press and then cycles to All, instead of jumping to an arbitrary label
- While a filter is active, kept-but-untagged shortcuts are neither re-added
  to the picker as red rows nor marked missing (main screen included); clear
  the filter to manage them

## 0.6.1

Touch by default, with the HeadeRSS pull-down pattern and a clean toggle.

- Touch is ON by default; the phone-settings toggle is now a bare switch
  (no firmware-bug caveat text)
- Pull down on the main menu to open the sub-menu, exactly like HeadeRSS:
  the pull only arms when the menu content is at its very top (real scroll
  offset — the touch bridge scrolls content without moving the selection)
  and the finger starts in the top band; the 3-dot row inverts to the
  accent fill as the armed cue; dragging back up cancels; release-while-
  armed opens the sub-menu
- Shortcuts trigger on a single tap as before; the UP + DOWN confirmation
  dialog stays button-only (touch can never confirm an execution)
- Rubber-band sheet feedback during the pull; snap-back on cancel

## 0.6.0

Safer confirmations and haptic feedback.

- Confirmation screen: shortcut icon above the prompt; confirm by pressing
  UP + DOWN together and releasing (BACK cancels)
- Watch vibrates on every shortcut launch and on every error/timeout
- New "Vibrations" sub-menu setting (ON/OFF) silences every haptic

## 0.5.0

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
