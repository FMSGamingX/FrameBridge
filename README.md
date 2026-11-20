# FrameBridge

Created by Faris Alsari / FMSGaming · Famous Gaming · version 0.1.0

A local x64 Windows application for previewing and interpolating external video. Open **bin/FrameBridge.exe**, leave **Synthetic motion test** selected, then choose **Start processing**. The default is HUD-aware optical flow with automatic GPU selection. The Generation page also offers classic optical flow, a wide motion search preset and optional RIFE. The image in standby is a reference frame from the test generator.

## Try a real source

- **Local video** — open **Video inputs → Video file → Browse video** and select a local file. The included development fixture is **Tests/results/motion-30.avi**. Start processing and watch the original, generated and presentation counters in Performance. A four-second, 120-frame file has 119 adjacent pairs, hence 239 possible presentations at 2×. The session ends at EOF.
- **Webcam or HDMI capture card** — select **Capture card / webcam**, choose the Windows device, and optionally read its native formats. Start with the device default. Windows camera privacy settings apply. The BRIO webcam has been exercised locally.
- **Desktop** — choose a monitor under **Desktop capture**. A separate output on another monitor avoids capturing the preview recursively. Choose the capture cadence under **Input FPS**. Synthetic and desktop capture default to 30 fps.
- **External monitor** — select windowed output, borderless fullscreen or an enumerated DXGI exclusive mode under **Output**. Escape closes an external output. Exclusive modes require a compatible hardware adapter attached to that monitor. Stop the session before changing source or output configuration.

For HDMI console input, connect the console's output to a compatible capture card's HDMI input and attach that device to the PC. PC graphics-card HDMI ports transmit video. FrameBridge supports the Windows capture path. PS5 and Xbox hardware validation remains outstanding. Protected content remains subject to the source and driver restrictions.

## Processing modes

**Traditional interpolation** uses a tile-based optical-flow estimate and motion-compensated midpoint reconstruction. Original frames go directly to the output path. Direct3D 11 compute runs on feature-level 11 hardware. A CPU implementation is selectable. Manual GPU selection surfaces initialization errors. Automatic selection tries hardware adapters in dedicated-memory order before CPU fallback.

**AI interpolation** runs a compatible RIFE ONNX model through ONNX Runtime with DirectML or CPU. The optional local installer verifies its model and library downloads. Select the model in Settings and the mode in Generation. The inference width controls the cost and resolution of the generated frame. Model inference time and missed presentation deadlines depend on the machine. The original input retains its resolution. See [model setup and provenance](Documentation/Models.md).

**NVIDIA Optical Flow** uses a compatible NVIDIA GPU's optical flow accelerator to track motion between two video frames. Choose it under **Generation → Generation mode**, then select Performance, Balanced or Quality. The optional occlusion setting gives more weight to the frame with consistent motion where objects overlap. Set **Output FPS → 60 fps** for a 30 fps source on a display running at 60 Hz or higher. The Performance page counts presented midpoints.

**NVIDIA DLSS Frame Generation** and **NVIDIA Smooth Motion** have independent capability explanations in the app. See [compatibility research](Documentation/Compatibility.md).

## Choosing frame rate and resolution

For a 75 Hz display, choose **37.5 fps** under synthetic or desktop input, select **Fullscreen · selected display mode** and a supported 75 Hz monitor timing, and choose **75 fps** under Output. Each adjacent pair supplies one midpoint. A 30 fps file supports 30 or 60 fps. Files follow their recorded timestamps, and capture devices use the native rates listed in their format picker. Available output rates depend on the selected input rate, generation setting and display.

**Separate output window** offers exact client-area resolutions that fit the monitor, including 1280×720 on a 1080p desktop. **Fullscreen · selected display mode** temporarily switches the selected monitor to its chosen Windows resolution and refresh, then restores the previous mode when the session ends. **DXGI exclusive fullscreen** requests a driver-owned mode. Windows can choose a different refresh in that mode. **Borderless fullscreen** fills the current desktop mode.

HUD-aware interpolation checks stationary strokes for matching color in both originals before accepting motion from the surrounding tile. This targets crosshairs and text over moving backgrounds. Semi-transparent overlays and occluded objects can still produce artifacts.

## Understanding measurements

Input rate counts frames admitted to the engine. Output rate counts successful original and generated presentations. An accepted presentation can be coalesced later by Windows or the monitor. Game simulation runs at the source's own rate. Latency measures from a decoded frame's admission to the return of its original presentation call. Optical capture-to-photon latency requires external timing equipment.

The Performance page reports processing work, midpoint preparation, GPU kernel duration when timestamp queries complete, the process's busiest measured GPU engine, process CPU, working set and dropped frames. The graph retains the most recent 60 half-second samples. Startup and unsupported counters show N/A. Stop and EOF keep the last session measurements for review.

## Development

- [Build and test](Documentation/Build.md)
- [Architecture and boundaries](Documentation/Architecture.md)
- [Local validation](Documentation/Validation.md)

The original project's open-source license decision is deferred. Third-party code and model files retain their own copyright and license terms. See **ThirdParty/Notices**, **ThirdParty/ONNXRuntime/LICENSE**, and the licenses installed alongside the optional inference runtime and model.

## Dedicated output and network input

FrameBridge starts **bin/FrameBridge.Output.exe** automatically when processing begins. Add this executable under NVIDIA App → Graphics → Program Settings to experiment with Smooth Motion. For an isolated test, turn off FrameBridge frame generation and play a 30 fps file on the second monitor. A successful driver profile change or DXGI Present does not verify driver-generated frames. Use independent display measurements. [Build and test](Documentation/Build.md)

Select **Video inputs → PS5 Network Connection → Find PS5 consoles** for LAN discovery. Pairing and network streaming are pending. [PlayStation research](Documentation/PlayStation.md) · [Development workflow](Documentation/Development.md)
