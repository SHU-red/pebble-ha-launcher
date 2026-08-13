# pebble-ha-launcher

A modern, touch-first Home Assistant script launcher for Pebble.

- Settings-menu-style shortcut list with touch navigation (swipe to scroll, tap to run)
- Edit screen fetches available HA scripts (with icons, areas and labels) only when opened
- Optional confirm-before-execute prompt, toggleable from the phone settings page
- Errors are surfaced in the app (no silent failures)

## Setup

1. Install the app on your watch.
2. In the Pebble app -> HA Launcher settings: enter your Home Assistant URL
   (e.g. `http://192.168.178.55:8123`), your long-lived access token, and choose
   whether every execution should ask for confirmation.
   Saving is persistent by design: the config page writes your values into its
   own storage, the phone app copies them back, and the app recovers them on
   the next start — it works even if the settings screen closes without a live
   JS session (the watch then shows no flash, but the values still stick).
3. On the watch: open the app, select "Edit shortcuts" (top entry), pick the
   scripts you want, then run them from the main list.

## Build

Requires the Pebble SDK (v4.17+ for touch support):

```bash
pebble build
```

## License

MIT. Icon glyphs from Material Design Icons (https://materialdesignicons.com),
Apache License 2.0. This project is an independent, original implementation; it
is not derived from other Home Assistant Pebble apps.
