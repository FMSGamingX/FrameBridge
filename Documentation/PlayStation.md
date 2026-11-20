# PS5 network input feasibility

Created by Faris Alsari / FMSGaming · Famous Gaming

A direct LAN client is technically feasible. PXPlay documents registration on the same network using the console's displayed pairing code and the user's PlayStation account identity. Sony documents enabling Remote Play, pairing and using its consumer client. We did not identify a public Sony SDK granting third-party Windows clients a supported protocol contract. PXPlay's manual describes user-visible behavior and supplies no implementation license or protocol specification. [PXPlay registration](https://streamingdv.github.io/pxplay/) · [Sony pairing instructions](https://www.playstation.com/en-gb/support/games/playstation-remote-play-on-pc-and-mac/)

The upstream chiaki-ng project implements an independent client. Its library covers discovery, registration, session establishment, feedback and media reception. It exposes video samples to its FFmpeg decoder and receives audio through an Opus path. FrameBridge could use a private, optional LAN backend with the following boundaries. [Upstream architecture](https://github.com/chiaki-ng/chiaki-ng/blob/main/CONTRIBUTOR_GUIDE.md)

| Area | Proposed integration |
|---|---|
| Discovery and pairing | Discover local consoles, allow an explicit address, request the user-visible pairing code and account ID, and report rejection or expiry. |
| Authentication and session | Reuse the upstream authenticated registration/session APIs. Store registration keys in Windows Credential Manager or DPAPI per user. Keep codes and keys out of logs and Git. |
| Video | Feed decoded frames with timestamps into the native Source interface. Reset interpolation on lost frames, resolution changes and reconnect. |
| Audio | Queue decoded samples in a bounded WASAPI renderer and synchronize to the video timeline. |
| Controller | Map a selected Windows controller to authenticated session feedback, clear held inputs on disconnect and test rumble separately. |
| Monitoring | Report receiving, decoder readiness, actual resolution, observed frame intervals, queue delay, losses and disconnect cause. Measure interactive latency with physical hardware. |

Firmware changes can interrupt interoperability. Upstream's session header uses an AGPL-3.0-only license with an OpenSSL exception. Before integrating or distributing a combined application, decide how FrameBridge will satisfy applicable source, notice and licensing obligations and review any applicable platform terms. An IPC boundary alone does not resolve those questions. The upstream license covers upstream code. Sony authorization is a separate question. [Session header](https://github.com/chiaki-ng/chiaki-ng/blob/main/lib/include/chiaki/session.h) · [Upstream license](https://github.com/chiaki-ng/chiaki-ng/blob/main/COPYING)

The PS5 Network Connection source sends a short LAN discovery request on active IPv4 interfaces when the user selects Find PS5 consoles. It displays responding consoles and their advertised awake or rest state. It does not collect pairing details. Session establishment and decoding await a backend decision. [Discovery request format](https://github.com/chiaki-ng/chiaki-ng/blob/main/lib/src/discovery.c) Hardware acceptance requires owner-authorized registration, real picture and audio, controller response, reconnect, disconnect and confirmation that restricted content stays protected.
