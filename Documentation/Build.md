# Building and testing FrameBridge

Created by Faris Alsari / FMSGaming · Famous Gaming · version 0.1.0

The executable targets 64-bit Windows with .NET Framework 4.8 or later and the Windows Media Foundation desktop components. The verified host has .NET Framework 4.8.1. Windows N editions may need Microsoft's Media Feature Pack. The shell builds with the installed Framework C# compiler and WPF assemblies. The native engine builds as C++20 with Clang, LLVM-MinGW and its Windows import libraries. Visual Studio, CUDA, a modern dotnet SDK and either console application are optional for this build.

From the **FrameBridge** directory in PowerShell:

```powershell
# First build on a new development machine
.\Tools\Install-Compiler.ps1

# Build, compile the HLSL shaders, run the motion regression and launch the shell test
.\Build.ps1 -Test

# Launch the local application
.\bin\FrameBridge.exe
```

The compiler installer downloads the pinned maintainer release to the current user's LocalAppData, validates its published SHA-256 and extracts it there. An existing LLVM-MinGW clang++.exe can be supplied with `-Compiler`. The shader compiler uses the Windows Direct3D compiler. The optimized application loads compiled shader bytecode, so normal startup does not compile shaders. FrameBridge.exe, FrameBridge.Output.exe and FrameBridge.Engine.dll travel together with **bin/Notices**. **bin/.build** contains development tools.

The optional AI installation is described in [Models.md](Models.md).

## Reproduce the checks

```powershell
# Python 3 standard library only
py -3 Tests/make_sample.py
py -3 Tests/smoke.py --interpolate --backend -1 --seconds 10
py -3 Tests/smoke.py --interpolate --backend -2
py -3 Tests/smoke.py --device
py -3 Tests/smoke.py --ai --backend 0
py -3 Tests/smoke.py --ai --backend -2

# Exercise the application shell through live sessions
py -3 Tests/secondary_suite.py
```

The secondary suite temporarily selects the secondary monitor’s advertised 75 Hz timing, runs the regressions, grants the exclusive test window focus with a click inside its verified bounds, and restores the prior desktop timing and pointer position. Use --ui-only to repeat the UI checks. The input smoke test opens a small native output window. The optional device check opens the first enumerated video capture device for eight seconds. The desktop and webcam tests process frames in memory. The UI check captures the app's own synthetic dashboard and Performance page, exercises windowed and fullscreen outputs, and writes its results to **Tests/results**. Visible tests use a secondary display advertising 75 Hz at its desktop resolution. They stop if they cannot identify it. The UI test requests its 75 Hz mode for the exclusive check. Windowed tests use the current desktop refresh.

Use the numeric DXGI adapter index to compare specific GPUs. Negative one selects automatic discovery and negative two selects CPU. The sample generator produces a local uncompressed AVI with a predictable moving rectangle. Other formats depend on the Media Foundation decoders installed on the machine.

Observed build environment on 19 November 2026: Windows 11 Pro 25H2, build 26200.9974, Ryzen 5 7600X (12 logical processors), with no registered dotnet SDK or usable MSVC desktop compiler found. NVIDIA's runtime driver and Windows' Direct3D interfaces were present. No standalone CUDA or Vulkan development kit was found in the inspected paths. LLVM-MinGW 20261117 (Clang 23.1.6) was installed per-user. [LLVM-MinGW release](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261117)

For the hardware-specific regression suite on the secondary 75 Hz display, run `py -3 Tests/secondary_suite.py` after building with `-Test`. For an originals-only video-file run with the same display guard, use `py -3 Tests/secondary_suite.py --presentation-only`. The suite restores the previous desktop mode on exit.

Build the optional private PS5 input separately with `Tools/Build-PlayStation.ps1 -Test`. [Dependencies and pairing](PlayStation.md)
