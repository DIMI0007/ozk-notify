# Privacy policy

OZK Notify runs locally as an OBS Studio plugin. The project does not operate a relay server, analytics service, or account database for plugin users.

## Connections requested by the user

When a user enables and configures a service, the plugin connects directly to that service:

- Telegram: sends configured announcement or manual message text and the destination chat identifier to Telegram's Bot API using the user's bot token.
- Discord: sends configured message text to the user's Discord webhook. Optional @everyone mentions are controlled by the user.
- DonationAlerts: uses the user's OAuth access token to identify the account and subscribe to donation events. Donation data can be included in outgoing messages if configured by the user.
- DonatePay: the experimental integration requests donation transactions using the user's API key when explicitly enabled.
- Currency rates: the donation feature can download public exchange-rate data from https://www.cbr-xml-daily.ru/daily_json.js. This request exposes the user's normal network connection information to that provider.

The providers receive the data necessary to process these requests and apply their own privacy policies: [Telegram](https://telegram.org/privacy), [Discord](https://discord.com/privacy), [DonationAlerts](https://www.donationalerts.com/), and [DonatePay](https://donatepay.ru/).

The plugin does not send credentials or donation data to the project maintainer, GitHub, or SignPath. Clicking help or project links opens the corresponding website in the user's browser.

## Local storage

Settings, delivery history, donation identifiers and delivery status are stored in the user's OBS configuration directory. Credentials use Windows DPAPI protection. Output and resource statistics are displayed locally; the resource counters do not upload telemetry. The local files remain until the user removes them.

## Contact and diagnostics

The public [GitHub issue tracker](https://github.com/DIMI0007/ozk-notify/issues) can be used for support. Do not post bot tokens, webhook URLs, OAuth tokens, widget tokens, private messages or unredacted configuration files. Logs and screenshots supplied by users may contain private data and should be reviewed before sharing.
