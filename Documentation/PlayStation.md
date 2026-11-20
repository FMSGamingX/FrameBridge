# Direct PS5 LAN input · private prototype

Created by Faris Alsari / FMSGaming · Famous Gaming

## Pair your console

1. Turn on the PS5, sign in to the account you will use, and enable **Settings → System → Remote Play → Enable Remote Play**. Put the PC and console on the same LAN.
2. In FrameBridge choose **Video inputs → PS5 Network Connection → Find PS5 consoles**. Select your console, or type its IPv4 address. Discovery reports awake/rest state. Wake the console before pairing.
3. Enter that account's Base64 PSN Account ID. You can obtain it with the pinned upstream account-ID script. Run that script locally, follow its PlayStation sign-in instructions and keep the redirect URL private. The account name and email address are different identifiers. [Upstream account-ID tool](https://github.com/chiaki-ng/chiaki-ng/blob/v1.10.1/scripts/psn-account-id.py)
4. Open **Pair Device** on the PS5 and enter its fresh eight-digit code in FrameBridge. If the console user has a four-digit login passcode, enter it in the separate optional field. Click **Pair PS5**.
5. Select the requested 30 or 60 FPS input, configure generation and output, then click **Start processing**. The input page shows the stream size and received rate. Choose **Disconnect PS5** or the main Stop button to end the session.

Pairing stores the resulting registration keys and optional login passcode in the current Windows user's Credential Manager under `FrameBridge/PS5/<address>`. Removing that generic credential forgets the pairing. If the console's IP changes, pair again at the new address.

## Media and controls

The optional plugin uses chiaki-ng v1.10.1 for registration, authenticated LAN sessions and controller feedback. It requests a 1080p SDR H.264 stream and allows the console to downgrade the profile. FFmpeg decodes received packets and converts the latest decoded frame to the engine's BGRA input format. Newer decoded frames replace unread frames. A discontinuity suppresses interpolation across a lost or replaced frame. A restricted-display notification ends the session.

Opus audio plays through the default Windows wave output with four reusable buffers. XInput controller slot 1 supplies buttons, sticks and triggers while the session runs. Native DualSense USB, adaptive triggers, touchpad, PS button, haptics, microphone, WAN traversal and synchronized audiovisual latency measurement are outside this prototype.

FrameBridge loads the plugin from **bin/PlayStation**. The base build and other inputs work independently. The upstream session API uses `LicenseRef-AGPL-3.0-only-OpenSSL`. This integration is private pending a distribution and license review. Changes in PS5 firmware may affect unofficial clients. [chiaki-ng source](https://github.com/chiaki-ng/chiaki-ng/tree/v1.10.1) · [License](https://github.com/chiaki-ng/chiaki-ng/blob/v1.10.1/COPYING) · [Sony pairing instructions](https://www.playstation.com/en-us/support/games/playstation-remote-play-on-pc-and-mac/)

## Development

Use MSYS2 UCRT64. Install the following signed packages in that environment.

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,curl,json-c,libevent,miniupnpc,openssl,opus,pkgconf,protobuf,python-protobuf,ffmpeg}
```

Run `Tools/Build-PlayStation.ps1 -Test` in PowerShell, passing `-MsysRoot` for a custom installation. The script checks the pinned upstream revision, builds the optional module, follows its PE imports to stage runtime DLLs and exercises the H.264 packet-to-BGRA path using a generated clip. Run `Build.ps1 -Test` for the main application. Live acceptance requires pairing on a real console and checking picture, audio, controller response, disconnect and reconnect.
