# Local validation · 19 November 2026

FrameBridge 0.1.0 · Created by Faris Alsari / FMSGaming · Famous Gaming

Tests ran on Windows 11 Pro 25H2, build 26200.9974. The machine has an AMD Ryzen 5 7600X, NVIDIA GeForce RTX 5060 (driver 621.24), AMD Radeon integrated graphics (Windows driver 32.0.13018.6), and a Logitech BRIO 4K Stream Edition. Windows reported two active 1920×1080 outputs at 59 and 240 Hz. The application queries the monitor and enumerates supported exclusive modes on the running system.

## Traditional interpolation

All rows below use the program's synthetic 1920×1080, 30 fps moving targets, Balanced quality and a 60 Hz test limit. The test presents into a 512×288 native window. These are short development measurements of processing at 1080p with scaled presentation. They do not establish sustained full-screen scanout performance. Active presentation rate excludes device startup. The counter snapshot can include an input whose original is still pending when the timed run ends.

| Backend | Active test | Input frames admitted | Presentations | Generated | Reported drops | Active presentations/s | Mean pacing error |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Automatic → RTX 5060 | 10 s | 301 | 600 | 300 | 0 | 59.9 | 0.38 ms |
| Radeon integrated GPU | 7 s | 211 | 421 | 210 | 0 | 60.0 | 0.69 ms |
| Ryzen CPU | 7 s | 211 | 419 | 209 | 0 | 59.7 | 0.39 ms |

Pacing error is the mean absolute difference between successive successful presentation intervals and the nominal target interval. GPU timestamp queries reported 0.14 ms of motion-estimation and reconstruction work on the RTX and 2.38 ms on the Radeon for these runs. CPU preparation on the software run averaged 9.12 ms. The metrics use different boundaries and should not be compared as full-pipeline processing costs.

The deterministic translation regression exercised all three implementations. An eight-pixel displacement requires a four-pixel midpoint translation. The CPU result and GPU texture readbacks on both adapters matched all **3,072 checked interior pixels** exactly. The tests also compare the untouched original pixels, and check static and single-pixel inputs on CPU.

## Inputs and output

- **Local AVI** — Media Foundation decoded all 120 originals. Each backend presented 239 frames, including 119 midpoints, then reported EOF. An absent file produced a visible error and a clean shutdown.
- **Webcam** — FrameBridge enumerated the BRIO's formats and selected its 1920×1080, 30 fps NV12 profile. The physical run admitted 208 frames and submitted 413 presentations, including 206 midpoints, with zero engine-reported drops. Measured active output was 51.5 presentations/s. The negotiated 30 fps is a device format. Delivery varied during this test.
- **Desktop** — a 320×180 real desktop region was captured and processed through the same native pipeline. The application exposes complete monitor capture as its normal input selection.
- **WPF** — all seven pages opened. The synthetic dashboard processed and presented frames. The app stopped and restarted for a separate output window, borderless fullscreen and the monitor's current-resolution exclusive fullscreen. The exclusive test exited and restored desktop presentation.
- **Hardware limitations** — no HDMI capture card, PlayStation, Xbox or Intel graphics adapter was available. Physical capture-card interoperability, console behavior, Intel performance, HDR presentation and optical end-to-end latency remain unverified.

## Optional AI

The pinned RIFE 4.9 model ran on both ONNX Runtime DirectML and CPU. These synthetic tests used a **320×180 source**, an inference width capped at **256 pixels**, and two-frame buffering. The six-second RTX test submitted 350 presentations with 174 generated midpoints and four drops, averaging 58.2 active presentations/s. The five-second CPU test submitted 166 presentations with 63 generated midpoints and 47 drops, averaging 33.0 presentations/s. A missing model produced a model-loading error for each backend. These measurements do not establish 1080p AI inference throughput.

## NVIDIA Optical Flow

The RTX 5060 ran all three optical flow presets. In the eight-pixel translation fixture the central crop contained 3,072 pixels. Balanced and Quality reproduced all 3,072 expected midpoint pixels exactly. Performance had a mean absolute grayscale error of 0.46 on a 0–255 scale. All three preserved the original input and the repeated-frame fixture.

The wider color-pan fixture moves a reproducible texture by 40 pixels per frame. Compared against the known 20-pixel midpoint, mean absolute channel error in the central crop was 76.39 for traditional Balanced and 0.00 (rounded) for NVIDIA Balanced. These fixtures measure translation accuracy. Gameplay HUDs, scene cuts and overlapping objects require separate evaluation.

For live session timings and display restoration, see the [secondary-monitor test output](../Tests/results/nvidia-suite.txt).

## Reproduction

Run the commands in [Build.md](Build.md). The development fixtures and logs are under **Tests/results**. The final verification includes **final-auto.txt**, **ui-test.txt**, and local dashboard and performance screenshots. Source pixels from the webcam and desktop tests are processed in memory.

## Dedicated presenter · 20 November 2026

On the secondary monitor, the 120-frame video fixture produced 239 acknowledged presentations including 119 generated midpoints in the NVIDIA Optical Flow run. An originals-only run produced 120 presentations and zero FrameBridge midpoints. A FrameBridge.Output.exe process was observed during streaming. Windowed and borderless output, Windows display-mode fullscreen and DXGI exclusive all presented frames. Windows reported 74.973 Hz for the selected display mode and 60 Hz in the exclusive test. The test harness restored the previous desktop mode.

The driver-generated Smooth Motion result is unverified. NVIDIA App's installed MCP bridge exposed no per-program Smooth Motion control. The bundled PresentMon did not return frame-type samples. Windows denied the requested DXGI ETW trace at this account's current privilege level. To compare on/off, manually select the helper's profile, disable FrameBridge generation, restart processing for each driver setting and compare independently measured displayed frames on the same moving clip. App counters alone cannot detect extra frames inserted after Present. [NVIDIA App instructions](https://www.nvidia.com/en-us/geforce/news/gfecnt/20251/nvidia-app-update-dlss-overrides-and-more/)
