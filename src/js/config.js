/**
 * HA Launcher — Clay configuration page definition.
 *
 * Built with @rebble/clay. Every messageKey matches an entry of the
 * "messageKeys" array in package.json so values can be transmitted to the
 * watch (ConfirmEnabled) or kept phone-side only (BaseUrl, Token).
 */

module.exports = [
  {
    'type': 'heading',
    'defaultValue': 'HA Launcher'
  },
  {
    'type': 'section',
    'items': [
      {
        'type': 'heading',
        'defaultValue': 'Connection'
      },
      {
        'type': 'input',
        'messageKey': 'BaseUrl',
        'label': 'Home Assistant URL',
        'attributes': {
          'placeholder': 'http://192.168.178.55:8123'
        }
      },
      {
        'type': 'input',
        'messageKey': 'Token',
        'label': 'Long-Lived Access Token',
        'attributes': {
          'type': 'password'
        }
      }
    ]
  },
  {
    'type': 'section',
    'items': [
      {
        'type': 'heading',
        'defaultValue': 'Appearance'
      },
      {
        'type': 'color',
        'messageKey': 'AccentColor',
        'label': 'Accent color',
        'defaultValue': 21930  // 0x0055AA = GColorCobaltBlue (24-bit RGB)
      },
      {
        'type': 'select',
        'messageKey': 'DarkMode',
        'label': 'Appearance',
        'options': [
          { 'label': 'Dark', 'value': 1 },
          { 'label': 'Light', 'value': 0 }
        ],
        'defaultValue': 1
      },
      {
        'type': 'toggle',
        'messageKey': 'TouchEnabled',
        'label': 'Touch',
        'description': 'Off while the native touch layer has bugs (PebbleOS #1865).\nTurn on once the watch firmware update ships.',
        'defaultValue': false
      }
    ]
  },
  {
    'type': 'submit',
    'defaultValue': 'Save Settings'
  }
];
