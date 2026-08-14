# pebble-ha-launcher

One-tap Home Assistant scripts from your wrist. Stored shortcuts launch
instantly; setup auto-fetches your scripts from HA. Per-shortcut confirmation
prevents accidental triggers; touch-ready; auto-close for a fast-launch style.
Scripts do whatever HA can. Inspired by
[home-assistant-shortcuts](https://github.com/Carles-Figuerola/home-assistant-shortcuts),
rebuilt for the newest SDKs. Built with AI, maintained with love.

![Main menu](resources/store/main.png)
![Shortcut picker with OFF/ON/CONFIRM](resources/store/settings_confirm.png)
![Settings menu](resources/store/settings.png)

## Features

- **Fast by design** — your shortcuts live on the watch; launching sends one
  request to HA, no browsing, no config round-trips
- **Easy setup** — the picker fetches every available HA script (name, area,
  tags, icon) automatically; pick the ones you use
- **Per-shortcut confirmation** — each script is OFF, ON (runs directly) or
  CONFIRM (asks before running); grey / green / orange states in the picker
- **Touch-ready** — native touch navigation, opt-in from the phone settings
  (off by default until the firmware touch bugs are fixed)
- **Auto-close** — set Never/3s/5s/10s/15s/30s on the watch; idle time on the
  main screen returns you to the watchface, quick-launch style
- **Anything HA can do** — scripts can orchestrate lights, locks, media,
  notifications, integrations… one tap away

## Setup

1. Install the app on your watch.
2. In the Pebble app -> HA Launcher settings: enter your Home Assistant URL
   (e.g. `http://192.168.178.55:8123`) and your long-lived access token. The
   watch flashes "Settings saved" and stores the config durably in its own
   flash — the phone's storage is only a prefill cache.
3. On the watch: open the app, select "Edit shortcuts" (top entry). Every
   script row shows its state — grey `OFF` (not in the launcher), green `ON`
   (runs directly), orange `CONFIRM` (asks before running). Press Select to
   cycle the state, then run the picked scripts from the main list. The 3-dot
   entry row also holds Change Order, Update metadata and Automatic close
   (SELECT cycles Never/3s/5s/10s/15s/30s). Missing scripts stay listed in
   the picker with a red `!` so you can turn them off.

## Build

Requires the Pebble SDK (v4.17+ for touch support):

```bash
pebble build
```

## Credits

- Inspired by [home-assistant-shortcuts](https://github.com/Carles-Figuerola/home-assistant-shortcuts)
- Icon glyphs from Material Design Icons (https://materialdesignicons.com),
  Apache License 2.0
- **Fair notice: built with AI, maintained with love** — this is an
  independent, original implementation, not derived from other Home Assistant
  Pebble apps

## License

MIT.
