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
#define PERSIST_KEY_SHORTCUT_BASE 100 // + i: shortcut structs

#define REQUEST_TIMEOUT_MS 10000     // execute request timeout
#define RESULT_DISMISS_MS 1500       // final result auto-dismiss
#define PULSE_INTERVAL_MS 250        // "Sending..." animated ellipsis

#define HEADER_HEIGHT 16
#define SECTION_ACTIONS 0
#define SECTION_SHORTCUTS 1

#define ICON_PLUS_IDX 46             // index of ICON_PLUS in the icon table

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
static Animation *s_slide_anim;
static bool s_dialog_active;
static char s_dialog_text_buf[128];
static uint8_t s_pulse_phase;
static GColor s_dialog_color;

//! Fill the dialog background with the current color.
static void dialog_bg_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, s_dialog_color);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void dialog_show_final(bool success, const char *text);
static void dialog_unload(Window *window);

static void dialog_dismiss_cb(void *data) {
  s_dismiss_timer = NULL;
  window_stack_pop_all(true);
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

static void dialog_slide_in_anim_stopped(Animation *animation, bool finished, void *context) {
  s_slide_anim = NULL;
  animation_destroy(animation);
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
  if (s_slide_anim) {
    animation_unschedule(s_slide_anim);
    animation_destroy(s_slide_anim);
    s_slide_anim = NULL;
  }
}

static void dialog_animate_in(void) {
  Layer *root = window_get_root_layer(s_dialog_window);
  GRect bounds = layer_get_bounds(root);
  GRect from = GRect(0, bounds.size.h, bounds.size.w, bounds.size.h);
  GRect to = GRect(0, 0, bounds.size.w, bounds.size.h);
  PropertyAnimation *pa = property_animation_create_layer_frame(s_dialog_bg, &from, &to);
  s_slide_anim = property_animation_get_animation(pa);
  animation_set_duration(s_slide_anim, 250);
  animation_set_curve(s_slide_anim, AnimationCurveEaseOut);
  animation_set_handlers(s_slide_anim,
                         (AnimationHandlers){ .stopped = dialog_slide_in_anim_stopped }, NULL);
  animation_schedule(s_slide_anim);
}

static void dialog_create(void) {
  s_dialog_window = window_create();
  window_set_window_handlers(s_dialog_window, (WindowHandlers){
    .unload = dialog_unload,
  });

  Layer *root = window_get_root_layer(s_dialog_window);
  GRect bounds = layer_get_bounds(root);

  s_dialog_bg = layer_create(bounds);
  layer_set_update_proc(s_dialog_bg, dialog_bg_update_proc);
  s_dialog_color = GColorGreen;
  layer_add_child(root, s_dialog_bg);

  s_dialog_text = text_layer_create(GRect(8, (bounds.size.h - 60) / 2, bounds.size.w - 16, 60));
  text_layer_set_font(s_dialog_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_dialog_text, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_dialog_text, GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_dialog_text, GColorClear);
  text_layer_set_text_color(s_dialog_text, GColorWhite);
  layer_add_child(s_dialog_bg, text_layer_get_layer(s_dialog_text));

  s_dialog_active = true;
  window_stack_push(s_dialog_window, false);
  dialog_animate_in();
}

//! Green working dialog (auto_dismiss = false, animated ellipsis).
static void dialog_show_working(const char *text) {
  if (!s_dialog_active) {
    dialog_create();
  }
  dialog_cancel_timers();
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

typedef struct {
  char key[64];
  char name[48];
} ConfirmCtx;

static ConfirmCtx s_confirm_ctx;

static void action_menu_did_close(ActionMenu *menu, const ActionMenuItem *performed_action,
                                  void *context) {
  ActionMenuLevel *root = action_menu_get_root_level(menu);
  if (root) {
    action_menu_hierarchy_destroy(root, NULL, NULL);
  }
}

static void confirm_run_cb(ActionMenu *menu, const ActionMenuItem *action, void *context) {
  ConfirmCtx *c = (ConfirmCtx *)action_menu_item_get_action_data(action);
  if (c) {
    start_execute(c->key);
  }
  action_menu_close(menu, true);
}

static void confirm_cancel_cb(ActionMenu *menu, const ActionMenuItem *action, void *context) {
  action_menu_close(menu, true);
}

//! Confirm-before-run ActionMenu: root item shows the script name (crumb),
//! child level offers Run / Cancel. On touch hardware taps activate items.
static void open_confirm_menu(const Shortcut *sc) {
  snprintf(s_confirm_ctx.name, sizeof(s_confirm_ctx.name), "%s",
           sc->name[0] ? sc->name : sc->key);
  snprintf(s_confirm_ctx.key, sizeof(s_confirm_ctx.key), "%s", sc->key);

  ActionMenuLevel *root = action_menu_level_create(1);
  ActionMenuLevel *actions = action_menu_level_create(2);
  action_menu_level_add_action(actions, "Run", confirm_run_cb, &s_confirm_ctx);
  action_menu_level_add_action(actions, "Cancel", confirm_cancel_cb, NULL);
  action_menu_level_add_child(root, actions, s_confirm_ctx.name);

  ActionMenuConfig config = {
    .root_level = root,
    .context = NULL,
    .colors = { .background = GColorBlack, .foreground = GColorWhite },
    .will_close = NULL,
    .did_close = action_menu_did_close,
    .align = ActionMenuAlignCenter,
  };
  action_menu_open(&config);
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
    open_confirm_menu(sc);
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
    .colors = { .background = GColorBlack, .foreground = GColorWhite },
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
    snprintf(out, out_len, "%s \xC2\xB7 %s", e->area, e->labels);
  } else if (e->area[0]) {
    snprintf(out, out_len, "%s", e->area);
  } else if (e->labels[0]) {
    snprintf(out, out_len, "%s", e->labels);
  }
}

static void edit_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                          void *callback_context) {
  ScriptEntry *e = &s_scripts[cell_index->row];
  char subtitle[96];
  build_edit_subtitle(e, subtitle, sizeof(subtitle));

  GBitmap *icon = gbitmap_create_with_resource(icon_resource(e->icon_idx));
  menu_cell_basic_draw(ctx, cell_layer, e->name[0] ? e->name : e->key,
                       subtitle[0] ? subtitle : NULL, icon);
  gbitmap_destroy(icon);

  // "✓" marker on entries already picked.
  if (shortcut_index_for_key(e->key) >= 0) {
    GBitmap *check = gbitmap_create_with_resource(RESOURCE_ID_ICON_CHECK);
    GRect bounds = layer_get_bounds(cell_layer);
    graphics_draw_bitmap_in_rect(ctx, check,
                                 GRect(bounds.size.w - 27, (bounds.size.h - 20) / 2, 20, 20));
    gbitmap_destroy(check);
  }
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

  s_edit_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_edit_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = edit_get_num_sections,
    .get_num_rows = edit_get_num_rows,
    .draw_row = edit_draw_row,
    .select_click = edit_select_cb,
  });
  menu_layer_set_click_config_onto_window(s_edit_menu, window);
  menu_layer_pad_bottom_enable(s_edit_menu, true);
  layer_add_child(root, menu_layer_get_layer(s_edit_menu));

  s_edit_status = text_layer_create(GRect(8, (bounds.size.h - 60) / 2, bounds.size.w - 16, 60));
  text_layer_set_font(s_edit_status, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_edit_status, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_edit_status, GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_edit_status, GColorClear);
  text_layer_set_text_color(s_edit_status, GColorBlack);
  layer_add_child(root, text_layer_get_layer(s_edit_status));
}

static void edit_window_unload(Window *window) {
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

// ---------------------------------------------------------------------------
// Main window
// ---------------------------------------------------------------------------

static Window *s_main_window;
static MenuLayer *s_main_menu;
static TextLayer *s_empty_text;

static uint16_t main_get_num_sections(MenuLayer *menu_layer, void *callback_context) {
  return 2;
}

static uint16_t main_get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                                  void *callback_context) {
  return section_index == SECTION_ACTIONS ? 1 : s_shortcut_count;
}

static int16_t main_get_header_height(MenuLayer *menu_layer, uint16_t section_index,
                                      void *callback_context) {
  return HEADER_HEIGHT;
}

static void main_draw_header(GContext *ctx, const Layer *cell_layer, uint16_t section_index,
                             void *callback_context) {
  menu_cell_title_draw(ctx, cell_layer,
                       section_index == SECTION_ACTIONS ? "Actions" : "Shortcuts");
}

static void main_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                          void *callback_context) {
  if (cell_index->section == SECTION_ACTIONS) {
    GBitmap *icon = gbitmap_create_with_resource(icon_resource(ICON_PLUS_IDX));
    menu_cell_basic_draw(ctx, cell_layer, "Edit shortcuts", NULL, icon);
    gbitmap_destroy(icon);
  } else {
    Shortcut *sc = &s_shortcuts[cell_index->row];
    GBitmap *icon = gbitmap_create_with_resource(icon_resource(sc->icon_idx));
    menu_cell_basic_draw(ctx, cell_layer, sc->name[0] ? sc->name : sc->key,
                         sc->area[0] ? sc->area : NULL, icon);
    gbitmap_destroy(icon);
  }
}

static void main_select_cb(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  if (cell_index->section == SECTION_ACTIONS) {
    push_edit_window();
  } else if (cell_index->row < s_shortcut_count) {
    execute_shortcut(&s_shortcuts[cell_index->row]);
  }
}

static void main_update_empty_state(void) {
  if (s_empty_text) {
    layer_set_hidden(text_layer_get_layer(s_empty_text), s_shortcut_count > 0);
  }
}

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_main_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_main_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = main_get_num_sections,
    .get_num_rows = main_get_num_rows,
    .get_header_height = main_get_header_height,
    .draw_header = main_draw_header,
    .draw_row = main_draw_row,
    .select_click = main_select_cb,
  });
  menu_layer_set_click_config_onto_window(s_main_menu, window);
  menu_layer_pad_bottom_enable(s_main_menu, true);
  layer_add_child(root, menu_layer_get_layer(s_main_menu));

  s_empty_text = text_layer_create(GRect(8, 72, bounds.size.w - 16, 60));
  text_layer_set_font(s_empty_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_empty_text, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_empty_text, GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_empty_text, GColorClear);
  text_layer_set_text_color(s_empty_text, GColorBlack);
  text_layer_set_text(s_empty_text, "No shortcuts yet\nSelect Edit shortcuts to add");
  layer_add_child(root, text_layer_get_layer(s_empty_text));
  main_update_empty_state();
}

static void main_window_unload(Window *window) {
  menu_layer_destroy(s_main_menu);
  text_layer_destroy(s_empty_text);
  s_main_menu = NULL;
  s_empty_text = NULL;
}

static void main_window_appear(Window *window) {
  menu_layer_reload_data(s_main_menu);
  main_update_empty_state();
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

#if defined(PBL_TOUCH)
  // Opt in to touch navigation so the MenuLayers scroll and activate by
  // swipe/tap on touch hardware; buttons keep working as before.
  app_touch_navigation_enable(true);
#endif

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
