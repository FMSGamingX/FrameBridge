# Input and GPU compatibility

Created by Faris Alsari / FMSGaming · Famous Gaming · 19 November 2026

**NVIDIA Optical Flow.** FrameBridge opens the D3D11 optical flow interface from the system NVIDIA display driver and verifies bidirectional initialization on each NVIDIA adapter. It registers two BGRA inputs and two signed vector maps at the input resolution's four-pixel grid. A FrameBridge compute shader reprojects both inputs into a midpoint. Resolution and resource errors are reported in the session. The build includes NVIDIA's MIT-licensed SDK 5 interface headers and license notice. The runtime comes from the installed driver. [NVIDIA Optical Flow SDK](https://developer.nvidia.com/opticalflow-sdk) · [Header source, pinned revision](https://github.com/AlanBacker/VRChat-DLSS5-Cam/tree/0ad343a576c3624c185f503b3cb15be229db213e/third_party/nvof)

**Capture devices.** FrameBridge enumerates Media Foundation video devices, including compatible UVC webcams and capture cards. The driver must expose a video stream that Windows can convert to RGB32. Native format selection is supported. A legacy DirectShow-only filter requires a separate backend. The development machine exposed a Logitech BRIO 4K Stream Edition through Media Foundation. Windows also listed an Epson imaging device in Plug and Play, which was not classified as an HDMI input. [Microsoft video capture](https://learn.microsoft.com/en-us/windows/win32/medfound/video-capture) · [Source Reader attributes](https://learn.microsoft.com/en-us/windows/win32/medfound/source-reader-attributes)

**Console networking.** Sony documents a Remote Play application for Windows. Xbox documents a consumer browser remote-play experience. The reviewed vendor material does not expose a supported independent SDK for retrieving decoded home-console frames into this application. FrameBridge therefore uses a compatible HDMI capture card for direct console input. There is no console sign-in, console application dependency, DRM bypass or verified PS5/Xbox hardware claim. [Sony PC Remote Play](https://www.playstation.com/en-gb/support/games/playstation-remote-play-on-pc-and-mac/) · [Xbox Remote Play](https://www.xbox.com/en-US/consoles/remote-play)

**DLSS Frame Generation.** NVIDIA Streamline requires renderer resources such as depth and engine motion vectors. A completed HDMI, desktop, webcam or decoded file frame lacks that data. The capture integration is disabled even on the RTX 5060. [NVIDIA DLSS Frame Generation integration guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md)

## Enable Smooth Motion

1. In FrameBridge, open **Generation → NVIDIA Smooth Motion**. This mode is offered when FrameBridge finds a Direct3D-capable NVIDIA adapter. The setup guide displays the full path to this installation's **FrameBridge.Output.exe**.
2. Open **NVIDIA App → Graphics → Program Settings**. Use the three-dot menu above the program list to add a program. Browse to the output executable shown in FrameBridge. [NVIDIA App graphics settings](https://www.nvidia.com/en-gb/geforce/news/nvidia-app-download-and-features/)
3. Select the new output program profile. Under **Driver Settings**, turn **Smooth Motion** on and apply the change if prompted. Availability depends on the GPU, installed driver and profile. [NVIDIA Smooth Motion settings](https://www.nvidia.com/en-us/geforce/news/gfecnt/20258/nvidia-app-global-dlss-overrides-rtx-40-series-smooth-motion/)
4. Choose the video source and output monitor in FrameBridge and click **Start processing**. Restart an already-running session after changing the driver profile. FrameBridge opens the output executable automatically.

The performance page counts original presentations in this mode. FrameBridge-generated presentations and motion jobs stay at zero. The driver controls any extra displayed frames. The project owner reports Smooth Motion working on their system.

**Vendor-neutral acceleration.** Traditional interpolation requires Direct3D feature level 11.0 and resource support for the engine's compute shaders. Actual resource creation is tested on startup. The RTX 5060 and integrated Radeon were tested on this machine. Intel hardware remains untested. CUDA and TensorRT are not dependencies. DXGI detection and optional DirectML are independent of NVIDIA.
