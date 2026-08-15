/**
 * HA Launcher — watchapp (C side).
 *
 * Touch-first script launcher for Home Assistant. Main window is a
 * Settings-menu-style MenuLayer (Actions / Shortcuts sections); an edit
 * window browses HA scripts via AppMessage and builds the shortcut list;
 * executing a shortcut runs the script through the phone JS with a
 * confirm-before-run prompt and colored result dialogs.
 */

#include <pebble.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define MAX_SHORTCUTS 32

#define PERSIST_KEY_COUNT 1          // int32: number of stored shortcuts
#define PERSIST_KEY_CONFIRM 2        // int32: 0/1 confirm-before-execute
#define PERSIST_KEY_BASEURL 3        // string: Home Assistant base URL
#define PERSIST_KEY_TOKEN 4          // string: long-lived access token
#define PERSIST_KEY_ACCENT 5         // int32: GColor8 argb value of the accent color
#define PERSIST_KEY_DARKMODE 6       // int32: 1 = dark, 0 = light
#define PERSIST_KEY_TOUCH 7          // int32: 1 = native touch navigation enabled
#define PERSIST_KEY_AUTOCLOSE 8      // int32: seconds after success before auto-close (0 = never)
#define PERSIST_KEY_SUBTITLE 9       // int32: main-screen info-line field mask (SUBTITLE_*)
#define PERSIST_KEY_SHORTCUT_BASE 100 // + i: shortcut structs

#define DEFAULT_ACCENT_HEX 0x0055AA  // GColorCobaltBlue (24-bit RGB)

// ---------------------------------------------------------------------------
// Main-screen info line: which fields the shortcut subtitle shows, in fixed
// order (symbol · area · tags · category). A lean bitmask, cycled in the
// sub-menu exactly like Automatic close; 0 hides the line entirely.
// ---------------------------------------------------------------------------

#define SUBTITLE_SYMBOL   0x01
#define SUBTITLE_AREA     0x02
#define SUBTITLE_TAGS     0x04
#define SUBTITLE_CATEGORY 0x08

#define SUBTITLE_NONE 0x00
#define SUBTITLE_FULL (SUBTITLE_SYMBOL | SUBTITLE_AREA | SUBTITLE_TAGS | SUBTITLE_CATEGORY)

static const uint8_t SUBTITLE_PRESETS[] = {
  SUBTITLE_NONE,                                           // none
  SUBTITLE_FULL,                                           // symbol · area · tags · category
  SUBTITLE_SYMBOL | SUBTITLE_AREA | SUBTITLE_TAGS,         // symbol · area · tags
  SUBTITLE_SYMBOL | SUBTITLE_AREA,                         // symbol · area
  SUBTITLE_AREA | SUBTITLE_TAGS | SUBTITLE_CATEGORY,       // area · tags · category
  SUBTITLE_SYMBOL | SUBTITLE_TAGS | SUBTITLE_CATEGORY,     // symbol · tags · category
};
#define SUBTITLE_PRESET_COUNT ((uint8_t)(sizeof(SUBTITLE_PRESETS) / sizeof(SUBTITLE_PRESETS[0])))

static bool subtitle_preset_valid(uint8_t fields) {
  for (uint8_t i = 0; i < SUBTITLE_PRESET_COUNT; i++) {
    if (SUBTITLE_PRESETS[i] == fields) {
      return true;
    }
  }
  return false;
}

#define REQUEST_TIMEOUT_MS 10000     // execute request timeout
#define RESULT_DISMISS_MS 1500       // final result auto-dismiss
#define PULSE_INTERVAL_MS 250        // "Sending..." animated ellipsis

// ---------------------------------------------------------------------------
// Icon table: index == pebble.resources.media order in package.json.
// Index 0 is the generic icon (ICON_SCRIPT_TEXT); unknown/empty -> 0.
// ---------------------------------------------------------------------------

static const uint32_t ICONS[] = {
  RESOURCE_ID_ICON_SCRIPT_TEXT,
  RESOURCE_ID_ICON_LIGHTBULB,
  RESOURCE_ID_ICON_LIGHTBULB_ON,
  RESOURCE_ID_ICON_LOCK,
  RESOURCE_ID_ICON_LOCK_OPEN,
  RESOURCE_ID_ICON_HOME,
  RESOURCE_ID_ICON_POWER,
  RESOURCE_ID_ICON_PLAY,
  RESOURCE_ID_ICON_PAUSE,
  RESOURCE_ID_ICON_STOP,
  RESOURCE_ID_ICON_BELL,
  RESOURCE_ID_ICON_BELL_RING,
  RESOURCE_ID_ICON_ALARM,
  RESOURCE_ID_ICON_TIMER,
  RESOURCE_ID_ICON_CLOCK,
  RESOURCE_ID_ICON_CALENDAR,
  RESOURCE_ID_ICON_CALENDAR_CHECK,
  RESOURCE_ID_ICON_DOOR,
  RESOURCE_ID_ICON_GARAGE,
  RESOURCE_ID_ICON_GARAGE_OPEN,
  RESOURCE_ID_ICON_CAMERA,
  RESOURCE_ID_ICON_VIDEO,
  RESOURCE_ID_ICON_MOTION_SENSOR,
  RESOURCE_ID_ICON_THERMOMETER,
  RESOURCE_ID_ICON_WEATHER_SUNNY,
  RESOURCE_ID_ICON_WEATHER_NIGHT,
  RESOURCE_ID_ICON_WATER,
  RESOURCE_ID_ICON_WATER_OUTLINE,
  RESOURCE_ID_ICON_FIRE,
  RESOURCE_ID_ICON_FAN,
  RESOURCE_ID_ICON_AIR_CONDITIONER,
  RESOURCE_ID_ICON_RADIATOR,
  RESOURCE_ID_ICON_SPEAKER,
  RESOURCE_ID_ICON_TELEVISION,
  RESOURCE_ID_ICON_VOLUME_HIGH,
  RESOURCE_ID_ICON_MUSIC_NOTE,
  RESOURCE_ID_ICON_PHONE,
  RESOURCE_ID_ICON_MESSAGE,
  RESOURCE_ID_ICON_EMAIL,
  RESOURCE_ID_ICON_ACCOUNT,
  RESOURCE_ID_ICON_KEY,
  RESOURCE_ID_ICON_SHIELD,
  RESOURCE_ID_ICON_SHIELD_CHECK,
  RESOURCE_ID_ICON_EYE,
  RESOURCE_ID_ICON_CHECK,
  RESOURCE_ID_ICON_CLOSE,
  RESOURCE_ID_ICON_PLUS,
  RESOURCE_ID_ICON_MINUS,
  RESOURCE_ID_ICON_STAR,
  RESOURCE_ID_ICON_HEART,
  RESOURCE_ID_ICON_LEAF,
  RESOURCE_ID_ICON_CAR,
  RESOURCE_ID_ICON_LIGHT_SWITCH,
  RESOURCE_ID_ICON_POWER_PLUG,
  RESOURCE_ID_ICON_REMOTE,
  RESOURCE_ID_ICON_BLUETOOTH,
  RESOURCE_ID_ICON_WIFI,
  RESOURCE_ID_ICON_CLOUD,
  RESOURCE_ID_ICON_REFRESH,
  RESOURCE_ID_ICON_COG,
  RESOURCE_ID_ICON_ALERT,
  RESOURCE_ID_ICON_INFORMATION,
  RESOURCE_ID_ICON_FLASH,
  RESOURCE_ID_ICON_PIN,
  RESOURCE_ID_ICON_MAP_MARKER,
  RESOURCE_ID_ICON_ROBOT,
  RESOURCE_ID_ICON_LAMP,
  RESOURCE_ID_ICON_WINDOW_CLOSED,
  RESOURCE_ID_ICON_BLINDS,
  RESOURCE_ID_ICON_WASHING_MACHINE,
  RESOURCE_ID_ICON_FRIDGE,
  RESOURCE_ID_ICON_COFFEE,
  RESOURCE_ID_ICON_DOORBELL,
  RESOURCE_ID_ICON_CCTV,
  RESOURCE_ID_ICON_ALARM_LIGHT_OUTLINE,
  RESOURCE_ID_ICON_SMOKE_DETECTOR,
  RESOURCE_ID_ICON_SOLAR_POWER,
  RESOURCE_ID_ICON_BANK,
  RESOURCE_ID_ICON_CURRENCY_EUR,
};

// White-glyph variants of the same table, used on dark backgrounds. The
// 1-bit decode of grayscale PNGs paints white with GCompOpSet; these RGBA
// glyphs composite their own color, so dark rows pick the white variant.
static const uint32_t ICONS_WHITE[] = {
  RESOURCE_ID_ICON_SCRIPT_TEXT_WHITE,
  RESOURCE_ID_ICON_LIGHTBULB_WHITE,
  RESOURCE_ID_ICON_LIGHTBULB_ON_WHITE,
  RESOURCE_ID_ICON_LOCK_WHITE,
  RESOURCE_ID_ICON_LOCK_OPEN_WHITE,
  RESOURCE_ID_ICON_HOME_WHITE,
  RESOURCE_ID_ICON_POWER_WHITE,
  RESOURCE_ID_ICON_PLAY_WHITE,
  RESOURCE_ID_ICON_PAUSE_WHITE,
  RESOURCE_ID_ICON_STOP_WHITE,
  RESOURCE_ID_ICON_BELL_WHITE,
  RESOURCE_ID_ICON_BELL_RING_WHITE,
  RESOURCE_ID_ICON_ALARM_WHITE,
  RESOURCE_ID_ICON_TIMER_WHITE,
  RESOURCE_ID_ICON_CLOCK_WHITE,
  RESOURCE_ID_ICON_CALENDAR_WHITE,
  RESOURCE_ID_ICON_CALENDAR_CHECK_WHITE,
  RESOURCE_ID_ICON_DOOR_WHITE,
  RESOURCE_ID_ICON_GARAGE_WHITE,
  RESOURCE_ID_ICON_GARAGE_OPEN_WHITE,
  RESOURCE_ID_ICON_CAMERA_WHITE,
  RESOURCE_ID_ICON_VIDEO_WHITE,
  RESOURCE_ID_ICON_MOTION_SENSOR_WHITE,
  RESOURCE_ID_ICON_THERMOMETER_WHITE,
  RESOURCE_ID_ICON_WEATHER_SUNNY_WHITE,
  RESOURCE_ID_ICON_WEATHER_NIGHT_WHITE,
  RESOURCE_ID_ICON_WATER_WHITE,
  RESOURCE_ID_ICON_WATER_OUTLINE_WHITE,
  RESOURCE_ID_ICON_FIRE_WHITE,
  RESOURCE_ID_ICON_FAN_WHITE,
  RESOURCE_ID_ICON_AIR_CONDITIONER_WHITE,
  RESOURCE_ID_ICON_RADIATOR_WHITE,
  RESOURCE_ID_ICON_SPEAKER_WHITE,
  RESOURCE_ID_ICON_TELEVISION_WHITE,
  RESOURCE_ID_ICON_VOLUME_HIGH_WHITE,
  RESOURCE_ID_ICON_MUSIC_NOTE_WHITE,
  RESOURCE_ID_ICON_PHONE_WHITE,
  RESOURCE_ID_ICON_MESSAGE_WHITE,
  RESOURCE_ID_ICON_EMAIL_WHITE,
  RESOURCE_ID_ICON_ACCOUNT_WHITE,
  RESOURCE_ID_ICON_KEY_WHITE,
  RESOURCE_ID_ICON_SHIELD_WHITE,
  RESOURCE_ID_ICON_SHIELD_CHECK_WHITE,
  RESOURCE_ID_ICON_EYE_WHITE,
  RESOURCE_ID_ICON_CHECK_WHITE,
  RESOURCE_ID_ICON_CLOSE_WHITE,
  RESOURCE_ID_ICON_PLUS_WHITE,
  RESOURCE_ID_ICON_MINUS_WHITE,
  RESOURCE_ID_ICON_STAR_WHITE,
  RESOURCE_ID_ICON_HEART_WHITE,
  RESOURCE_ID_ICON_LEAF_WHITE,
  RESOURCE_ID_ICON_CAR_WHITE,
  RESOURCE_ID_ICON_LIGHT_SWITCH_WHITE,
  RESOURCE_ID_ICON_POWER_PLUG_WHITE,
  RESOURCE_ID_ICON_REMOTE_WHITE,
  RESOURCE_ID_ICON_BLUETOOTH_WHITE,
  RESOURCE_ID_ICON_WIFI_WHITE,
  RESOURCE_ID_ICON_CLOUD_WHITE,
  RESOURCE_ID_ICON_REFRESH_WHITE,
  RESOURCE_ID_ICON_COG_WHITE,
  RESOURCE_ID_ICON_ALERT_WHITE,
  RESOURCE_ID_ICON_INFORMATION_WHITE,
  RESOURCE_ID_ICON_FLASH_WHITE,
  RESOURCE_ID_ICON_PIN_WHITE,
  RESOURCE_ID_ICON_MAP_MARKER_WHITE,
  RESOURCE_ID_ICON_ROBOT_WHITE,
  RESOURCE_ID_ICON_LAMP_WHITE,
  RESOURCE_ID_ICON_WINDOW_CLOSED_WHITE,
  RESOURCE_ID_ICON_BLINDS_WHITE,
  RESOURCE_ID_ICON_WASHING_MACHINE_WHITE,
  RESOURCE_ID_ICON_FRIDGE_WHITE,
  RESOURCE_ID_ICON_COFFEE_WHITE,
  RESOURCE_ID_ICON_DOORBELL_WHITE,
  RESOURCE_ID_ICON_CCTV_WHITE,
  RESOURCE_ID_ICON_ALARM_LIGHT_OUTLINE_WHITE,
  RESOURCE_ID_ICON_SMOKE_DETECTOR_WHITE,
  RESOURCE_ID_ICON_SOLAR_POWER_WHITE,
  RESOURCE_ID_ICON_BANK_WHITE,
  RESOURCE_ID_ICON_CURRENCY_EUR_WHITE,
};

#define ICON_COUNT ((uint8_t)(sizeof(ICONS) / sizeof(ICONS[0])))

//! Map a curated icon index to a resource id; out-of-range clamps to index 0.
static uint32_t icon_resource(uint8_t idx) {
  if (idx >= ICON_COUNT) {
    return ICONS[0];
  }
  return ICONS[idx];
}

//! Same, for the white-glyph variant set (dark backgrounds).
static uint32_t icon_resource_white(uint8_t idx) {
  if (idx >= ICON_COUNT) {
    return ICONS_WHITE[0];
  }
  return ICONS_WHITE[idx];
}

// ---------------------------------------------------------------------------
// Type symbols: '$' (script, a plain ASCII glyph) and a filled play triangle
// (scene, drawn via GPath — U+25B6 is not covered by the system fonts).
// One shared path, repositioned with gpath_move_to per draw.
// ---------------------------------------------------------------------------

static const GPathInfo SCENE_TRI_PATH_INFO = {
  .num_points = 3,
  .points = (GPoint[]) { { 0, -4 }, { 0, 4 }, { 8, 0 } },
};

static GPath *s_scene_tri = NULL;

// ---------------------------------------------------------------------------
// Persisted shortcut storage
// ---------------------------------------------------------------------------

typedef struct {
  char key[64];
  uint8_t type;     // 0 = script, 1 = scene (HA domain of the shortcut)
  char name[48];
  char area[32];
  uint8_t icon_idx;
  uint8_t missing;  // 1 = entity no longer exists in Home Assistant
  uint8_t confirm;  // 1 = require approval before executing
  char labels[64];  // HA entity labels (tags), comma-joined
  char icon_name[40]; // mdi icon name without prefix (category line)
} Shortcut;

static Shortcut s_shortcuts[MAX_SHORTCUTS];
static uint16_t s_shortcut_count;
static bool s_confirm_enabled;
static char s_base_url[256];
static char s_token[256];
static GColor s_accent;
static uint8_t s_accent_argb;
static uint32_t s_accent_hex;
static bool s_dark_mode;
static bool s_touch_enabled;
static int32_t s_autoclose_seconds; // 0 = never close automatically
static uint8_t s_subtitle_fields = SUBTITLE_FULL; // main-screen info line mask

static AppTimer *s_autoclose_timer;

//! A shortcut's identity is (type, key): scene.pebble_test and
//! script.pebble_test coexist as distinct shortcuts.
static const char *TYPE_DOMAIN(uint8_t type) {
  return type ? "scene" : "script";
}

//! Write the full entity id ("script.<key>" / "scene.<key>") of a shortcut.
static void shortcut_entity_id(const Shortcut *sc, char *buf, size_t len) {
  snprintf(buf, len, "%s.%s", TYPE_DOMAIN(sc->type), sc->key);
}

static int32_t shortcut_index_for_type(uint8_t type, const char *key) {
  if (!key || !key[0]) {
    return -1;
  }
  for (uint16_t i = 0; i < s_shortcut_count; i++) {
    if (s_shortcuts[i].type == type && strcmp(s_shortcuts[i].key, key) == 0) {
      return (int32_t)i;
    }
  }
  return -1;
}

//! Look up a shortcut by its full entity id ("script.<key>" / "scene.<key>").
static int32_t shortcut_index_for_entity(const char *entity_id) {
  if (!entity_id || !entity_id[0]) {
    return -1;
  }
  const char *dot = strchr(entity_id, '.');
  if (!dot || dot == entity_id || !dot[1]) {
    return -1;
  }
  uint8_t type = (strncmp(entity_id, "scene.", 6) == 0) ? 1 : 0;
  return shortcut_index_for_type(type, dot + 1);
}

static void persist_save(void) {
  persist_write_int(PERSIST_KEY_COUNT, (int32_t)s_shortcut_count);
  for (uint16_t i = 0; i < s_shortcut_count; i++) {
    persist_write_data(PERSIST_KEY_SHORTCUT_BASE + i, &s_shortcuts[i], sizeof(Shortcut));
  }
  // Delete stale keys when the list shrank.
  for (uint16_t i = s_shortcut_count; i < MAX_SHORTCUTS; i++) {
    if (persist_exists(PERSIST_KEY_SHORTCUT_BASE + i)) {
      persist_delete(PERSIST_KEY_SHORTCUT_BASE + i);
    }
  }
}

static void persist_load(void) {
  int32_t count = persist_read_int(PERSIST_KEY_COUNT);
  if (count < 0 || count > MAX_SHORTCUTS) {
    count = 0;
  }
  s_shortcut_count = (uint16_t)count;
  for (uint16_t i = 0; i < s_shortcut_count; i++) {
    Shortcut *sc = &s_shortcuts[i];
    memset(sc, 0, sizeof(Shortcut)); // new fields (labels/icon_name) must be clean
    int n = persist_read_data(PERSIST_KEY_SHORTCUT_BASE + i, sc, sizeof(Shortcut));
    if (n < (int)sizeof(Shortcut)) {
      sc->type = 0;    // pre-scene blobs are all scripts
      sc->missing = 0; // pre-missing / missing-era blobs lack these bytes
      sc->confirm = 0;
    }
  }
  s_confirm_enabled = persist_read_int(PERSIST_KEY_CONFIRM) != 0;

  persist_read_string(PERSIST_KEY_BASEURL, s_base_url, sizeof(s_base_url));
  persist_read_string(PERSIST_KEY_TOKEN, s_token, sizeof(s_token));

  s_accent_hex = (uint32_t)persist_read_int(PERSIST_KEY_ACCENT);
  if (s_accent_hex <= 255) {
    // 0 = unset; <=255 = the legacy broken GColor8-byte value -> use the default.
    s_accent_hex = DEFAULT_ACCENT_HEX;
  }
  s_accent = GColorFromHEX(s_accent_hex);
  s_accent_argb = s_accent.argb;
  s_dark_mode = persist_read_int(PERSIST_KEY_DARKMODE) != 0;
  s_touch_enabled = persist_read_int(PERSIST_KEY_TOUCH) != 0;
  s_autoclose_seconds = persist_read_int(PERSIST_KEY_AUTOCLOSE);
  // Whitelist: only the settings-page choices are valid; anything else = never.
  if (s_autoclose_seconds != 3 && s_autoclose_seconds != 5 && s_autoclose_seconds != 10 &&
      s_autoclose_seconds != 15 && s_autoclose_seconds != 30) {
    s_autoclose_seconds = 0;
  }
  s_subtitle_fields = (uint8_t)persist_read_int(PERSIST_KEY_SUBTITLE);
  if (!subtitle_preset_valid(s_subtitle_fields)) {
    s_subtitle_fields = SUBTITLE_FULL; // unset/corrupt -> full line
  }
}

static void persist_save_config(const char *base_url, const char *token, bool confirm,
                                uint32_t accent_hex, bool dark_mode, bool touch_enabled,
                                int32_t autoclose_seconds) {
  if (base_url) {
    strncpy(s_base_url, base_url, sizeof(s_base_url) - 1);
    s_base_url[sizeof(s_base_url) - 1] = '\0';
    persist_write_string(PERSIST_KEY_BASEURL, s_base_url);
  }
  if (token) {
    strncpy(s_token, token, sizeof(s_token) - 1);
    s_token[sizeof(s_token) - 1] = '\0';
    persist_write_string(PERSIST_KEY_TOKEN, s_token);
  }
  s_confirm_enabled = confirm;
  persist_write_int(PERSIST_KEY_CONFIRM, s_confirm_enabled ? 1 : 0);
  if (accent_hex != 0) {
    s_accent_hex = accent_hex;
    s_accent = GColorFromHEX(s_accent_hex);
    s_accent_argb = s_accent.argb;
    persist_write_int(PERSIST_KEY_ACCENT, (int32_t)s_accent_hex);
  }
  s_dark_mode = dark_mode;
  persist_write_int(PERSIST_KEY_DARKMODE, s_dark_mode ? 1 : 0);
  s_touch_enabled = touch_enabled;
  persist_write_int(PERSIST_KEY_TOUCH, s_touch_enabled ? 1 : 0);
  if (autoclose_seconds != 3 && autoclose_seconds != 5 && autoclose_seconds != 10 &&
      autoclose_seconds != 15 && autoclose_seconds != 30) {
    autoclose_seconds = 0;
  }
  s_autoclose_seconds = autoclose_seconds;
  persist_write_int(PERSIST_KEY_AUTOCLOSE, s_autoclose_seconds);
}

// ---------------------------------------------------------------------------
// Result dialog (built from scratch: colored background + centered text +
// slide-in + animated ellipsis while working; auto-dismiss for finals)
// ---------------------------------------------------------------------------

static Window *s_dialog_window;
static Layer *s_dialog_bg;
static TextLayer *s_dialog_text;
static AppTimer *s_timeout_timer;
static AppTimer *s_dismiss_timer;
static AppTimer *s_pulse_timer;
static bool s_dialog_active;
static bool s_dialog_confirm;
static bool s_dialog_delete;
static char s_dialog_text_buf[128];
static uint8_t s_pulse_phase;
static GColor s_dialog_color;

static void start_execute(const char *entity_id);

typedef struct {
  char entity_id[72]; // full entity id ("script.<key>" / "scene.<key>")
  char name[48];
} ConfirmCtx;

static ConfirmCtx s_confirm_ctx;

// Row-level execution feedback: the executing shortcut's row turns green
// ('LAUNCHED') while sending and after success, red ('FAILED') on error.
static MenuLayer *s_main_menu;

typedef enum {
  EXEC_NONE = 0,
  EXEC_LAUNCHING = 1,
  EXEC_DONE = 2,
  EXEC_FAILED = 3,
} ExecState;

static int32_t s_exec_row = -1;
static ExecState s_exec_state = EXEC_NONE;
static char s_exec_error[32];
static AppTimer *s_exec_revert_timer;

#define EXEC_REVERT_DONE_MS 1200
#define EXEC_REVERT_FAILED_MS 2200

static void arm_exec_revert(void);
static void exec_revert_cb(void *data) {
  s_exec_revert_timer = NULL;
  s_exec_row = -1;
  s_exec_state = EXEC_NONE;
  menu_layer_reload_data(s_main_menu);
}

static void arm_exec_revert(void) {
  if (s_exec_revert_timer) { app_timer_cancel(s_exec_revert_timer); s_exec_revert_timer = NULL; }
  s_exec_revert_timer = app_timer_register(
      s_exec_state == EXEC_FAILED ? EXEC_REVERT_FAILED_MS : EXEC_REVERT_DONE_MS,
      exec_revert_cb, NULL);
}

static void clear_exec_overlay(void) {
  if (s_exec_revert_timer) { app_timer_cancel(s_exec_revert_timer); s_exec_revert_timer = NULL; }
  s_exec_row = -1;
  s_exec_state = EXEC_NONE;
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Automatic close: an idle timeout on the main screen. Armed when the main
// window appears and reset on every interaction; when it fires the app
// returns to the watchface. Never fires while an execution overlay is up or
// while the user is in any other window (those cancel the timer).
// ---------------------------------------------------------------------------

static void cancel_autoclose(void) {
  if (s_autoclose_timer) {
    app_timer_cancel(s_autoclose_timer);
    s_autoclose_timer = NULL;
  }
}

static void cancel_autoclose(void);
static void arm_autoclose(void);

static void autoclose_cb(void *data) {
  s_autoclose_timer = NULL;
  if (s_exec_row >= 0 && s_exec_state != EXEC_NONE) {
    // An execution is still in flight: re-arm instead of closing.
    arm_autoclose();
    return;
  }
  // Pop every window: the watchface reappears, i.e. the app "closes".
  window_stack_pop_all(true);
}

static void arm_autoclose(void) {
  cancel_autoclose();
  if (s_autoclose_seconds > 0) {
    s_autoclose_timer = app_timer_register((uint32_t)s_autoclose_seconds * 1000,
                                           autoclose_cb, NULL);
  }
}

static MenuLayer *s_main_menu;

static GColor theme_bg(void);
static GColor theme_fg(void);
static GColor theme_muted(void);
static void apply_theme(void);

//! Fill the dialog background with the current color.
static void dialog_bg_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, s_dialog_color);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void dialog_show_final(bool success, const char *text);
static void dialog_unload(Window *window);
static void dialog_dismiss_cb(void *data);
static void delete_shortcut_idx(int32_t idx);

static void dialog_confirm_select(ClickRecognizerRef rec, void *ctx) {
  if (s_dialog_delete) {
    // Missing-in-HA prompt: SELECT deletes the shortcut.
    dialog_dismiss_cb(NULL);
    delete_shortcut_idx(shortcut_index_for_entity(s_confirm_ctx.entity_id));
    return;
  }
  if (!s_dialog_confirm) return;
  s_dialog_confirm = false;
  // The confirm screen is only an intermediate step: dismiss it and run
  // the standard execute flow so the main menu shows the exec overlay
  // exactly as when executing without confirmation.
  dialog_dismiss_cb(NULL);
  start_execute(s_confirm_ctx.entity_id);
}

static void dialog_confirm_cancel(ClickRecognizerRef rec, void *ctx) {
  if (s_dialog_delete) {
    // Missing-in-HA prompt: BACK keeps the shortcut (HA may be
    // temporarily unreachable or the script only momentarily gone).
    dialog_dismiss_cb(NULL);
    return;
  }
  if (!s_dialog_confirm) return;
  s_dialog_confirm = false;
  dialog_dismiss_cb(NULL);
}

static void dialog_click_config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, dialog_confirm_select);
  window_single_click_subscribe(BUTTON_ID_BACK, dialog_confirm_cancel);
}

static void dialog_dismiss_cb(void *data) {
  s_dismiss_timer = NULL;
  // Pop only the dialog: the app stays alive (exiting mid-AppMessage-stream
  // crashed on use-after-free of windows/animation objects).
  if (s_dialog_window) {
    window_stack_remove(s_dialog_window, true);
  }
}

static void request_timeout_cb(void *data) {
  s_timeout_timer = NULL;
  if (s_exec_row >= 0) {
    s_exec_state = EXEC_FAILED;
    snprintf(s_exec_error, sizeof(s_exec_error), "Timeout");
    arm_exec_revert();
    menu_layer_reload_data(s_main_menu);
  }
}

static void pulse_tick_cb(void *data) {
  static const char *phases[] = {
    "Sending", "Sending.", "Sending..", "Sending...",
  };
  s_pulse_phase = (uint8_t)((s_pulse_phase + 1) % 4);
  text_layer_set_text(s_dialog_text, phases[s_pulse_phase]);
}

static void dialog_cancel_timers(void) {
  if (s_timeout_timer) {
    app_timer_cancel(s_timeout_timer);
    s_timeout_timer = NULL;
  }
  if (s_dismiss_timer) {
    app_timer_cancel(s_dismiss_timer);
    s_dismiss_timer = NULL;
  }
  if (s_pulse_timer) {
    app_timer_cancel(s_pulse_timer);
    s_pulse_timer = NULL;
  }
}

static void dialog_create(void) {
  if (s_dialog_active) {
    return;
  }
  s_dialog_window = window_create();
  window_set_window_handlers(s_dialog_window, (WindowHandlers){
    .unload = dialog_unload,
  });
  window_set_click_config_provider(s_dialog_window, dialog_click_config_provider);

  Layer *root = window_get_root_layer(s_dialog_window);
  GRect bounds = layer_get_bounds(root);

  s_dialog_bg = layer_create(bounds);
  layer_set_update_proc(s_dialog_bg, dialog_bg_update_proc);
  s_dialog_color = GColorGreen;
  layer_add_child(root, s_dialog_bg);

  s_dialog_text = text_layer_create(GRect(8, (bounds.size.h - 100) / 2, bounds.size.w - 16, 100));
  text_layer_set_font(s_dialog_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_dialog_text, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_dialog_text, GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_dialog_text, GColorClear);
  text_layer_set_text_color(s_dialog_text, GColorWhite);
  layer_add_child(s_dialog_bg, text_layer_get_layer(s_dialog_text));

  s_dialog_active = true;
  window_stack_push(s_dialog_window, false);
}

//! Orange approval screen: one more SELECT confirms, BACK cancels.
//! Shared dialog setup: cancel timers, reset the interaction modes, paint
//! the background color, show white text and pulse. The caller sets its
//! mode flag and any extra timers afterwards.
static void dialog_prepare(GColor color, const char *text) {
  dialog_cancel_timers();
  s_dialog_confirm = false;
  s_dialog_delete = false;
  s_dialog_color = color;
  layer_mark_dirty(s_dialog_bg);
  text_layer_set_text_color(s_dialog_text, GColorWhite);
  text_layer_set_text(s_dialog_text, text);
  vibes_short_pulse();
}

//! Remember the shortcut the dialog is about (name for the prompt, key for
//! the action) — shared by the confirm and delete prompts.
static void dialog_capture(const Shortcut *sc) {
  // Precision-bound: the key fallback can exceed the name buffer.
  snprintf(s_confirm_ctx.name, sizeof(s_confirm_ctx.name), "%.*s",
           (int)sizeof(s_confirm_ctx.name) - 1, sc->name[0] ? sc->name : sc->key);
  shortcut_entity_id(sc, s_confirm_ctx.entity_id, sizeof(s_confirm_ctx.entity_id));
}

static void dialog_show_confirm(const Shortcut *sc) {
  if (!s_dialog_active) {
    dialog_create();
  }
  dialog_capture(sc);
  snprintf(s_dialog_text_buf, sizeof(s_dialog_text_buf),
           "Run %s?\n\nSELECT: confirm\nBACK: cancel", s_confirm_ctx.name);
  dialog_prepare(GColorOrange, s_dialog_text_buf);
  s_dialog_confirm = true;
}

//! Red prompt for a shortcut marked missing in Home Assistant: SELECT
//! deletes it from the launcher, BACK keeps it (HA may be temporarily
//! unreachable or the script only momentarily gone).
static void dialog_show_delete(const Shortcut *sc) {
  if (!s_dialog_active) {
    dialog_create();
  }
  dialog_capture(sc);
  snprintf(s_dialog_text_buf, sizeof(s_dialog_text_buf),
           "Delete %s?\n\nSELECT: delete\nBACK: keep", s_confirm_ctx.name);
  dialog_prepare(GColorRed, s_dialog_text_buf);
  s_dialog_delete = true;
}

//! Green working dialog (auto_dismiss = false, animated ellipsis).
static void dialog_show_working(const char *text) {
  if (!s_dialog_active) {
    dialog_create();
  }
  dialog_prepare(GColorGreen, text);
  s_pulse_phase = 3;
  s_pulse_timer = app_timer_register(PULSE_INTERVAL_MS, pulse_tick_cb, NULL);
}

//! Green (success) or red (failure) final dialog, auto-dismissed after 1.5s.
static void dialog_show_final(bool success, const char *text) {
  if (!s_dialog_active) {
    return;
  }
  dialog_prepare(success ? GColorGreen : GColorRed, text);
  if (!success) {
    vibes_double_pulse();
  }
  s_dismiss_timer = app_timer_register(RESULT_DISMISS_MS, dialog_dismiss_cb, NULL);
}

static void dialog_unload(Window *window) {
  dialog_cancel_timers();
  text_layer_destroy(s_dialog_text);
  layer_destroy(s_dialog_bg);
  window_destroy(s_dialog_window);
  s_dialog_window = NULL;
  s_dialog_active = false;
}

// ---------------------------------------------------------------------------
// Execute flow
// ---------------------------------------------------------------------------

static void start_execute(const char *entity_id);

static void execute_shortcut(const Shortcut *sc) {
  if (sc->confirm) {
    dialog_show_confirm(sc);
  } else {
    char entity_id[72];
    shortcut_entity_id(sc, entity_id, sizeof(entity_id));
    start_execute(entity_id);
  }
}

//! Kick off an execution by full entity id. The type is derived from the
//! domain prefix and forwarded so the JS posts to the right turn_on service.
static void start_execute(const char *entity_id) {
  cancel_autoclose(); // a new execution supersedes any pending auto-close
  int32_t idx = shortcut_index_for_entity(entity_id);
  if (idx >= 0) {
    clear_exec_overlay();
    s_exec_row = 1 + (int32_t)idx;
    s_exec_state = EXEC_LAUNCHING;
    s_exec_error[0] = '\0';
    menu_layer_reload_data(s_main_menu);
  }
  const char *dot = strchr(entity_id, '.');
  const char *key = dot ? dot + 1 : entity_id;
  uint8_t type = (strncmp(entity_id, "scene.", 6) == 0) ? 1 : 0;
  DictionaryIterator *iter;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if (res == APP_MSG_OK) {
    dict_write_cstring(iter, MESSAGE_KEY_ScriptKey, key);
    dict_write_cstring(iter, MESSAGE_KEY_ScriptType, TYPE_DOMAIN(type));
    res = app_message_outbox_send();
  }
  if (res != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to send ScriptKey (%d)", (int)res);
    if (s_exec_row >= 0) {
      s_exec_state = EXEC_FAILED;
      menu_layer_reload_data(s_main_menu);
    }
    return;
  }
  s_timeout_timer = app_timer_register(REQUEST_TIMEOUT_MS, request_timeout_cb, NULL);
}

// ---------------------------------------------------------------------------
// Edit window (browse scripts on Home Assistant)
// ---------------------------------------------------------------------------

typedef struct {
  char key[64];
  uint8_t type;     // 0 = script, 1 = scene
  char name[48];
  char area[32];
  char labels[64];
  uint8_t icon_idx;
  char icon_name[40];
  uint8_t cycle;  // 0 = OFF, 1 = ON, 2 = CONFIRM (transient, from confirm flag)
  uint8_t missing; // 1 = entity no longer exists in Home Assistant
} ScriptEntry;

static ScriptEntry s_scripts[MAX_SHORTCUTS];
static uint16_t s_script_count;
static uint16_t s_script_expected;
static ScriptEntry s_pending;
static bool s_pending_active;

static Window *s_edit_window;
static MenuLayer *s_edit_menu;
static TextLayer *s_edit_status;
static bool s_edit_visible;

static void edit_render(void);
static void edit_fetch_done(void);
static void metadata_apply(void);
static bool s_edit_update_mode;

static void edit_begin_collect(int32_t count) {
  s_script_count = 0;
  s_script_expected = (uint16_t)(count < 0 ? 0 : (count > MAX_SHORTCUTS ? MAX_SHORTCUTS : count));
  memset(&s_pending, 0, sizeof(s_pending));
  s_pending_active = false;
}

static void edit_commit_pending(void) {
  if (s_script_count >= MAX_SHORTCUTS) {
    s_pending_active = false;
    memset(&s_pending, 0, sizeof(s_pending));
    return;
  }
  // Picked shortcuts show ON (runs directly) or CONFIRM (asks first); others OFF.
  int32_t idx = shortcut_index_for_type(s_pending.type, s_pending.key);
  s_pending.cycle = (idx >= 0) ? (s_shortcuts[idx].confirm ? 2 : 1) : 0;
  s_scripts[s_script_count++] = s_pending;
  memset(&s_pending, 0, sizeof(s_pending));
  s_pending_active = false;
}

//! Collect one script/scene entry from an incoming message. Entries arrive as
//! {ScriptName, ScriptKey, ScriptType, ScriptArea, ScriptLabels, ScriptIcon};
//! a new ScriptName commits the previous entry and starts the next one.
static void edit_collect_script(DictionaryIterator *iter) {
  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_ScriptName))) {
    if (s_pending_active) {
      edit_commit_pending();
    }
    s_pending_active = true;
    memset(&s_pending, 0, sizeof(s_pending));
    snprintf(s_pending.name, sizeof(s_pending.name), "%s", t->value->cstring);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ScriptKey))) {
    snprintf(s_pending.key, sizeof(s_pending.key), "%s", t->value->cstring);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ScriptType))) {
    s_pending.type = (strcmp(t->value->cstring, "scene") == 0) ? 1 : 0;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ScriptArea))) {
    snprintf(s_pending.area, sizeof(s_pending.area), "%s", t->value->cstring);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ScriptLabels))) {
    snprintf(s_pending.labels, sizeof(s_pending.labels), "%s", t->value->cstring);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ScriptIcon))) {
    s_pending.icon_idx = (uint8_t)t->value->int32;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ScriptIconName))) {
    snprintf(s_pending.icon_name, sizeof(s_pending.icon_name), "%s", t->value->cstring);
  }
  if (!s_pending_active || s_script_expected == 0) {
    return;
  }
  uint16_t have = (uint16_t)(s_script_count + 1);
  if (have >= s_script_expected) {
    edit_commit_pending();
    if (s_edit_update_mode) {
      metadata_apply();
    } else {
      edit_fetch_done();
    }
  }
}

//! Refresh stored shortcut metadata from the fetched list: names, areas,
//! labels, icon names, icons. Scripts absent from the fetch are marked
//! missing. Never adds/removes/reorders. Persists. Returns updated count.
static uint16_t refresh_shortcut_metadata(void) {
  for (uint16_t i = 0; i < s_shortcut_count; i++) {
    s_shortcuts[i].missing = 1;
  }
  uint16_t updated = 0;
  for (uint16_t j = 0; j < s_script_count; j++) {
    int32_t idx = shortcut_index_for_type(s_scripts[j].type, s_scripts[j].key);
    if (idx >= 0) {
      Shortcut *sc = &s_shortcuts[idx];
      snprintf(sc->name, sizeof(sc->name), "%s",
               s_scripts[j].name[0] ? s_scripts[j].name : s_scripts[j].key);
      snprintf(sc->area, sizeof(sc->area), "%s", s_scripts[j].area);
      snprintf(sc->labels, sizeof(sc->labels), "%s", s_scripts[j].labels);
      snprintf(sc->icon_name, sizeof(sc->icon_name), "%s", s_scripts[j].icon_name);
      sc->icon_idx = s_scripts[j].icon_idx;
      sc->missing = 0;
      updated++;
    }
  }
  persist_save();
  return updated;
}

//! Update-only pass (sub-menu "Update metadata"): refresh + result dialog.
static void metadata_apply(void) {
  uint16_t updated = refresh_shortcut_metadata();
  uint16_t missing = 0;
  for (uint16_t i = 0; i < s_shortcut_count; i++) {
    if (s_shortcuts[i].missing) {
      missing++;
    }
  }
  s_edit_update_mode = false;
  char msg[64];
  if (missing > 0) {
    snprintf(msg, sizeof(msg), "Updated %u, %u missing", updated, missing);
    dialog_show_final(false, msg);
  } else {
    snprintf(msg, sizeof(msg), "Updated %u shortcuts", updated);
    dialog_show_final(true, msg);
  }
}

static void edit_show_status(const char *text, GColor color) {
  if (!s_edit_status) {
    return;
  }
  text_layer_set_text_color(s_edit_status, color);
  text_layer_set_text(s_edit_status, text);
  layer_set_hidden(text_layer_get_layer(s_edit_status), false);
}

static void edit_hide_status(void) {
  if (s_edit_status) {
    layer_set_hidden(text_layer_get_layer(s_edit_status), true);
  }
}

static void edit_show_status_error(const char *text) {
  edit_show_status(text, GColorRed);
}

static void edit_render(void) {
  if (!s_edit_window) {
    return;
  }
  menu_layer_reload_data(s_edit_menu);
}

//! The picker fetch completed: refresh the stored shortcut metadata (same as
//! "Update metadata") and append shortcuts that no longer exist in HA, marked
//! missing, so the user can still see them and turn them off like any other.
static void edit_fetch_done(void) {
  if (!s_edit_window) {
    return;
  }
  edit_hide_status();
  refresh_shortcut_metadata();

  for (uint16_t i = 0; i < s_shortcut_count; i++) {
    if (!s_shortcuts[i].missing) {
      continue;
    }
    bool present = false;
    for (uint16_t j = 0; j < s_script_count; j++) {
      // Type matters: a scene with the same key does not resurrect a
      // missing script shortcut (and vice versa).
      if (s_scripts[j].type == s_shortcuts[i].type &&
          strcmp(s_scripts[j].key, s_shortcuts[i].key) == 0) {
        present = true;
        break;
      }
    }
    if (present || s_script_count >= MAX_SHORTCUTS) {
      continue;
    }
    ScriptEntry e;
    memset(&e, 0, sizeof(e));
    snprintf(e.key, sizeof(e.key), "%s", s_shortcuts[i].key);
    e.type = s_shortcuts[i].type;
    snprintf(e.name, sizeof(e.name), "%s",
             s_shortcuts[i].name[0] ? s_shortcuts[i].name : s_shortcuts[i].key);
    snprintf(e.area, sizeof(e.area), "%s", s_shortcuts[i].area);
    snprintf(e.labels, sizeof(e.labels), "%s", s_shortcuts[i].labels);
    snprintf(e.icon_name, sizeof(e.icon_name), "%s", s_shortcuts[i].icon_name);
    e.icon_idx = s_shortcuts[i].icon_idx;
    e.cycle = s_shortcuts[i].confirm ? 2 : 1; // still picked: show its stored mode
    e.missing = 1;
    s_scripts[s_script_count++] = e;
  }
  menu_layer_reload_data(s_edit_menu);
}

// ---- pick / unpick ----

// Remember where shortcuts were removed, so a re-pick (cycling through OFF)
// restores the previous launcher position instead of appending at the end —
// the user's set order must never change outside the Change Order screen.
// Keyed by full entity id so a scene and a script sharing a key name each
// restore their own slot.
static char s_removed_entities[8][72];
static int32_t s_removed_pos[8];
static uint8_t s_removed_next;

static int32_t removed_position(const char *entity_id) {
  for (uint8_t i = 0; i < 8; i++) {
    if (s_removed_entities[i][0] && strcmp(s_removed_entities[i], entity_id) == 0) {
      return s_removed_pos[i];
    }
  }
  return -1;
}

static void remember_removed(const char *entity_id, int32_t pos) {
  uint8_t slot = s_removed_next++ % 8;
  snprintf(s_removed_entities[slot], sizeof(s_removed_entities[slot]), "%s", entity_id);
  s_removed_pos[slot] = pos;
}

//! Remove a shortcut from the launcher (SELECT on a missing row): remember
//! its position for re-pick restoration, shift the rest down, persist.
static void delete_shortcut_idx(int32_t idx) {
  if (idx < 0 || idx >= s_shortcut_count) {
    return;
  }
  char entity_id[72];
  shortcut_entity_id(&s_shortcuts[idx], entity_id, sizeof(entity_id));
  remember_removed(entity_id, idx);
  memmove(&s_shortcuts[idx], &s_shortcuts[idx + 1],
          (size_t)(s_shortcut_count - (uint16_t)idx - 1) * sizeof(Shortcut));
  s_shortcut_count--;
  persist_save();
  menu_layer_reload_data(s_main_menu);
}

static void pick_script(uint16_t row, uint8_t confirm) {
  ScriptEntry *e = &s_scripts[row];
  if (!e->key[0] || shortcut_index_for_type(e->type, e->key) >= 0) {
    return;
  }
  if (s_shortcut_count >= MAX_SHORTCUTS) {
    edit_show_status("Max 32 shortcuts", GColorRed);
    return;
  }
  char entity_id[72];
  snprintf(entity_id, sizeof(entity_id), "%s.%s", TYPE_DOMAIN(e->type), e->key);
  Shortcut *sc;
  int32_t restore = removed_position(entity_id);
  if (restore >= 0) {
    if (restore > (int32_t)s_shortcut_count) {
      restore = s_shortcut_count;
    }
    memmove(&s_shortcuts[restore + 1], &s_shortcuts[restore],
            (size_t)(s_shortcut_count - (uint16_t)restore) * sizeof(Shortcut));
    sc = &s_shortcuts[restore];
    s_shortcut_count++;
  } else {
    sc = &s_shortcuts[s_shortcut_count++];
  }
  snprintf(sc->key, sizeof(sc->key), "%s", e->key);
  sc->type = e->type;
  snprintf(sc->name, sizeof(sc->name), "%s", e->name[0] ? e->name : e->key);
  snprintf(sc->area, sizeof(sc->area), "%s", e->area);
  snprintf(sc->labels, sizeof(sc->labels), "%s", e->labels);
  snprintf(sc->icon_name, sizeof(sc->icon_name), "%s", e->icon_name);
  sc->icon_idx = e->icon_idx;
  sc->missing = e->missing;
  sc->confirm = confirm;
  persist_save();
  edit_render();
}

static void unpick_script(uint16_t row) {
  ScriptEntry *e = &s_scripts[row];
  int32_t idx = shortcut_index_for_type(e->type, e->key);
  if (idx < 0) {
    return;
  }
  char entity_id[72];
  snprintf(entity_id, sizeof(entity_id), "%s.%s", TYPE_DOMAIN(e->type), e->key);
  remember_removed(entity_id, idx);
  memmove(&s_shortcuts[idx], &s_shortcuts[idx + 1],
          (size_t)(s_shortcut_count - (uint16_t)idx - 1) * sizeof(Shortcut));
  s_shortcut_count--;
  persist_save();
  edit_render();
}

// ---- menu callbacks ----

static uint16_t edit_get_num_sections(MenuLayer *menu_layer, void *callback_context) {
  return 1;
}

static uint16_t edit_get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                                  void *callback_context) {
  return s_script_count;
}

// Full-screen notification-style card per script (one card per screen).
static int16_t edit_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index,
                                    void *callback_context) {
  return layer_get_bounds(menu_layer_get_layer(menu_layer)).size.h;
}

// ---------------------------------------------------------------------------
// State bar: full-width color band at the select-button level. The state
// transition is cross-faded with the firmware's native Animation API
// (property_animation has no color property, so the 2-bit channels of the
// GColor8 palette are interpolated per frame — cheap, no float math).
// ---------------------------------------------------------------------------

static GColor cycle_color(uint8_t cycle) {
  return (cycle == 1) ? GColorGreen : (cycle == 2) ? GColorOrange : GColorDarkGray;
}

static GColor s_bar_color;   // currently drawn bar color
static uint16_t s_bar_row = 0xFFFF; // row it belongs to (cells are full-screen)
static Animation *s_bar_anim;
static GColor s_bar_from;
static GColor s_bar_to;

static GColor bar_color_lerp(GColor from, GColor to, uint32_t num, uint32_t den) {
  uint8_t fr = (from.argb >> 4) & 0x3, fg = (from.argb >> 2) & 0x3, fb = from.argb & 0x3;
  uint8_t tr = (to.argb >> 4) & 0x3, tg = (to.argb >> 2) & 0x3, tb = to.argb & 0x3;
  uint8_t r = (uint8_t)((fr * (den - num) + tr * num + den / 2) / den);
  uint8_t g = (uint8_t)((fg * (den - num) + tg * num + den / 2) / den);
  uint8_t b = (uint8_t)((fb * (den - num) + tb * num + den / 2) / den);
  return (GColor){ .argb = (uint8_t)(0xC0 | (r << 4) | (g << 2) | b) };
}

static void bar_anim_update(Animation *anim, const AnimationProgress progress) {
  s_bar_color = bar_color_lerp(s_bar_from, s_bar_to, progress, ANIMATION_NORMALIZED_MAX);
  layer_mark_dirty(menu_layer_get_layer(s_edit_menu));
}

static void bar_anim_stopped(Animation *anim, bool finished, void *context) {
  if (anim != s_bar_anim) {
    return; // superseded by a newer animation
  }
  s_bar_anim = NULL; // completed animations are freed by the system
  s_bar_color = s_bar_to;
  layer_mark_dirty(menu_layer_get_layer(s_edit_menu));
}

static const AnimationImplementation BAR_ANIM_IMPL = {
  .update = bar_anim_update,
};

static void bar_animate_to(GColor to) {
  if (s_bar_anim) {
    Animation *old = s_bar_anim;
    s_bar_anim = NULL; // the unscheduled old animation must not win
    animation_unschedule(old);
  }
  s_bar_from = s_bar_color;
  s_bar_to = to;
  s_bar_anim = animation_create();
  animation_set_duration(s_bar_anim, 220);
  animation_set_curve(s_bar_anim, AnimationCurveEaseInOut);
  animation_set_implementation(s_bar_anim, &BAR_ANIM_IMPL);
  animation_set_handlers(s_bar_anim, (AnimationHandlers){
    .stopped = bar_anim_stopped,
  }, NULL);
  animation_schedule(s_bar_anim);
}

//! Full-screen picker card in the style of the native Pebble notification
//! page: white body, colored top banner (32px) with the entity icon in it,
//! black info text. A full-width color band at the select-button level shows
//! the OFF/ON/CONFIRM state (grey/green/orange) and toggles on SELECT.
//! Four strictly separated regions: banner | top split (Type | Area) |
//! state band | bottom split (Tags | Category) as full-width label/value
//! rows — muted bold labels, black values, no fills; entity key as footer.
static void edit_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                          void *callback_context) {
  ScriptEntry *e = &s_scripts[cell_index->row];
  GRect bounds = layer_get_bounds(cell_layer);
  const int16_t banner_h = 32;
  const int16_t margin = 10;
  const int16_t bar_h = 34;
  const int16_t center = bounds.size.h / 2; // physical select-button level

  // White notification-style page.
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Accent banner: icon far left, script name right after it.
  GRect banner = GRect(0, 0, bounds.size.w, banner_h);
  graphics_context_set_fill_color(ctx, s_accent);
  graphics_fill_rect(ctx, banner, 0, GCornerNone);
  GBitmap *icon = gbitmap_create_with_resource(icon_resource_white(e->icon_idx));
  // White-glyph RGBA variant at native 32x32, composited over the accent
  // banner (GCompOpSet alpha-composites the glyph's own color).
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, icon, GRect(margin, 0, 32, 32));
  gbitmap_destroy(icon);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, e->name[0] ? e->name : e->key,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(margin + 36, 6, bounds.size.w - margin - 42, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Missing-upstream badge: red circle with '!', like the main screen.
  if (e->missing) {
    graphics_context_set_fill_color(ctx, GColorRed);
    graphics_fill_circle(ctx, GPoint(bounds.size.w - 16, 16), 8);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "!", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(bounds.size.w - 24, 8, 16, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // Whole-width state bar at the select-button level; the color cross-fades
  // on toggle. New row: snap to its static state color, no animation.
  if (s_bar_row != cell_index->row) {
    s_bar_row = cell_index->row;
    s_bar_color = cycle_color(e->cycle);
  }
  GRect bar = GRect(0, center - bar_h / 2, bounds.size.w, bar_h);
  graphics_context_set_fill_color(ctx, s_bar_color);
  graphics_fill_rect(ctx, bar, 0, GCornerNone);
  const char *st = (e->cycle == 1) ? "ON" : (e->cycle == 2) ? "CONFIRM" : "OFF";
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, st, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, center - 14, bounds.size.w, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Strict four-region layout: banner | top split (Type | Area) | state bar
  // | bottom split (Tags | Category). The splits are full-width label/value
  // rows — muted bold labels left, black values right, no boxes, nothing
  // overlaps the state band. Empty values render '—'. Value x-offsets are
  // the measured GOTHIC_14_BOLD label advances + 6px gap, so each value
  // gets the maximum remaining width. (Text rects are 16px tall: Gothic 14
  // glyph ink spans up to 16px and graphics_draw_text clips to its box.)
  const int16_t bar_top = center - bar_h / 2;
  const int16_t row_top1 = banner_h + 1;              // 33
  const int16_t row_top2 = row_top1 + 16;             // 49
  const int16_t row_bot1 = bar_top + bar_h + 4;       // 105
  const int16_t row_bot2 = row_bot1 + 19;             // 124
  const GColor muted = GColorDarkGray;

  // ---- Top split: Type | Area ----
  graphics_context_set_text_color(ctx, GColorBlack);
  if (e->type) {
    // Scene: drawn play triangle (U+25B6 is not in the system fonts), then
    // the word — same legend as the main screen's leading symbol.
    if (s_scene_tri) {
      graphics_context_set_fill_color(ctx, GColorBlack);
      gpath_move_to(s_scene_tri, GPoint(margin + 3, row_top1 + 8));
      gpath_draw_filled(ctx, s_scene_tri);
    }
    graphics_draw_text(ctx, "Scene", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(margin + 14, row_top1, bounds.size.w - margin - 14, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  } else {
    graphics_draw_text(ctx, "$", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(margin, row_top1, 10, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, "Script", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(margin + 14, row_top1, bounds.size.w - margin - 14, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // Area row: 'AREA' label (31px), value right after it (max width).
  graphics_context_set_text_color(ctx, muted);
  graphics_draw_text(ctx, "AREA", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(margin, row_top2, 31, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, e->area[0] ? e->area : "—",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(margin + 37, row_top2, bounds.size.w - margin - 47, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // ---- Bottom split: Tags | Category ----
  // Tags row: labels joined with '·' (one row, ellipsis on overflow).
  char tags[192];
  tags[0] = '\0';
  const char *p = e->labels;
  while (p && p[0]) {
    const char *comma = strchr(p, ',');
    size_t len = comma ? (size_t)(comma - p) : strlen(p);
    while (len > 0 && p[len - 1] == ' ') {
      len--;
    }
    size_t l = strlen(tags);
    snprintf(tags + l, sizeof(tags) - l, "%s%.*s", l ? " · " : "", (int)len, p);
    p = comma ? comma + 1 : NULL;
  }
  graphics_context_set_text_color(ctx, muted);
  graphics_draw_text(ctx, "TAGS", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(margin, row_bot1 + 1, 32, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, tags[0] ? tags : "—",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(margin + 38, row_bot1 + 1, bounds.size.w - margin - 48, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Category row: HA categories (entity-registry scope mapping) are only
  // exposed over WebSocket, which PebbleKit JS cannot use, so the value is
  // always '—' unless a future HA release exposes them via REST/templates.
  graphics_context_set_text_color(ctx, muted);
  graphics_draw_text(ctx, "CATEGORY", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(margin, row_bot2 + 1, 63, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "—", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(margin + 69, row_bot2 + 1, bounds.size.w - margin - 79, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Below the bar: nothing but the footer — the entity's icon stays a pure
  // glyph in the banner (no icon-name text anywhere).

  // Footer: the full entity id (script.<key> / scene.<key>, like the
  // notification timestamp line) — distinguishes same-named scene/script
  // rows at a glance.
  char foot[72];
  snprintf(foot, sizeof(foot), "%s.%s", TYPE_DOMAIN(e->type), e->key);
  graphics_context_set_text_color(ctx, GColorDarkGray);
  graphics_draw_text(ctx, foot, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(margin, bounds.size.h - 22, bounds.size.w - 2 * margin, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void edit_select_cb(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  if (cell_index->row >= s_script_count) {
    return;
  }
  ScriptEntry *e = &s_scripts[cell_index->row];
  e->cycle = (uint8_t)((e->cycle + 1) % 3);
  int32_t idx = shortcut_index_for_type(e->type, e->key);
  switch (e->cycle) {
    case 0:  // OFF: remove from the launcher
      if (idx >= 0) {
        unpick_script(cell_index->row);
      }
      break;
    case 1:  // ON: picked, runs directly
      if (idx >= 0) {
        s_shortcuts[idx].confirm = 0;
        persist_save();
      } else {
        pick_script(cell_index->row, 0);
      }
      break;
    default: // CONFIRM: picked, asks before running
      if (idx >= 0) {
        s_shortcuts[idx].confirm = 1;
        persist_save();
      } else {
        pick_script(cell_index->row, 1);
      }
      break;
  }
  bar_animate_to(cycle_color(e->cycle));
  menu_layer_reload_data(s_edit_menu);
}

// ---- window handlers ----

static void edit_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Notification-style picker: white page regardless of the app theme.
  window_set_background_color(window, GColorWhite);

  // No action bar: the picker is a full-bleed native-notification page.
  s_edit_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_edit_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = edit_get_num_sections,
    .get_num_rows = edit_get_num_rows,
    .get_cell_height = edit_get_cell_height,
    .draw_row = edit_draw_row,
    .select_click = edit_select_cb,
  });
  menu_layer_set_click_config_onto_window(s_edit_menu, window);
  menu_layer_pad_bottom_enable(s_edit_menu, true);
  // Theme + accent highlight.
  menu_layer_set_normal_colors(s_edit_menu, theme_bg(), theme_fg());
  menu_layer_set_highlight_colors(s_edit_menu, s_accent, GColorBlack);
  layer_add_child(root, menu_layer_get_layer(s_edit_menu));

  s_edit_status = text_layer_create(GRect(8, (bounds.size.h - 60) / 2, bounds.size.w - 20, 60));
  text_layer_set_font(s_edit_status, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_edit_status, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_edit_status, GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_edit_status, GColorClear);
  text_layer_set_text_color(s_edit_status, GColorBlack);
  layer_add_child(root, text_layer_get_layer(s_edit_status));
}

static void edit_window_unload(Window *window) {
  if (s_bar_anim) {
    Animation *a = s_bar_anim;
    s_bar_anim = NULL; // stopped handler sees the mismatch and bails
    animation_unschedule(a);
  }
  s_bar_row = 0xFFFF;
  menu_layer_destroy(s_edit_menu);
  text_layer_destroy(s_edit_status);
  s_edit_menu = NULL;
  s_edit_status = NULL;
  window_destroy(s_edit_window);
  s_edit_window = NULL;
}

static void edit_window_appear(Window *window) {
  s_edit_visible = true;
  edit_begin_collect(0);
  edit_show_status(s_edit_update_mode ? "Updating..." : "Fetching...",
                   s_edit_update_mode ? GColorBlack : GColorBlack);

  DictionaryIterator *iter;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if (res == APP_MSG_OK) {
    dict_write_int32(iter, MESSAGE_KEY_FetchScripts, 1);
    res = app_message_outbox_send();
  }
  if (res != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to send FetchScripts (%d)", (int)res);
    edit_show_status_error("Send failed");
  }
}

static void edit_window_disappear(Window *window) {
  s_edit_visible = false;
}

static void push_edit_window(void) {
  s_edit_update_mode = false;
  if (!s_edit_window) {
    s_edit_window = window_create();
    window_set_window_handlers(s_edit_window, (WindowHandlers){
      .load = edit_window_load,
      .appear = edit_window_appear,
      .disappear = edit_window_disappear,
      .unload = edit_window_unload,
    });
  }
  window_stack_push(s_edit_window, true);
}

static void push_update_window(void) {
  // Fullscreen loading indicator (working dialog) while the fetch streams;
  // metadata_apply() then shows the result and the dialog dismisses back to
  // the main menu, which redraws with the updated data.
  s_edit_update_mode = true;
  s_edit_visible = false;
  edit_begin_collect(0);
  dialog_show_working("Updating...");
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_int32(iter, MESSAGE_KEY_FetchScripts, 1);
    dict_write_end(iter);
    app_message_outbox_send();
  }
}

static void push_submenu_window(void);
static void push_reorder_window(void);

// ---------------------------------------------------------------------------
// Sub-menu (Shortcuts / Change order) + reorder mode
// ---------------------------------------------------------------------------

static Window *s_sub_window;
static MenuLayer *s_sub_menu;
static Window *s_reorder_window;
static MenuLayer *s_reorder_menu;
static int32_t s_reorder_held = -1;

static uint16_t sub_get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                                 void *callback_context) {
  return 5;
}

static const char *autoclose_label(int32_t seconds);
static void autoclose_cycle(void);
static const char *subtitle_label(uint8_t fields);
static void subtitle_cycle(void);

static void sub_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                         void *callback_context) {
  if (cell_index->row == 0) {
    menu_cell_basic_draw(ctx, cell_layer, "Shortcuts",
                         "Pick scripts from Home Assistant", NULL);
  } else if (cell_index->row == 1) {
    menu_cell_basic_draw(ctx, cell_layer, "Change Order",
                         "Select, move with up/down, select to drop", NULL);
  } else if (cell_index->row == 2) {
    menu_cell_basic_draw(ctx, cell_layer, "Update metadata",
                         "Refresh names, icons and labels from Home Assistant", NULL);
  } else if (cell_index->row == 3) {
    char sub[48];
    snprintf(sub, sizeof(sub), "%s - SELECT cycles", autoclose_label(s_autoclose_seconds));
    menu_cell_basic_draw(ctx, cell_layer, "Automatic close", sub, NULL);
  } else {
    char sub[48];
    snprintf(sub, sizeof(sub), "%s - SELECT cycles", subtitle_label(s_subtitle_fields));
    menu_cell_basic_draw(ctx, cell_layer, "Info line", sub, NULL);
  }
}

static void sub_select_cb(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  if (cell_index->row == 0) {
    push_edit_window();
  } else if (cell_index->row == 1) {
    push_reorder_window();
  } else if (cell_index->row == 2) {
    push_update_window();
  } else if (cell_index->row == 3) {
    autoclose_cycle();
  } else {
    subtitle_cycle();
  }
}

static void sub_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, theme_bg());
  s_sub_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_sub_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = sub_get_num_rows,
    .draw_row = sub_draw_row,
    .select_click = sub_select_cb,
  });
  menu_layer_set_click_config_onto_window(s_sub_menu, window);
  menu_layer_pad_bottom_enable(s_sub_menu, true);
  menu_layer_set_normal_colors(s_sub_menu, theme_bg(), theme_fg());
  menu_layer_set_highlight_colors(s_sub_menu, s_accent, GColorBlack);
  layer_add_child(root, menu_layer_get_layer(s_sub_menu));
}

static void sub_window_unload(Window *window) {
  menu_layer_destroy(s_sub_menu);
  s_sub_menu = NULL;
  window_destroy(s_sub_window);
  s_sub_window = NULL;
}

static void push_submenu_window(void) {
  s_sub_window = window_create();
  window_set_window_handlers(s_sub_window, (WindowHandlers){
    .load = sub_window_load,
    .unload = sub_window_unload,
  });
  window_stack_push(s_sub_window, true);
}

// ---------------------------------------------------------------------------
// Automatic close: cycles through the options on SELECT, directly in the
// sub-menu (no extra settings screen).
// ---------------------------------------------------------------------------

static const int32_t AUTOCLOSE_OPTIONS[] = { 0, 3, 5, 10, 15, 30 };
#define AUTOCLOSE_OPTION_COUNT ((int32_t)(sizeof(AUTOCLOSE_OPTIONS) / sizeof(AUTOCLOSE_OPTIONS[0])))

static const char *autoclose_label(int32_t seconds) {
  switch (seconds) {
    case 3: return "3s";
    case 5: return "5s";
    case 10: return "10s";
    case 15: return "15s";
    case 30: return "30s";
    default: return "Never";
  }
}

static void autoclose_cycle(void) {
  int32_t idx = 0;
  for (int32_t i = 0; i < AUTOCLOSE_OPTION_COUNT; i++) {
    if (AUTOCLOSE_OPTIONS[i] == s_autoclose_seconds) {
      idx = (i + 1) % AUTOCLOSE_OPTION_COUNT;
      break;
    }
  }
  s_autoclose_seconds = AUTOCLOSE_OPTIONS[idx];
  persist_write_int(PERSIST_KEY_AUTOCLOSE, s_autoclose_seconds);
  vibes_short_pulse();
  menu_layer_reload_data(s_sub_menu);
}

// ---------------------------------------------------------------------------
// Info line: which fields the main shortcut subtitle shows, cycled on SELECT
// in the sub-menu (no extra settings screen), like Automatic close.
// ---------------------------------------------------------------------------

//! Compact preset label: T = type symbol, A = area, Tg = tags, C = category.
static const char *subtitle_label(uint8_t fields) {
  switch (fields) {
    case SUBTITLE_FULL: return "T·A·Tg·C";
    case SUBTITLE_SYMBOL | SUBTITLE_AREA | SUBTITLE_TAGS: return "T·A·Tg";
    case SUBTITLE_SYMBOL | SUBTITLE_AREA: return "T·A";
    case SUBTITLE_AREA | SUBTITLE_TAGS | SUBTITLE_CATEGORY: return "A·Tg·C";
    case SUBTITLE_SYMBOL | SUBTITLE_TAGS | SUBTITLE_CATEGORY: return "T·Tg·C";
    default: return "none";
  }
}

static void subtitle_cycle(void) {
  uint8_t idx = 0;
  for (uint8_t i = 0; i < SUBTITLE_PRESET_COUNT; i++) {
    if (SUBTITLE_PRESETS[i] == s_subtitle_fields) {
      idx = (uint8_t)((i + 1) % SUBTITLE_PRESET_COUNT);
      break;
    }
  }
  s_subtitle_fields = SUBTITLE_PRESETS[idx];
  persist_write_int(PERSIST_KEY_SUBTITLE, (int32_t)s_subtitle_fields);
  vibes_short_pulse();
  menu_layer_reload_data(s_sub_menu);
}

// ---- reorder mode ----

// Reordering works on a scratch copy: up/down shifts mutate the scratch
// only; SELECT on the held row ("DROP") commits it to the real list and
// persists. BACK discards the pending moves, so leaving without an explicit
// drop never changes the stored order.
static Shortcut s_reorder_scratch[MAX_SHORTCUTS];

static uint16_t reorder_get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                                     void *callback_context) {
  return s_shortcut_count;
}

static void reorder_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                             void *callback_context) {
  if (cell_index->row >= s_shortcut_count) {
    return;
  }
  Shortcut *sc = &s_reorder_scratch[cell_index->row];
  GRect b = layer_get_bounds(cell_layer);
  bool selected = menu_layer_is_index_selected(s_reorder_menu, (MenuIndex *)cell_index);
  bool held = (s_reorder_held == (int32_t)cell_index->row);

  // Row background: accent when selected, theme otherwise.
  graphics_context_set_fill_color(ctx, selected ? s_accent : theme_bg());
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Icon at native 32x32; black glyphs on light/selected rows, white on
  // dark rows (RGBA glyphs composite their own color via GCompOpSet).
  uint32_t icon_res = (selected || !s_dark_mode) ? icon_resource(sc->icon_idx)
                                                 : icon_resource_white(sc->icon_idx);
  GBitmap *icon = gbitmap_create_with_resource(icon_res);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, icon, GRect(6, (b.size.h - 32) / 2, 32, 32));
  gbitmap_destroy(icon);

  graphics_context_set_text_color(ctx, selected ? GColorBlack : theme_fg());
  graphics_draw_text(ctx, sc->name[0] ? sc->name : sc->key,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(44, 4, b.size.w - 50, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // The subtitle tells what the next SELECT press does: grab this row
  // (MOVE), or commit it at the current position (DROP).
  const char *subtitle = held ? "DROP" : "MOVE";
  graphics_context_set_text_color(ctx, selected ? GColorBlack : theme_muted());
  graphics_draw_text(ctx, subtitle, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(44, 27, b.size.w - 50, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void reorder_toggle_hold(void) {
  MenuIndex idx = menu_layer_get_selected_index(s_reorder_menu);
  int32_t row = idx.row;
  if (row < 0 || row >= s_shortcut_count) {
    return;
  }
  if (s_reorder_held < 0) {
    // Grab.
    s_reorder_held = row;
    vibes_short_pulse();
  } else if (row == s_reorder_held) {
    // Drop: commit the scratch order to the real list and persist.
    memcpy(s_shortcuts, s_reorder_scratch, (size_t)s_shortcut_count * sizeof(Shortcut));
    persist_save();
    s_reorder_held = -1;
    vibes_short_pulse();
  }
  menu_layer_reload_data(s_reorder_menu);
}

static void reorder_select_click(ClickRecognizerRef rec, void *ctx) {
  reorder_toggle_hold();
}

static void reorder_move(int32_t delta) {
  if (s_reorder_held < 0) {
    return;
  }
  int32_t target = s_reorder_held + delta;
  if (target < 0 || target >= s_shortcut_count) {
    return;
  }
  Shortcut tmp = s_reorder_scratch[s_reorder_held];
  s_reorder_scratch[s_reorder_held] = s_reorder_scratch[target];
  s_reorder_scratch[target] = tmp;
  s_reorder_held = target;
  MenuIndex idx = { .section = 0, .row = (uint16_t)target };
  // Instant jump (no scroll animation): rapid up/down presses would cancel
  // the animation mid-flight and leave the held row half off-screen.
  menu_layer_set_selected_index(s_reorder_menu, idx, MenuRowAlignCenter, false);
  menu_layer_reload_data(s_reorder_menu);
}

static void reorder_up_click(ClickRecognizerRef rec, void *ctx) {
  if (s_reorder_held >= 0) {
    reorder_move(-1);
    return;
  }
  MenuIndex idx = menu_layer_get_selected_index(s_reorder_menu);
  if (idx.row > 0) {
    idx.row--;
    menu_layer_set_selected_index(s_reorder_menu, idx, MenuRowAlignCenter, true);
  }
}

static void reorder_down_click(ClickRecognizerRef rec, void *ctx) {
  if (s_reorder_held >= 0) {
    reorder_move(1);
    return;
  }
  MenuIndex idx = menu_layer_get_selected_index(s_reorder_menu);
  if (idx.row + 1 < s_shortcut_count) {
    idx.row++;
    menu_layer_set_selected_index(s_reorder_menu, idx, MenuRowAlignCenter, true);
  }
}

static void reorder_back_click(ClickRecognizerRef rec, void *ctx) {
  // Discard pending moves: nothing was committed unless SELECT dropped the
  // held shortcut. The scratch copy is simply dropped with the window.
  s_reorder_held = -1;
  window_stack_pop(true);
}

static void reorder_click_config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, reorder_select_click);
  window_single_click_subscribe(BUTTON_ID_UP, reorder_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, reorder_down_click);
  window_single_click_subscribe(BUTTON_ID_BACK, reorder_back_click);
}

static void reorder_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  // Snapshot the current order; moves only mutate the scratch until a drop.
  memcpy(s_reorder_scratch, s_shortcuts, (size_t)s_shortcut_count * sizeof(Shortcut));
  window_set_background_color(window, theme_bg());
  s_reorder_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_reorder_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = reorder_get_num_rows,
    .draw_row = reorder_draw_row,
  });
  menu_layer_pad_bottom_enable(s_reorder_menu, true);
  menu_layer_set_normal_colors(s_reorder_menu, theme_bg(), theme_fg());
  menu_layer_set_highlight_colors(s_reorder_menu, s_accent, GColorBlack);
  layer_add_child(root, menu_layer_get_layer(s_reorder_menu));
}

static void reorder_window_unload(Window *window) {
  menu_layer_destroy(s_reorder_menu);
  s_reorder_menu = NULL;
  s_reorder_held = -1;
  window_destroy(s_reorder_window);
  s_reorder_window = NULL;
}

static void push_reorder_window(void) {
  s_reorder_window = window_create();
  window_set_window_handlers(s_reorder_window, (WindowHandlers){
    .load = reorder_window_load,
    .unload = reorder_window_unload,
  });
  window_set_click_config_provider(s_reorder_window, reorder_click_config_provider);
  window_stack_push(s_reorder_window, true);
}

// ---------------------------------------------------------------------------
// Main window
// ---------------------------------------------------------------------------

static Window *s_main_window;

static uint16_t main_get_num_sections(MenuLayer *menu_layer, void *callback_context) {
  return 1;
}

static uint16_t main_get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                                  void *callback_context) {
  // Row 0 = narrow entry row (up-arrow -> Shortcuts / Change order sub-menu);
  // then one row per shortcut; a hint row when empty.
  return 1 + (s_shortcut_count > 0 ? s_shortcut_count : 1);
}

//! Row 0 is a narrow accent entry row (indicates "more above"); shortcuts
//! keep a comfortable touch target.
static int16_t main_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index,
                                    void *callback_context) {
  return cell_index->row == 0 ? 15 : 48;
}

static void main_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                          void *callback_context) {
  uint16_t row = cell_index->row;
  GRect bounds = layer_get_bounds(cell_layer);

  if (row == 0) {
    // Narrow entry row: three horizontal accent dots, centered.
    int16_t cx = bounds.size.w / 2;
    int16_t cy = bounds.size.h / 2;
    graphics_context_set_fill_color(ctx, s_accent);
    for (int i = -1; i <= 1; i++) {
      graphics_fill_circle(ctx, GPoint(cx + i * 6, cy), 2);
    }
    return;
  }
  if (row <= s_shortcut_count) {
    Shortcut *sc = &s_shortcuts[row - 1];
    GRect b = layer_get_bounds(cell_layer);
    bool selected = menu_layer_is_index_selected(s_main_menu, (MenuIndex *)cell_index);

    // Row background: accent when selected, theme otherwise.
    graphics_context_set_fill_color(ctx, selected ? s_accent : theme_bg());
    graphics_fill_rect(ctx, b, 0, GCornerNone);

    // Execution feedback: the row's own icon/text are replaced by the
    // state icon (rocket / check / alert) and label (LAUNCHING / DONE /
    // FAILED + short error) so the row background and accent selection
    // stay untouched.
    bool exec = (row == (uint16_t)s_exec_row);
    bool exec_failed = exec && (s_exec_state == EXEC_FAILED);
    GColor exec_col = exec ? (exec_failed ? GColorRed : GColorGreen) : GColorClear;

    uint32_t icon_res;
    if (exec) {
      // Tinted glyphs: the exec state's own color (matches the title text).
      icon_res = exec_failed ? RESOURCE_ID_ICON_ALERT_RED
                             : (s_exec_state == EXEC_DONE ? RESOURCE_ID_ICON_CHECK_GREEN
                                                          : RESOURCE_ID_ICON_ROCKET_GREEN);
    } else {
      // Black glyphs on light rows and on the accent selection (black text),
      // white glyphs on dark rows — the icon always matches the title color.
      icon_res = (selected || !s_dark_mode) ? icon_resource(sc->icon_idx)
                                            : icon_resource_white(sc->icon_idx);
    }
    GBitmap *icon = gbitmap_create_with_resource(icon_res);
    // RGBA glyphs composite their own color via GCompOpSet; the transparent
    // background is left untouched, so there is no white/black square.
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, icon, GRect(6, (b.size.h - 32) / 2, 32, 32));
    gbitmap_destroy(icon);

    const char *title;
    if (exec) {
      title = exec_failed ? "FAILED" : (s_exec_state == EXEC_DONE ? "DONE" : "LAUNCHING");
    } else {
      title = sc->name[0] ? sc->name : sc->key;
    }
    // Exec label color: the exec color on theme rows, black on the accent
    // selection — green/red on the user's accent can vanish (the tinted
    // state icon still carries the color on both).
    graphics_context_set_text_color(ctx,
        exec ? (selected ? GColorBlack : exec_col)
             : (selected ? GColorBlack : theme_fg()));
    graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(44, 4, b.size.w - 50, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    if (exec) {
      // Subtitle: the short error on failure, nothing otherwise.
      if (exec_failed) {
        graphics_context_set_text_color(ctx, selected ? GColorBlack : exec_col);
        graphics_draw_text(ctx, s_exec_error[0] ? s_exec_error : "Error",
                           fonts_get_system_font(FONT_KEY_GOTHIC_14),
                           GRect(44, 27, b.size.w - 50, 18),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      }
    } else if (sc->missing) {
      graphics_context_set_text_color(ctx, selected ? GColorDarkGray : GColorRed);
      graphics_draw_text(ctx, "Missing in Home Assistant",
                         fonts_get_system_font(FONT_KEY_GOTHIC_14),
                         GRect(44, 27, b.size.w - 68, 18),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      graphics_context_set_fill_color(ctx, GColorRed);
      graphics_fill_circle(ctx, GPoint(b.size.w - 16, 16), 8);
      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx, "!", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                         GRect(b.size.w - 24, 8, 16, 16),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    } else {
      // Second line: the Info-line setting picks which fields show, in
      // fixed order <symbol> · <area> · <tags> · <category> (0 = hidden).
      // The symbol always leads, followed by a '·' divider, so on overflow
      // the trailing ellipsis keeps the beginning fixed.
      uint8_t fields = s_subtitle_fields;
      if (fields) {
        bool sym = (fields & SUBTITLE_SYMBOL) != 0;
        char sub[192];
        sub[0] = '\0';
        if ((fields & SUBTITLE_AREA) && sc->area[0]) {
          snprintf(sub, sizeof(sub), "%s", sc->area);
        }
        if ((fields & SUBTITLE_TAGS) && sc->labels[0]) {
          size_t l = strlen(sub);
          snprintf(sub + l, sizeof(sub) - l, "%s%s", sub[0] ? " · " : "", sc->labels);
        }
        if (fields & SUBTITLE_CATEGORY) {
          size_t l = strlen(sub);
          // Category falls back to the HA domain default icon name.
          const char *category = sc->icon_name[0] ? sc->icon_name
                               : (sc->type ? "palette" : "script-text");
          snprintf(sub + l, sizeof(sub) - l, "%s%s", sub[0] ? " · " : "", category);
        }
        GColor sub_col = selected ? GColorBlack : theme_muted();
        graphics_context_set_text_color(ctx, sub_col);
        if (sym && sc->type) {
          // Scene: drawn play triangle (U+25B6 is not in the system fonts),
          // then a '·' divider before the fields — the triangle ends at
          // x=53, the text starts at x=57, so they never touch.
          if (s_scene_tri) {
            graphics_context_set_fill_color(ctx, sub_col);
            gpath_move_to(s_scene_tri, GPoint(45, 36));
            gpath_draw_filled(ctx, s_scene_tri);
          }
          if (sub[0]) {
            char s2[200];
            snprintf(s2, sizeof(s2), "· %s", sub);
            graphics_draw_text(ctx, s2, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                               GRect(57, 27, b.size.w - 63, 18),
                               GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
          }
        } else if (sym) {
          char s2[200];
          snprintf(s2, sizeof(s2), sub[0] ? "$ · %s" : "$", sub);
          graphics_draw_text(ctx, s2, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                             GRect(44, 27, b.size.w - 50, 18),
                             GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        } else {
          graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                             GRect(44, 27, b.size.w - 50, 18),
                             GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        }
      }
    }
  } else {
    menu_cell_basic_draw(ctx, cell_layer, "No shortcuts yet",
                         "Open Shortcuts to add", NULL);
  }
}

static void main_select_cb(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  uint16_t row = cell_index->row;
  if (row == 0) {
    push_submenu_window();
  } else if (row <= s_shortcut_count) {
    Shortcut *sc = &s_shortcuts[row - 1];
    if (sc->missing) {
      // Gone from Home Assistant: SELECT deletes, BACK keeps. Never try
      // to execute a shortcut that no longer exists.
      dialog_show_delete(sc);
      return;
    }
    execute_shortcut(sc);
  } else {
    push_submenu_window();
  }
}

static void apply_accent(void) {
  if (s_main_menu) {
    menu_layer_set_highlight_colors(s_main_menu, s_accent, GColorBlack);
  }
  if (s_edit_menu) {
    menu_layer_set_highlight_colors(s_edit_menu, s_accent, GColorBlack);
  }
}

static GColor theme_bg(void) { return s_dark_mode ? GColorBlack : GColorWhite; }
static GColor theme_fg(void) { return s_dark_mode ? GColorWhite : GColorBlack; }
static GColor theme_muted(void) { return s_dark_mode ? GColorLightGray : GColorDarkGray; }

static void apply_theme(void) {
  if (s_main_menu) {
    menu_layer_set_normal_colors(s_main_menu, theme_bg(), theme_fg());
  }
  if (s_edit_menu) {
    menu_layer_set_normal_colors(s_edit_menu, theme_bg(), theme_fg());
  }
}

// Custom click handling so UP on the first shortcut opens the sub-menu and
// the selection starts on the first shortcut (not the dots row).
static uint16_t main_total_rows(void) {
  return 1 + (s_shortcut_count > 0 ? s_shortcut_count : 1);
}

static void main_up_click(ClickRecognizerRef rec, void *ctx) {
  arm_autoclose(); // interaction resets the idle close countdown
  MenuIndex idx = menu_layer_get_selected_index(s_main_menu);
  if (idx.row <= 1) {
    push_submenu_window();  // push upwards on the first entry
    return;
  }
  idx.row--;
  menu_layer_set_selected_index(s_main_menu, idx, MenuRowAlignCenter, true);
}

static void main_down_click(ClickRecognizerRef rec, void *ctx) {
  arm_autoclose(); // interaction resets the idle close countdown
  MenuIndex idx = menu_layer_get_selected_index(s_main_menu);
  if (idx.row + 1 < main_total_rows()) {
    idx.row++;
    menu_layer_set_selected_index(s_main_menu, idx, MenuRowAlignCenter, true);
  }
}

static void main_select_click(ClickRecognizerRef rec, void *ctx) {
  arm_autoclose(); // interaction resets the idle close countdown
  MenuIndex idx = menu_layer_get_selected_index(s_main_menu);
  main_select_cb(s_main_menu, &idx, NULL);
}

static void main_click_config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, main_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, main_down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, main_select_click);
}

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, theme_bg());

  s_main_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_main_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = main_get_num_sections,
    .get_num_rows = main_get_num_rows,
    .get_cell_height = main_get_cell_height,
    .draw_row = main_draw_row,
    .select_click = main_select_cb,
  });
  window_set_click_config_provider(window, main_click_config_provider);
  menu_layer_pad_bottom_enable(s_main_menu, true);
  // Dark/light rows with accent highlight.
  menu_layer_set_normal_colors(s_main_menu, theme_bg(), theme_fg());
  menu_layer_set_highlight_colors(s_main_menu, s_accent, GColorBlack);
  layer_add_child(root, menu_layer_get_layer(s_main_menu));
  // Always start on the first shortcut.
  MenuIndex first = { .section = 0, .row = 1 };
  menu_layer_set_selected_index(s_main_menu, first, MenuRowAlignCenter, true);
}

static void main_window_unload(Window *window) {
  menu_layer_destroy(s_main_menu);
  s_main_menu = NULL;
}

static void main_window_appear(Window *window) {
  menu_layer_reload_data(s_main_menu);
  arm_autoclose(); // idle timeout starts when the main screen is visible
}

static void main_window_disappear(Window *window) {
  cancel_autoclose(); // any other window (sub-menu, dialog, settings) suspends it
}

static void push_main_window(void) {
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers){
    .load = main_window_load,
    .appear = main_window_appear,
    .disappear = main_window_disappear,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
}

// ---------------------------------------------------------------------------
// AppMessage
// ---------------------------------------------------------------------------

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_ResultCode))) {
    int32_t code = t->value->int32;
    Tuple *text_t = dict_find(iter, MESSAGE_KEY_ResultText);
    const char *text = text_t ? text_t->value->cstring : "";
    if (s_exec_row >= 0) {
      if (s_timeout_timer) { app_timer_cancel(s_timeout_timer); s_timeout_timer = NULL; }
      if (code == 200) {
        s_exec_state = EXEC_DONE;
        arm_autoclose();
      } else {
        s_exec_state = EXEC_FAILED;
        snprintf(s_exec_error, sizeof(s_exec_error), "%s", text[0] ? text : "Error");
      }
      arm_exec_revert();
      menu_layer_reload_data(s_main_menu);
    } else if (s_dialog_active) {
      if (code == 200) {
        dialog_show_final(true, "Done!");
        arm_autoclose();
      } else {
        dialog_show_final(false, text[0] ? text : "Error");
      }
    } else if (s_edit_visible) {
      edit_show_status_error(text[0] ? text : "Fetch error");
    } else {
      APP_LOG(APP_LOG_LEVEL_INFO, "ResultCode %ld with no active UI; ignored", (long)code);
    }
    return;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ShortcutCount))) {
    edit_begin_collect(t->value->int32);
    if (s_edit_visible && s_script_expected == 0) {
      edit_fetch_done();
    }
    return;
  }
  if (dict_find(iter, MESSAGE_KEY_ScriptName) || dict_find(iter, MESSAGE_KEY_ScriptKey)) {
    edit_collect_script(iter);
    return;
  }
  // Config save from Clay: the phone app delivers every messageKey value to
  // the watch. Store durably (flash) and acknowledge visibly.
  Tuple *base_t = dict_find(iter, MESSAGE_KEY_BaseUrl);
  if (base_t) {
    Tuple *tok_t = dict_find(iter, MESSAGE_KEY_Token);
    Tuple *conf_t = dict_find(iter, MESSAGE_KEY_ConfirmEnabled);
    Tuple *acc_t = dict_find(iter, MESSAGE_KEY_AccentColor);
    Tuple *dark_t = dict_find(iter, MESSAGE_KEY_DarkMode);
    Tuple *touch_t = dict_find(iter, MESSAGE_KEY_TouchEnabled);
    Tuple *auto_t = dict_find(iter, MESSAGE_KEY_AutoClose);
    persist_save_config(
      base_t->value->cstring,
      tok_t ? tok_t->value->cstring : NULL,
      conf_t ? conf_t->value->int32 != 0 : s_confirm_enabled,
      acc_t ? (uint32_t)acc_t->value->int32 : 0,
      dark_t ? dark_t->value->int32 != 0 : s_dark_mode,
      touch_t ? touch_t->value->int32 != 0 : s_touch_enabled,
      auto_t ? auto_t->value->int32 : s_autoclose_seconds);
    apply_accent();
    apply_theme();
    app_touch_navigation_enable(s_touch_enabled);
    APP_LOG(APP_LOG_LEVEL_INFO, "Config saved from phone");
    if (s_dialog_active) {
      dialog_show_final(true, "Settings saved");
    } else {
      dialog_create();
      dialog_show_final(true, "Settings saved");
    }
    return;
  }
  // Config request from the JS (on 'ready'): reply with the durable copy.
  if (dict_find(iter, MESSAGE_KEY_RequestConfig)) {
    DictionaryIterator *out;
    if (app_message_outbox_begin(&out) == APP_MSG_OK) {
      dict_write_cstring(out, MESSAGE_KEY_BaseUrl, s_base_url);
      dict_write_cstring(out, MESSAGE_KEY_Token, s_token);
      dict_write_int32(out, MESSAGE_KEY_ConfirmEnabled, s_confirm_enabled ? 1 : 0);
      dict_write_int32(out, MESSAGE_KEY_AccentColor, (int32_t)s_accent_hex);
      dict_write_int32(out, MESSAGE_KEY_DarkMode, s_dark_mode ? 1 : 0);
      dict_write_int32(out, MESSAGE_KEY_TouchEnabled, s_touch_enabled ? 1 : 0);
      dict_write_int32(out, MESSAGE_KEY_AutoClose, s_autoclose_seconds);
      dict_write_end(out);
      app_message_outbox_send();
    }
    return;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ConfirmEnabled))) {
    s_confirm_enabled = t->value->int32 != 0;
    persist_write_int(PERSIST_KEY_CONFIRM, s_confirm_enabled ? 1 : 0);
    APP_LOG(APP_LOG_LEVEL_INFO, "ConfirmEnabled set to %d", s_confirm_enabled ? 1 : 0);
    return;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Unrecognized AppMessage payload");
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage inbox dropped (%d)", (int)reason);
}

static void outbox_sent(DictionaryIterator *iter, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "AppMessage sent");
}

static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage outbox failed");
  if (s_exec_row >= 0) {
    if (s_timeout_timer) { app_timer_cancel(s_timeout_timer); s_timeout_timer = NULL; }
    s_exec_state = EXEC_FAILED;
    snprintf(s_exec_error, sizeof(s_exec_error), "Send failed");
    arm_exec_revert();
    menu_layer_reload_data(s_main_menu);
  } else if (s_dialog_active) {
    dialog_show_final(false, "Send failed");
  } else if (s_edit_visible) {
    edit_show_status_error("Send failed");
  }
}

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------

static void init(void) {
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_sent(outbox_sent);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(4096, 1024);

  s_scene_tri = gpath_create(&SCENE_TRI_PATH_INFO);

  persist_load();

  // Native touch navigation (the firmware's own Tier-1 MenuLayer handling),
  // enabled only when the user turns it on in settings: on firmware 4.33.1
  // the opt-in currently faults on first touch (PebbleOS issue #1865), so the
  // default is OFF until that firmware bug is fixed.
  app_touch_navigation_enable(s_touch_enabled);

  push_main_window();
}

static void deinit(void) {
  if (s_main_window) {
    window_destroy(s_main_window);
    s_main_window = NULL;
  }
  if (s_scene_tri) {
    gpath_destroy(s_scene_tri);
    s_scene_tri = NULL;
  }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
