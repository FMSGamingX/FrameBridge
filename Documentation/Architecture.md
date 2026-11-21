# Architecture

Created by Faris Alsari / FMSGaming · Famous Gaming · version 0.1.0

## Data flow

```text
MF file / MF capture device / GDI desktop / synthetic clock
                       |
                 capture worker
                       |
          source PTS + monotonic arrival time
                       |
          one or two pending frames + reusable pool
                       |
                processing worker
                       |
            original pair + generator
                       |
         scheduled midpoint, then current original
                       |
                shared GPU texture or mapped CPU frame
                       |
          FrameBridge.Output.exe · D3D11 draw and DXGI Present
```

**App** owns the WPF window, native child surface, output windows, per-user settings and telemetry. An x64 C ABI exposes inventory, session lifecycle, immutable statistics snapshots and a small input thumbnail for external output. UI updates run independently of frame presentation. Stop signals cancellation immediately, then joins the native workers off the WPF dispatcher so DXGI can finish window operations.

**Engine/Capture** supplies one source interface. The asynchronous Media Foundation Source Reader negotiates RGB32, observes stream changes and EOF, handles signed stride and timestamps, and opens cameras by symbolic link. The capture-device picker can query native profiles. A file is clocked to its media timestamps. Synthetic and desktop sources have a configurable acquisition clock that defaults to 30 Hz. Windows desktop capture uses a reusable top-down DIB.

**Engine/Session.cpp** owns the bounded queue and frame pool. A producer supersedes the oldest pending frame when the consumer falls behind. A discontinuity prevents interpolation across missing frames. The consumer keeps only the previous original and the current pair. Render targets and GPU textures are reused. The CPU-to-GPU upload is one full-resolution copy per original. External preview thumbnails are small, sampled independently of presentation.

**Engine/Interpolation** implements a coarse-to-fine symmetric block search in luma. The vector at each 16×16 midpoint tile points into the previous and next originals. Motion-compensated reconstruction averages the corresponding colors. A residual gate chooses an original on low-confidence matches. Fine motion, occlusions and tile edges can still produce artifacts.

**Engine/GPU** implements the same midpoint model in two Direct3D compute passes. The second pass reads the vector field produced by the first. The presenter samples the selected original or generated texture using a fullscreen triangle in a letterboxed viewport. Four asynchronous GPU timestamp slots measure motion estimation and warping without waiting on the GPU. CPU and optional AI return frames in host memory.

**Engine/Rendering** enumerates monitor modes and chooses the requested renderer. The pacer uses source timestamps, display limits and measured preparation time. It can skip late midpoints. Two-frame buffering trades additional delay for tolerance of slow processing. Original frames are kept unmodified throughout generation. Overload can still cause a capture drop. Original and generated presentation counters are explicit.

**Engine/AI** loads the optional ONNX Runtime from its versioned local install directory. The model contract validates tensor type and shape, RGB layout, timestep and returned dimensions. The selected quality caps inference width. Edge padding uses multiples of 32 for dynamic inputs.

**Engine/Backends** probes the NVIDIA driver's optical flow interface and owns the lifetime of the registered D3D11 resources. During NVIDIA interpolation, two alternating BGRA textures serve both as optical flow inputs and as original presentation frames. The driver writes forward and backward vectors, and the GPU warp shader uses them to sample each source at the estimated midpoint. The default grid covers four pixels per axis. Resolution changes recreate the session and its registrations. Skipped pairs reset temporal hints. DLSS and Smooth Motion have separate availability messages.

**Engine/Performance** reads Windows GPU Engine counters scoped to the FrameBridge process. D3D11 timestamp queries measure the graphics queue interval around submitted work. They do not isolate time spent on the dedicated optical flow engine.

## Development boundaries

This release handles SDR video without audio. HDR passthrough, color-managed HDR presentation, DirectShow-only device drivers, console-network decoding, higher-than-2× ratios and automatic online updates remain separate work. Current output targets are source cadence or one midpoint per pair, subject to the detected refresh limit. The exclusive renderer can select a reported display resolution and refresh rate. Windowed and borderless output follow the Windows desktop mode. The application uses manual local builds for updates and never polls an online release channel.

**Output process.** The main process owns capture, interpolation and UI window lifetime. FrameBridge.Output.exe owns a separate D3D11 device and immediate context, shaders, letterboxing, DXGI swapchain, fullscreen transition and Present. On hardware paths, the producer copies the scheduled texture once on the GPU into a same-adapter shared surface. A DXGI keyed mutex hands ownership to the presenter and back. Packed originals and BGRA originals have separate reusable slots. A session-specific file mapping and two events carry commands and acknowledgements. Only one presentation can be outstanding. Successful Present acknowledgements drive the application's counters. CPU and AI paths copy the finished host frame to a bounded shared mapping and upload it on the presenter. The presenter watches the producing process handle and exits on shutdown or producer exit. [DXGI keyed mutex](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgikeyedmutex)

**PlayStation LAN input** dynamically loads the optional chiaki-ng adapter for registration and streaming. The capture thread pulls the newest decoded frame with a bounded wait so that Stop can cancel acquisition. Audio playback and XInput feedback belong to the adapter. [Plugin architecture and limitations](PlayStation.md)

## Smooth Motion presentation

The main process receives or decodes the source and paces originals using source timestamps. In Smooth Motion mode it sends the source BGRA pixels through the bounded shared-memory channel. The output process uploads them on the chosen NVIDIA adapter, fits the image to the output window and presents it. The session bypasses motion estimation, inference, midpoint scheduling, dashboard thumbnail sampling and application-forced vertical sync. Program-specific driver settings belong to FrameBridge.Output.exe.
