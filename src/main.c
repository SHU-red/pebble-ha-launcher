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

#define MAX_SHORTCUTS 64

#define PERSIST_KEY_COUNT 1          // int32: number of stored shortcuts
#define PERSIST_KEY_CONFIRM 2        // int32: 0/1 confirm-before-execute
#define PERSIST_KEY_BASEURL 3        // string: Home Assistant base URL
#define PERSIST_KEY_TOKEN 4          // string: long-lived access token
#define PERSIST_KEY_ACCENT 5         // int32: GColor8 argb value of the accent color
#define PERSIST_KEY_DARKMODE 6       // int32: 1 = dark, 0 = light
#define PERSIST_KEY_SHORTCUT_BASE 100 // + i: shortcut structs

#define DEFAULT_ACCENT_ARGB8 198     // GColorCobaltBlue

#define REQUEST_TIMEOUT_MS 10000     // execute request timeout
#define RESULT_DISMISS_MS 1500       // final result auto-dismiss
#define PULSE_INTERVAL_MS 250        // "Sending..." animated ellipsis

#define ICON_PLUS_IDX 46             // index of ICON_PLUS in the icon table
#define ICON_ARROW_UP_IDX 89         // index of ICON_ARROW_UP in the icon table
#define ICON_ARROW_DOWN_IDX 90       // index of ICON_ARROW_DOWN in the icon table

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
  RESOURCE_ID_ICON_PIGGY_BANK,
  RESOURCE_ID_ICON_GAMEPAD,
  RESOURCE_ID_ICON_GAUGE,
  RESOURCE_ID_ICON_WEATHER_SUNSET_UP,
  RESOURCE_ID_ICON_WEATHER_CLOUDY,
  RESOURCE_ID_ICON_WEATHER_RAINY,
  RESOURCE_ID_ICON_WEATHER_FOG,
  RESOURCE_ID_ICON_BELL_OFF,
  RESOURCE_ID_ICON_SHUFFLE,
  RESOURCE_ID_ICON_REPEAT,
  RESOURCE_ID_ICON_ARROW_UP,
  RESOURCE_ID_ICON_ARROW_DOWN,
};

#define ICON_COUNT ((uint8_t)(sizeof(ICONS) / sizeof(ICONS[0])))

//! Map a curated icon index to a resource id; out-of-range clamps to index 0.
static uint32_t icon_resource(uint8_t idx) {
  if (idx >= ICON_COUNT) {
    return ICONS[0];
  }
  return ICONS[idx];
}

// ---------------------------------------------------------------------------
// Persisted shortcut storage
// ---------------------------------------------------------------------------

typedef struct {
  char key[64];
  char name[48];
  char area[32];
  uint8_t icon_idx;
} Shortcut;

static Shortcut s_shortcuts[MAX_SHORTCUTS];
static uint16_t s_shortcut_count;
static bool s_confirm_enabled;
static char s_base_url[256];
static char s_token[256];
static GColor s_accent;
static uint8_t s_accent_argb;
static bool s_dark_mode;

static int32_t shortcut_index_for_key(const char *key) {
  if (!key || !key[0]) {
    return -1;
  }
  for (uint16_t i = 0; i < s_shortcut_count; i++) {
    if (strcmp(s_shortcuts[i].key, key) == 0) {
      return (int32_t)i;
    }
  }
  return -1;
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
    if (persist_read_data(PERSIST_KEY_SHORTCUT_BASE + i, &s_shortcuts[i], sizeof(Shortcut))
        != (int)sizeof(Shortcut)) {
      memset(&s_shortcuts[i], 0, sizeof(Shortcut));
    }
  }
  s_confirm_enabled = persist_read_int(PERSIST_KEY_CONFIRM) != 0;

  persist_read_string(PERSIST_KEY_BASEURL, s_base_url, sizeof(s_base_url));
  persist_read_string(PERSIST_KEY_TOKEN, s_token, sizeof(s_token));

  s_accent_argb = (uint8_t)(persist_read_int(PERSIST_KEY_ACCENT) & 0xFF);
  if (s_accent_argb == 0) {
    s_accent_argb = DEFAULT_ACCENT_ARGB8;
  }
  s_accent = (GColor){ .argb = s_accent_argb };
  s_dark_mode = persist_read_int(PERSIST_KEY_DARKMODE) != 0;
}

static void persist_save_config(const char *base_url, const char *token, bool confirm,
                                uint8_t accent_argb, bool dark_mode) {
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
  if (accent_argb != 0) {
    s_accent_argb = accent_argb;
    s_accent = (GColor){ .argb = s_accent_argb };
    persist_write_int(PERSIST_KEY_ACCENT, s_accent_argb);
  }
  s_dark_mode = dark_mode;
  persist_write_int(PERSIST_KEY_DARKMODE, s_dark_mode ? 1 : 0);
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
static char s_dialog_text_buf[128];
static uint8_t s_pulse_phase;
static GColor s_dialog_color;

static void start_execute(const char *key);

typedef struct {
  char key[64];
  char name[48];
} ConfirmCtx;

static ConfirmCtx s_confirm_ctx;

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

static void dialog_confirm_select(ClickRecognizerRef rec, void *ctx) {
  if (!s_dialog_confirm) return;
  s_dialog_confirm = false;
  start_execute(s_confirm_ctx.key);
}

static void dialog_confirm_cancel(ClickRecognizerRef rec, void *ctx) {
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
  dialog_show_final(false, "Timeout");
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
static void dialog_show_confirm(const Shortcut *sc) {
  if (!s_dialog_active) {
    dialog_create();
  }
  dialog_cancel_timers();
  s_dialog_confirm = true;
  s_dialog_color = GColorOrange;
  layer_mark_dirty(s_dialog_bg);
  text_layer_set_text_color(s_dialog_text, GColorWhite);
  snprintf(s_confirm_ctx.name, sizeof(s_confirm_ctx.name), "%s",
           sc->name[0] ? sc->name : sc->key);
  snprintf(s_confirm_ctx.key, sizeof(s_confirm_ctx.key), "%s", sc->key);
  snprintf(s_dialog_text_buf, sizeof(s_dialog_text_buf),
           "Run %s?\n\nSELECT: confirm\nBACK: cancel", s_confirm_ctx.name);
  text_layer_set_text(s_dialog_text, s_dialog_text_buf);
  vibes_short_pulse();
}

//! Green working dialog (auto_dismiss = false, animated ellipsis).
static void dialog_show_working(const char *text) {
  if (!s_dialog_active) {
    dialog_create();
  }
  dialog_cancel_timers();
  s_dialog_confirm = false;
  s_dialog_color = GColorGreen;
  layer_mark_dirty(s_dialog_bg);
  text_layer_set_text_color(s_dialog_text, GColorWhite);
  s_pulse_phase = 3;
  text_layer_set_text(s_dialog_text, text);
  s_pulse_timer = app_timer_register(PULSE_INTERVAL_MS, pulse_tick_cb, NULL);
}

//! Green (success) or red (failure) final dialog, auto-dismissed after 1.5s.
static void dialog_show_final(bool success, const char *text) {
  if (!s_dialog_active) {
    return;
  }
  dialog_cancel_timers();
  s_dialog_confirm = false;
  s_dialog_color = success ? GColorGreen : GColorRed;
  layer_mark_dirty(s_dialog_bg);
  text_layer_set_text_color(s_dialog_text, GColorWhite);
  snprintf(s_dialog_text_buf, sizeof(s_dialog_text_buf), "%s", text);
  text_layer_set_text(s_dialog_text, s_dialog_text_buf);
  if (success) {
    vibes_short_pulse();
  } else {
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

static void start_execute(const char *key);

static void action_menu_did_close(ActionMenu *menu, const ActionMenuItem *performed_action,
                                  void *context) {
  ActionMenuLevel *root = action_menu_get_root_level(menu);
  if (root) {
    action_menu_hierarchy_destroy(root, NULL, NULL);
  }
}

static void start_execute(const char *key) {
  dialog_show_working("Sending...");
  DictionaryIterator *iter;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if (res == APP_MSG_OK) {
    dict_write_cstring(iter, MESSAGE_KEY_ScriptKey, key);
    res = app_message_outbox_send();
  }
  if (res != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to send ScriptKey (%d)", (int)res);
    dialog_show_final(false, "Send failed");
    return;
  }
  s_timeout_timer = app_timer_register(REQUEST_TIMEOUT_MS, request_timeout_cb, NULL);
}

static void execute_shortcut(const Shortcut *sc) {
  if (s_confirm_enabled) {
    // One more SELECT on the orange approval screen; BACK cancels.
    dialog_show_confirm(sc);
  } else {
    start_execute(sc->key);
  }
}

// ---------------------------------------------------------------------------
// Edit window (browse scripts on Home Assistant)
// ---------------------------------------------------------------------------

typedef struct {
  char key[64];
  char name[48];
  char area[32];
  char labels[64];
  uint8_t icon_idx;
} ScriptEntry;

static ScriptEntry s_scripts[MAX_SHORTCUTS];
static uint16_t s_script_count;
static uint16_t s_script_expected;
static ScriptEntry s_pending;
static bool s_pending_active;

static Window *s_edit_window;
static MenuLayer *s_edit_menu;
static ActionBarLayer *s_edit_bar;
static TextLayer *s_edit_status;
static bool s_edit_visible;

static void edit_render(void);

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
  s_scripts[s_script_count++] = s_pending;
  memset(&s_pending, 0, sizeof(s_pending));
  s_pending_active = false;
}

//! Collect one script entry from an incoming message. Entries arrive as
//! {ScriptName, ScriptKey, ScriptArea, ScriptLabels, ScriptIcon}; a new
//! ScriptName commits the previous entry and starts the next one.
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
  if ((t = dict_find(iter, MESSAGE_KEY_ScriptArea))) {
    snprintf(s_pending.area, sizeof(s_pending.area), "%s", t->value->cstring);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ScriptLabels))) {
    snprintf(s_pending.labels, sizeof(s_pending.labels), "%s", t->value->cstring);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ScriptIcon))) {
    s_pending.icon_idx = (uint8_t)t->value->int32;
  }
  if (!s_pending_active || s_script_expected == 0) {
    return;
  }
  uint16_t have = (uint16_t)(s_script_count + 1);
  if (have >= s_script_expected) {
    edit_commit_pending();
    edit_render();
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
  edit_hide_status();
  // The edit screen contacted HA anyway: refresh the stored shortcut icons so
  // the main list shows the scripts' current icons.
  bool changed = false;
  for (uint16_t i = 0; i < s_script_count; i++) {
    int32_t idx = shortcut_index_for_key(s_scripts[i].key);
    if (idx >= 0 && s_scripts[i].icon_idx != s_shortcuts[idx].icon_idx) {
      s_shortcuts[idx].icon_idx = s_scripts[i].icon_idx;
      changed = true;
    }
  }
  if (changed) {
    persist_save();
  }
  menu_layer_reload_data(s_edit_menu);
}

// ---- pick / unpick ----

static void pick_script(uint16_t row) {
  ScriptEntry *e = &s_scripts[row];
  if (!e->key[0] || shortcut_index_for_key(e->key) >= 0) {
    return;
  }
  if (s_shortcut_count >= MAX_SHORTCUTS) {
    edit_show_status("Max 64 shortcuts", GColorRed);
    return;
  }
  Shortcut *sc = &s_shortcuts[s_shortcut_count];
  snprintf(sc->key, sizeof(sc->key), "%s", e->key);
  snprintf(sc->name, sizeof(sc->name), "%s", e->name[0] ? e->name : e->key);
  snprintf(sc->area, sizeof(sc->area), "%s", e->area);
  sc->icon_idx = e->icon_idx;
  s_shortcut_count++;
  persist_save();
  edit_render();
}

static void unpick_script(uint16_t row) {
  ScriptEntry *e = &s_scripts[row];
  int32_t idx = shortcut_index_for_key(e->key);
  if (idx < 0) {
    return;
  }
  memmove(&s_shortcuts[idx], &s_shortcuts[idx + 1],
          (size_t)(s_shortcut_count - (uint16_t)idx - 1) * sizeof(Shortcut));
  s_shortcut_count--;
  persist_save();
  edit_render();
}

static void pick_action_cb(ActionMenu *menu, const ActionMenuItem *action, void *context) {
  uint16_t row = (uint16_t)(uintptr_t)action_menu_item_get_action_data(action);
  if (row < s_script_count) {
    if (shortcut_index_for_key(s_scripts[row].key) >= 0) {
      unpick_script(row);
    } else {
      pick_script(row);
    }
  }
  action_menu_close(menu, true);
}

static void open_pick_menu(uint16_t row) {
  ScriptEntry *e = &s_scripts[row];
  bool picked = shortcut_index_for_key(e->key) >= 0;
  ActionMenuLevel *level = action_menu_level_create(1);
  action_menu_level_add_action(level, picked ? "Unpick" : "Pick", pick_action_cb,
                               (void *)(uintptr_t)row);
  ActionMenuConfig config = {
    .root_level = level,
    .context = NULL,
    .colors = { .background = GColorBlack, .foreground = s_accent },
    .will_close = NULL,
    .did_close = action_menu_did_close,
    .align = ActionMenuAlignCenter,
  };
  action_menu_open(&config);
}

// ---- menu callbacks ----

static uint16_t edit_get_num_sections(MenuLayer *menu_layer, void *callback_context) {
  return 1;
}

static uint16_t edit_get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                                  void *callback_context) {
  return s_script_count;
}

static void build_edit_subtitle(const ScriptEntry *e, char *out, size_t out_len) {
  out[0] = 0;
  if (e->area[0] && e->labels[0]) {
    snprintf(out, out_len, "Area: %s \xC2\xB7 Tags: %s", e->area, e->labels);
  } else if (e->area[0]) {
    snprintf(out, out_len, "Area: %s", e->area);
  } else if (e->labels[0]) {
    snprintf(out, out_len, "Tags: %s", e->labels);
  }
}

// Full-screen notification-style card per script (one card per screen).
static int16_t edit_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index,
                                    void *callback_context) {
  return layer_get_bounds(menu_layer_get_layer(menu_layer)).size.h;
}

static void edit_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                          void *callback_context) {
  ScriptEntry *e = &s_scripts[cell_index->row];
  GRect bounds = layer_get_bounds(cell_layer);
  const int16_t band_h = 48;

  // Card background (theme).
  graphics_context_set_fill_color(ctx, theme_bg());
  graphics_fill_rect(ctx, bounds, 0, 0);

  // Accent band with the script name; black check marker when picked.
  GRect band = GRect(0, 0, bounds.size.w, band_h);
  graphics_context_set_fill_color(ctx, s_accent);
  graphics_fill_rect(ctx, band, 0, 0);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, e->name[0] ? e->name : e->key,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(8, (band_h - 26) / 2, bounds.size.w - 44, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (shortcut_index_for_key(e->key) >= 0) {
    GBitmap *check = gbitmap_create_with_resource(RESOURCE_ID_ICON_CHECK);
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_draw_bitmap_in_rect(ctx, check,
                                 GRect(bounds.size.w - 32, (band_h - 22) / 2, 22, 22));
    gbitmap_destroy(check);
  }

  // Details: area · tags, then the entity key.
  char subtitle[112];
  build_edit_subtitle(e, subtitle, sizeof(subtitle));
  graphics_context_set_text_color(ctx, theme_fg());
  int16_t y = band_h + 12;
  if (subtitle[0]) {
    graphics_draw_text(ctx, subtitle, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(10, y, bounds.size.w - 20, 28),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    y += 32;
  }
  graphics_context_set_text_color(ctx, theme_muted());
  graphics_draw_text(ctx, e->key, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(10, y, bounds.size.w - 20, 20),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void edit_select_cb(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  if (cell_index->row < s_script_count) {
    open_pick_menu(cell_index->row);
  }
}

// ---- window handlers ----

static void edit_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, theme_bg());

  // Native right-edge action bar (like PebbleOS notifications): up/down
  // navigation and the select (pick/unpick) affordance. Touch taps on the bar
  // are zoned into the corresponding button events by the system.
  s_edit_bar = action_bar_layer_create();
  action_bar_layer_add_to_window(s_edit_bar, window);
  GBitmap *up_icon = gbitmap_create_with_resource(icon_resource(ICON_ARROW_UP_IDX));
  GBitmap *down_icon = gbitmap_create_with_resource(icon_resource(ICON_ARROW_DOWN_IDX));
  GBitmap *sel_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_CHECK);
  action_bar_layer_set_icon(s_edit_bar, BUTTON_ID_UP, up_icon);
  action_bar_layer_set_icon(s_edit_bar, BUTTON_ID_DOWN, down_icon);
  action_bar_layer_set_icon(s_edit_bar, BUTTON_ID_SELECT, sel_icon);

  GRect menu_bounds = bounds;
  menu_bounds.size.w -= ACTION_BAR_WIDTH;

  s_edit_menu = menu_layer_create(menu_bounds);
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
  text_layer_set_text_color(s_edit_status, theme_fg());
  layer_add_child(root, text_layer_get_layer(s_edit_status));
}

static void edit_window_unload(Window *window) {
  menu_layer_destroy(s_edit_menu);
  action_bar_layer_destroy(s_edit_bar);
  text_layer_destroy(s_edit_status);
  s_edit_menu = NULL;
  s_edit_bar = NULL;
  s_edit_status = NULL;
  window_destroy(s_edit_window);
  s_edit_window = NULL;
}

static void edit_window_appear(Window *window) {
  s_edit_visible = true;
  edit_begin_collect(0);
  edit_show_status("Fetching...", GColorBlack);

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

static void swap_shortcuts(int32_t a, int32_t b) {
  if (a < 0 || b < 0 || a >= s_shortcut_count || b >= s_shortcut_count || a == b) {
    return;
  }
  Shortcut tmp = s_shortcuts[a];
  s_shortcuts[a] = s_shortcuts[b];
  s_shortcuts[b] = tmp;
  persist_save();
}

static uint16_t sub_get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                                 void *callback_context) {
  return 2;
}

static void sub_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                         void *callback_context) {
  if (cell_index->row == 0) {
    menu_cell_basic_draw(ctx, cell_layer, "Shortcuts",
                         "Pick scripts from Home Assistant", NULL);
  } else {
    menu_cell_basic_draw(ctx, cell_layer, "Change Order",
                         "Select, move with up/down, select to drop", NULL);
  }
}

static void sub_select_cb(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  if (cell_index->row == 0) {
    push_edit_window();
  } else {
    push_reorder_window();
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

// ---- reorder mode ----

static uint16_t reorder_get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                                     void *callback_context) {
  return s_shortcut_count;
}

static void reorder_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                             void *callback_context) {
  if (cell_index->row >= s_shortcut_count) {
    return;
  }
  Shortcut *sc = &s_shortcuts[cell_index->row];
  char subtitle[64];
  if (s_reorder_held == (int32_t)cell_index->row) {
    snprintf(subtitle, sizeof(subtitle), "Moving - up/down shifts, SELECT drops");
  } else {
    snprintf(subtitle, sizeof(subtitle), "%d of %d", cell_index->row + 1, s_shortcut_count);
  }
  GBitmap *icon = gbitmap_create_with_resource(icon_resource(sc->icon_idx));
  menu_cell_basic_draw(ctx, cell_layer, sc->name[0] ? sc->name : sc->key, subtitle, icon);
  gbitmap_destroy(icon);
}

static void reorder_select_click(ClickRecognizerRef rec, void *ctx) {
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
    // Drop.
    s_reorder_held = -1;
    vibes_short_pulse();
  }
  menu_layer_reload_data(s_reorder_menu);
}

static void reorder_move(int32_t delta) {
  if (s_reorder_held < 0) {
    return;
  }
  int32_t target = s_reorder_held + delta;
  if (target < 0 || target >= s_shortcut_count) {
    return;
  }
  swap_shortcuts(s_reorder_held, target);
  s_reorder_held = target;
  MenuIndex idx = { .section = 0, .row = (uint16_t)target };
  menu_layer_set_selected_index(s_reorder_menu, idx, MenuRowAlignCenter, true);
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
  if (s_reorder_held >= 0) {
    s_reorder_held = -1;
    persist_save();
  }
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
static MenuLayer *s_main_menu;

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
  return cell_index->row == 0 ? 30 : 48;
}

static void main_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                          void *callback_context) {
  uint16_t row = cell_index->row;
  GRect bounds = layer_get_bounds(cell_layer);
  if (row == 0) {
    // Accent entry row: up arrow + label, indicating the sub-menu above.
    graphics_context_set_fill_color(ctx, s_accent);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    GBitmap *up = gbitmap_create_with_resource(icon_resource(ICON_ARROW_UP_IDX));
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_draw_bitmap_in_rect(ctx, up, GRect(5, (bounds.size.h - 22) / 2, 22, 22));
    gbitmap_destroy(up);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, "Shortcuts \xC2\xB7 Change order",
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(32, (bounds.size.h - 22) / 2, bounds.size.w - 36, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    return;
  }
  if (row <= s_shortcut_count) {
    Shortcut *sc = &s_shortcuts[row - 1];
    GBitmap *icon = gbitmap_create_with_resource(icon_resource(sc->icon_idx));
    menu_cell_basic_draw(ctx, cell_layer, sc->name[0] ? sc->name : sc->key,
                         sc->area[0] ? sc->area : NULL, icon);
    gbitmap_destroy(icon);
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
    execute_shortcut(&s_shortcuts[row - 1]);
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
  menu_layer_set_click_config_onto_window(s_main_menu, window);
  menu_layer_pad_bottom_enable(s_main_menu, true);
  // Dark/light rows with accent highlight.
  menu_layer_set_normal_colors(s_main_menu, theme_bg(), theme_fg());
  menu_layer_set_highlight_colors(s_main_menu, s_accent, GColorBlack);
  layer_add_child(root, menu_layer_get_layer(s_main_menu));
}

static void main_window_unload(Window *window) {
  menu_layer_destroy(s_main_menu);
  s_main_menu = NULL;
}

static void main_window_appear(Window *window) {
  menu_layer_reload_data(s_main_menu);
}

static void push_main_window(void) {
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers){
    .load = main_window_load,
    .appear = main_window_appear,
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
    if (s_dialog_active) {
      if (code == 200) {
        dialog_show_final(true, "Done!");
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
      edit_render();
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
    persist_save_config(
      base_t->value->cstring,
      tok_t ? tok_t->value->cstring : NULL,
      conf_t ? conf_t->value->int32 != 0 : s_confirm_enabled,
      acc_t ? (uint8_t)(acc_t->value->int32 & 0xFF) : 0,
      dark_t ? dark_t->value->int32 != 0 : s_dark_mode);
    apply_accent();
    apply_theme();
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
      dict_write_int32(out, MESSAGE_KEY_AccentColor, s_accent_argb);
      dict_write_int32(out, MESSAGE_KEY_DarkMode, s_dark_mode ? 1 : 0);
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
  if (s_dialog_active) {
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

  persist_load();

  // Opt in to touch navigation so the MenuLayers scroll and activate by
  // swipe/tap on touch hardware; buttons keep working as before. The call is
  // a no-op macro on non-touch platforms, so no compile-time guard is needed.
  app_touch_navigation_enable(true);

  push_main_window();
}

static void deinit(void) {
  if (s_main_window) {
    window_destroy(s_main_window);
    s_main_window = NULL;
  }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
