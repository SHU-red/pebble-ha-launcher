/**
 * HA Launcher — PebbleKit JavaScript.
 *
 * Bridges the watch app to Home Assistant:
 *  - Execute: POST /api/services/script/<key>
 *  - Browse:  POST /api/template (Jinja) to list script entities
 *  - Config:  Clay page (documented defaults) — the phone app delivers the
 *             settings to the WATCH, which persists them in its flash; this
 *             JS pulls the config back from the watch on 'ready'.
 *
 * All watch-bound results travel over the ResultCode/ResultText keys so the C
 * side can surface every failure in-app.
 */

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var messageKeys = require('message_keys');

var CONFIG_KEY = 'haConfig';
var CLAY_SETTINGS_KEY = 'clay-settings';
var MAX_SHORTCUTS = 64;
var EXECUTE_TIMEOUT_MS = 10000;
var BROWSE_TIMEOUT_MS = 20000;

// Clay handles showConfiguration/webviewclosed itself (the documented
// default): on save it normalizes the response ({value: ...} wrapping), writes
// 'clay-settings' for page prefill, and sends every messageKey value to the
// WATCH via AppMessage. The watch persists them durably in its flash; this JS
// pulls the durable copy back on 'ready'. No manual event handling needed.
var clay = new Clay(clayConfig);

// Monotonic generation counter for browse requests: if the user re-enters the
// edit window while a previous fetch chain is still streaming, the stale chain
// is dropped instead of interleaving with the newer one.
var fetchGeneration = 0;

// Jinja template for browsing: one line per script entity, fields joined by '|'.
// entity_id|name|area|labels|icon
var SCRIPT_BROWSE_TEMPLATE =
  "{% for s in states.script %}{{ s.entity_id }}|{{ s.name }}|" +
  "{{ area_name(s.entity_id) or '' }}|{{ s.labels | join(',') }}|" +
  "{{ s.attributes.icon or '' }}\n{% endfor %}";

/**
 * Curated mdi icon names, in the same order as pebble.resources.media in
 * package.json. Array index == icon resource index (0 == generic/script-text).
 */
var MDI_ICONS = [
  'script-text',        // 0
  'lightbulb',          // 1
  'lightbulb-on',       // 2
  'lock',               // 3
  'lock-open',          // 4
  'home',               // 5
  'power',              // 6
  'play',               // 7
  'pause',              // 8
  'stop',               // 9
  'bell',               // 10
  'bell-ring',          // 11
  'alarm',              // 12
  'timer',              // 13
  'clock',              // 14
  'calendar',           // 15
  'calendar-check',     // 16
  'door',               // 17
  'garage',             // 18
  'garage-open',        // 19
  'camera',             // 20
  'video',              // 21
  'motion-sensor',      // 22
  'thermometer',        // 23
  'weather-sunny',      // 24
  'weather-night',      // 25
  'water',              // 26
  'water-outline',      // 27
  'fire',               // 28
  'fan',                // 29
  'air-conditioner',    // 30
  'radiator',           // 31
  'speaker',            // 32
  'television',         // 33
  'volume-high',        // 34
  'music-note',         // 35
  'phone',              // 36
  'message',            // 37
  'email',              // 38
  'account',            // 39
  'key',                // 40
  'shield',             // 41
  'shield-check',       // 42
  'eye',                // 43
  'check',              // 44
  'close',              // 45
  'plus',               // 46
  'minus',              // 47
  'star',               // 48
  'heart',              // 49
  'leaf',               // 50
  'car',                // 51
  'light-switch',       // 52
  'power-plug',         // 53
  'remote',             // 54
  'bluetooth',          // 55
  'wifi',               // 56
  'cloud',              // 57
  'refresh',            // 58
  'cog',                // 59
  'alert',              // 60
  'information',        // 61
  'flash',              // 62
  'pin',                // 63
  'map-marker',         // 64
  'robot',              // 65
  'lamp',               // 66
  'window-closed',      // 67
  'blinds',             // 68
  'washing-machine',    // 69
  'fridge',             // 70
  'coffee',             // 71
  'doorbell',           // 72
  'cctv',               // 73
  'alarm-light-outline',// 74
  'smoke-detector',     // 75
  'solar-power',        // 76
  'bank',               // 77
  'currency-eur',       // 78
  'piggy-bank',         // 79
  'gamepad',            // 80
  'gauge',              // 81
  'weather-sunset-up',  // 82
  'weather-cloudy',     // 83
  'weather-rainy',      // 84
  'weather-fog',        // 85
  'bell-off',           // 86
  'shuffle',            // 87
  'repeat',             // 88
  'arrow-up',           // 89
  'arrow-down'          // 90
];

var MDI_ICON_INDEX = {};
for (var i = 0; i < MDI_ICONS.length; i++) {
  MDI_ICON_INDEX[MDI_ICONS[i]] = i;
}

/**
 * Map a Home Assistant mdi icon name to a curated icon resource index.
 * Strips the 'mdi:' prefix, lowercases and normalizes separators; unknown or
 * empty names map to 0 (generic).
 * @param {string} mdiName
 * @return {number}
 */
function mdiToIndex(mdiName) {
  if (!mdiName) {
    return 0;
  }
  var name = String(mdiName).replace(/^mdi:/, '').toLowerCase();
  var idx = MDI_ICON_INDEX[name];
  return (typeof idx === 'number') ? idx : 0;
}

/**
 * Normalize a stored base URL: trim whitespace, drop a single trailing slash
 * and strip a legacy '/api/services/script' suffix if present.
 * @param {string} url
 * @return {string}
 */
function normalizeBaseUrl(url) {
  if (!url) {
    return '';
  }
  url = url.trim();
  var suffix = '/api/services/script';
  if (url.length > suffix.length && url.slice(-suffix.length) === suffix) {
    url = url.slice(0, url.length - suffix.length);
  }
  if (url.charAt(url.length - 1) === '/') {
    url = url.slice(0, -1);
  }
  return url;
}

/**
 * Load the phone-side configuration from localStorage.
 * @return {{baseUrl: string, token: string, confirm: number}}
 */
function loadConfig() {
  var config = { baseUrl: '', token: '', confirm: 0 };
  try {
    var raw = localStorage.getItem(CONFIG_KEY);
    if (raw) {
      var parsed = JSON.parse(raw);
      config.baseUrl = normalizeBaseUrl(parsed.baseUrl || '');
      config.token = parsed.token || '';
      config.confirm = parsed.confirm ? 1 : 0;
    }
  } catch (err) {
    console.log('loadConfig: failed to read stored config: ' + err);
  }
  return config;
}

/**
 * Persist the phone-side configuration to localStorage.
 * @param {{baseUrl: string, token: string, confirm: number}} config
 */
function saveConfig(config) {
  try {
    localStorage.setItem(CONFIG_KEY, JSON.stringify(config));
  } catch (err) {
    console.log('saveConfig: failed to persist config: ' + err);
  }
}

/**
 * Send an execute/fetch result to the watch over ResultCode/ResultText.
 * @param {number} code
 * @param {string} text
 */
function sendResult(code, text) {
  var dict = {};
  dict.ResultCode = code;
  dict.ResultText = text;
  Pebble.sendAppMessage(dict, function() {
    console.log('sendResult: ' + code + ' ' + text);
  }, function(err) {
    console.log('sendResult: failed to deliver (' + code + '): ' + JSON.stringify(err));
  });
}

/**
 * Execute a script: POST {baseUrl}/api/services/script/<key>.
 * @param {string} scriptKey - key WITHOUT the 'script.' prefix
 */
function executeScript(scriptKey) {
  var config = loadConfig();
  if (!config.baseUrl || !config.token) {
    console.log('executeScript: no config, aborting');
    sendResult(0, 'No config');
    return;
  }

  var xhr = new XMLHttpRequest();
  xhr.open('POST', config.baseUrl + '/api/services/script/' + encodeURIComponent(scriptKey), true);
  xhr.setRequestHeader('Authorization', 'Bearer ' + config.token);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.timeout = EXECUTE_TIMEOUT_MS;
  xhr.onload = function() {
    if (xhr.status === 200) {
      sendResult(200, 'OK');
    } else {
      sendResult(xhr.status, 'HTTP ' + xhr.status);
    }
  };
  xhr.onerror = function() {
    sendResult(0, 'Net error');
  };
  xhr.ontimeout = function() {
    sendResult(0, 'Timeout');
  };
  xhr.send('{}');
}

/**
 * Browse scripts: POST {baseUrl}/api/template with the script-listing template.
 */
function fetchScripts() {
  var config = loadConfig();
  if (!config.baseUrl || !config.token) {
    console.log('fetchScripts: no config, aborting');
    sendResult(0, 'Fetch error');
    return;
  }

  var generation = ++fetchGeneration;

  var xhr = new XMLHttpRequest();
  xhr.open('POST', config.baseUrl + '/api/template', true);
  xhr.setRequestHeader('Authorization', 'Bearer ' + config.token);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.timeout = BROWSE_TIMEOUT_MS;
  xhr.onload = function() {
    if (generation !== fetchGeneration) {
      return; // stale: a newer browse superseded this one
    }
    if (xhr.status === 200) {
      handleBrowseResponse(xhr.responseText, generation);
    } else {
      sendResult(0, 'Fetch error');
    }
  };
  xhr.onerror = function() {
    if (generation === fetchGeneration) {
      sendResult(0, 'Fetch error');
    }
  };
  xhr.ontimeout = function() {
    if (generation === fetchGeneration) {
      sendResult(0, 'Fetch error');
    }
  };
  xhr.send(JSON.stringify({ template: SCRIPT_BROWSE_TEMPLATE }));
}

/**
 * Parse the browse response ('entity_id|name|area|labels|icon' per line) and
 * stream the results to the watch: ShortcutCount first, then one message per
 * entry. Malformed lines are skipped; the list is capped at MAX_SHORTCUTS.
 * @param {string} responseText
 */
function handleBrowseResponse(responseText, generation) {
  var scripts = [];
  var lines = responseText.split('\n');

  for (var i = 0; i < lines.length; i++) {
    var line = lines[i];
    if (!line) {
      continue;
    }
    var parts = line.split('|');
    if (parts.length < 5) {
      // Malformed line (missing fields) — skip.
      console.log('browse: skipping malformed line: ' + line);
      continue;
    }
    var entityId = parts[0].trim();
    if (entityId.indexOf('script.') !== 0) {
      // Not a script entity_id — cannot derive a key.
      console.log('browse: skipping non-script entity: ' + entityId);
      continue;
    }
    var key = entityId.slice('script.'.length);
    var name = parts[1] || key;
    scripts.push({
      key: key,
      name: name,
      area: parts[2],
      labels: parts[3],
      icon: mdiToIndex(parts[4])
    });
    if (scripts.length >= MAX_SHORTCUTS) {
      break;
    }
  }

  sendBrowseResults(scripts, generation);
}

/**
 * Send the browse results: {ShortcutCount: n} first, then one
 * {ScriptName, ScriptKey, ScriptArea, ScriptLabels, ScriptIcon} message per
 * entry, chained through the ack callback to respect the app message queue.
 * @param {Array<Object>} scripts
 * @param {number} generation - fetch generation; a stale chain aborts
 */
function sendBrowseResults(scripts, generation) {
  if (generation !== fetchGeneration) {
    console.log('browse: stale chain dropped');
    return;
  }
  console.log('browse: sending ' + scripts.length + ' script(s)');
  var dict = {};
  dict.ShortcutCount = scripts.length;
  Pebble.sendAppMessage(dict, function() {
    sendScriptEntry(scripts, 0, generation);
  }, function(err) {
    console.log('browse: failed to send ShortcutCount: ' + JSON.stringify(err));
  });
}

/**
 * Send one script entry, then the next, chained on success.
 * @param {Array<Object>} scripts
 * @param {number} index
 * @param {number} generation - fetch generation; a stale chain aborts
 */
function sendScriptEntry(scripts, index, generation) {
  if (generation !== fetchGeneration || index >= scripts.length) {
    return;
  }
  var script = scripts[index];
  var dict = {};
  dict.ScriptName = script.name;
  dict.ScriptKey = script.key;
  dict.ScriptArea = script.area;
  dict.ScriptLabels = script.labels;
  dict.ScriptIcon = script.icon;
  Pebble.sendAppMessage(dict, function() {
    sendScriptEntry(scripts, index + 1, generation);
  }, function(err) {
    console.log('browse: failed to send entry ' + script.key + ': ' + JSON.stringify(err));
  });
}

/**
 * Read a payload field by messageKey name, tolerating both the string-name
 * form (multi-JS) and the numeric-key form.
 * @param {Object} payload
 * @param {string} keyName
 * @return {*}
 */
function payloadValue(payload, keyName) {
  if (payload[keyName] !== undefined) {
    return payload[keyName];
  }
  return payload[messageKeys[keyName]];
}

/**
 * Parse the webviewclosed response, which may arrive as JSON or as a
 * URL-encoded JSON string.
 * @param {string} response
 * @return {Object}
 */
/**
 * Recover the config from Clay's persisted settings (written by Clay's own
 * getSettings when a save is delivered, and by this JS when the watch
 * replies). Acts as a prefill/cache layer; the watch's flash is the source of
 * truth.
 */
function importClaySettings() {
  try {
    var raw = localStorage.getItem(CLAY_SETTINGS_KEY);
    if (!raw) return;
    var s = JSON.parse(raw);
    function plain(k) {
      var v = s[k];
      return (v && typeof v === 'object' && 'value' in v) ? v.value : v;
    }
    var cfg = loadConfig();
    var changed = false;
    if (plain('BaseUrl')) { cfg.baseUrl = normalizeBaseUrl(plain('BaseUrl')); changed = true; }
    if (plain('Token')) { cfg.token = plain('Token'); changed = true; }
    if (plain('ConfirmEnabled') !== undefined) {
      cfg.confirm = plain('ConfirmEnabled') ? 1 : 0;
      changed = true;
    }
    if (changed) {
      saveConfig(cfg);
      console.log('ready: recovered config from clay-settings');
    }
  } catch (err) {
    console.log('ready: clay-settings import failed: ' + err);
  }
}

Pebble.addEventListener('ready', function() {
  console.log('JS ready');
  importClaySettings();
  // Pull the durable config from the watch (it persists what Clay sent).
  var msg = {};
  msg.RequestConfig = 1;
  Pebble.sendAppMessage(msg, function() {
    console.log('ready: requested config from watch');
  }, function(err) {
    console.log('ready: config request failed: ' + JSON.stringify(err));
  });
});

Pebble.addEventListener('appmessage', function(e) {
  var payload = e.payload || {};

  // Config reply from the watch (its durable copy of what Clay sent it).
  var baseUrl = payloadValue(payload, 'BaseUrl');
  var token = payloadValue(payload, 'Token');
  if (baseUrl !== undefined && baseUrl !== null) {
    var confirmVal = payloadValue(payload, 'ConfirmEnabled');
    var cfg = loadConfig();
    cfg.baseUrl = normalizeBaseUrl(String(baseUrl));
    cfg.token = (token !== undefined && token !== null) ? String(token) : (cfg.token || '');
    cfg.confirm = (confirmVal !== undefined && confirmVal !== null) ? (confirmVal ? 1 : 0) : cfg.confirm;
    saveConfig(cfg);
    try {
      localStorage.setItem(CLAY_SETTINGS_KEY, JSON.stringify({
        BaseUrl: cfg.baseUrl,
        Token: cfg.token,
        ConfirmEnabled: cfg.confirm ? true : false
      }));
    } catch (err) { /* best effort */ }
    console.log('appmessage: config from watch saved');
    return;
  }

  var scriptKey = payloadValue(payload, 'ScriptKey');

  if (scriptKey !== undefined && scriptKey !== null && scriptKey !== '') {
    console.log('appmessage: executing script');
    executeScript(String(scriptKey));
    return;
  }

  var fetchFlag = payloadValue(payload, 'FetchScripts');
  if (fetchFlag !== undefined) {
    console.log('appmessage: fetching scripts');
    fetchScripts();
    return;
  }

  console.log('appmessage: unrecognized payload');
});
