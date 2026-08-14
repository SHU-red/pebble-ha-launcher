# Changelog

## 0.3.0

Launch Home Assistant scripts from your wrist, one tap.

- Stored shortcuts -> instant launch, no browsing
- Picker auto-fetches all HA scripts (names, areas, labels, icons)
- Per-shortcut OFF / ON / CONFIRM; the confirm screen is a quick step —
  execution then runs from the main screen exactly like without it
- Missing scripts show a red `!`; SELECT deletes them right from the list,
  BACK keeps them (HA may be temporarily unreachable)
- Robust against renamed / duplicated / deleted scripts: ghosts and
  entities without a runnable service are never offered, stale entries
  surface as missing instead of failing with HTTP 400
- Icon clustering: 79 glyphs cover about half of Material Design Icons via
  concept clusters (all `garage-*` -> one garage, `bed-*` -> home, ...)
- Automatic close (Never/3s/5s/10s/15s/30s), set on the watch
- Touch-ready; dark/light themes + accent color
- Reorder that only commits when you drop; order never touched by updates
- Up to 32 shortcuts (comfortable for a watch; keeps the app lean)
- Fixed: CONFIRM flow freeze, invisible icons, missing labels/tags,
  exec-feedback text, 400-on-ghost-script execution
- Built with AI, maintained with love
