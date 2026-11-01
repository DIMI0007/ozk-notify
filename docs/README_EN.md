# OZK Notify

**Twitch, YouTube and Kick stream announcements, plus donation messages from OBS to Telegram and Discord.** The plugin runs on your computer without a project-operated server. [Русский](../README.md).

| Waiting for a stream | During a broadcast |
| --- | --- |
| ![OZK Notify home dock](screenshots/home.png) | ![Outputs, bitrate, and graphs](screenshots/monitoring.png) |

*Version 1.0.14 UI previews were made in an isolated test environment. Shown accounts and outputs are sample data.*
[Narrow 275 px dock](screenshots/home-narrow.png).

## Features

- Recognize Twitch, YouTube and Kick outputs, with a separate tile, bitrate, graph and transferred data for each service.
- Send an automatic announcement after a service confirms a live broadcast. A later second or third platform announcement and fallback links are configurable. [Verification details](AUTO_ANNOUNCEMENTS_RU.md).
- Set recipients, text, and announcement delay in the OBS dock. See the latest delivery result at a glance.
- Receive DonationAlerts and DonatePay donation notifications with sender, amount, message, and configurable thresholds. [Donation setup](../README_DONATIONS_RU.md).
- Monitor active outputs: **LIVE**, current bitrate, a graph of the last 30 minutes, and transferred data.
- OZK Stats can show Notify's measured CPU, tracked data and session traffic. Settings and donation delivery history stay local; Windows protects stored secrets.

## How the plugins work together

OZK Notify works on its own when you sign in here and configure recipients. If OZK Chat is installed, Notify may use its protected Twitch and YouTube sign-ins and its Kick channel name only to check live status. OZK Stats can read Notify's local counters; it does not send announcements.

## Install

1. Close OBS and [download the latest successful build](https://github.com/DIMI0007/ozk-notify/actions/workflows/build-test-windows.yml).
2. Copy the entire `stream-notify` folder into `C:\ProgramData\obs-studio\plugins\`. Replace the whole folder when updating.
3. Start OBS, open the **OZK Notify** dock, and configure recipients. Test buttons verify sending without a live stream.

Target: **OBS 33, Windows x64**. `BUILD_INFO.json` in the archive identifies the version and source revision. The build is currently unsigned.

Automatic links and duplicate protection still need verification during a real multistream.

[Donation guide](../README_DONATIONS_RU.md) · [GPL v2 license](../LICENSE) · [Privacy](../PRIVACY.md)

## Support the project

If these plugins help your streams, please consider a donation. Your support helps improve the plugins, fix issues and fund future development.

[Support via DonationAlerts](https://www.donationalerts.com/r/otmopozok) · [Support via DonatePay](https://donatepay.ru/don/OTMOPOZOK)

Developed by [OTMOPOZOK](https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/).
