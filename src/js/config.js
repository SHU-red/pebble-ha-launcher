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
        'defaultValue': 'Behavior'
      },
      {
        'type': 'toggle',
        'messageKey': 'ConfirmEnabled',
        'label': 'Confirm before executing'
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
        'defaultValue': 198
      }
    ]
  },
  {
    'type': 'submit',
    'defaultValue': 'Save Settings'
  }
];
