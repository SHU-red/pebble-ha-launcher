# pebble-ha-launcher

A modern, touch-first Home Assistant script launcher for Pebble.

- Settings-menu-style shortcut list with touch navigation (swipe to scroll, tap to run)
- Each shortcut row: bold name with an area / tags / icon line underneath to
  tell same-named scripts apart; a Change Order screen reorders them (the
  order is never touched by metadata updates)
- Edit screen fetches available HA scripts (with icons, areas and labels) only when opened
- Optional confirm-before-execute prompt, toggleable from the phone settings page
- Errors are surfaced in the app (no silent failures)

## Setup

1. Install the app on your watch.
2. In the Pebble app -> HA Launcher settings: enter your Home Assistant URL
   (e.g. `http://192.168.178.55:8123`), your long-lived access token, and choose
   whether every execution should ask for confirmation. The watch flashes
   "Settings saved" and stores the config durably in its own flash — the
   phone's storage is only a prefill cache. The app pulls the config back from
   the watch on every start.
4. On the watch: "Settings" (via the 3-dot entry row) holds the Automatic
   close option — the app returns to the watchface after Never/3s/5s/10s/15s/30s
   of idle time on the main screen; SELECT cycles the choices.
3. On the watch: open the app, select "Edit shortcuts" (top entry). Every
   script row shows its state — grey `OFF` (not in the launcher), green `ON`
   (runs directly), orange `CONFIRM` (asks before running). Press Select to
   cycle the state; then run the picked scripts from the main list.

## Build

Requires the Pebble SDK (v4.17+ for touch support):

```bash
pebble build
```

## License

MIT. Icon glyphs from Material Design Icons (https://materialdesignicons.com),
Apache License 2.0. This project is an independent, original implementation; it
is not derived from other Home Assistant Pebble apps.
