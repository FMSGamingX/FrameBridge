# Optional RIFE inference

Created by Faris Alsari / FMSGaming · Famous Gaming · version 0.1.0

Run **Tools/Install-AI.ps1** in PowerShell. The installer downloads Microsoft’s ONNX Runtime DirectML 1.24.4 and DirectML 1.15.4 packages from NuGet. It verifies SHA-512 against pinned package catalog digests, extracts the x64 native runtime libraries and preserves package licenses and notices under **%LOCALAPPDATA%/FrameBridge/AI/1.24.4**. These libraries are optional. [ONNX Runtime package](https://www.nuget.org/packages/Microsoft.ML.OnnxRuntime.DirectML/1.24.4) · [DirectML package](https://www.nuget.org/packages/Microsoft.AI.DirectML/1.15.4)

The model is RIFE v4.9 from hzwer/Practical-RIFE, exported to ONNX by yuvraj108c and mirrored by Edge Tools. Upstream applies its MIT license to the model links. The mirror provides the license, provenance and ONNX model card. The installer pins revision **321b593c5f36c26f5b08fa2838dcf5eece382fa0** and SHA-256 **76e4cef9ab42fa7dd4e8f6e4aba47462051e3faa969e4bca6479784fbab0ac6f**. Weights, license and card are installed separately under **%LOCALAPPDATA%/FrameBridge/Models/RIFE-4.9**. [Upstream model and license](https://github.com/hzwer/Practical-RIFE) · [Pinned model card](https://huggingface.co/edgetools/rife/tree/321b593c5f36c26f5b08fa2838dcf5eece382fa0)

In **Generation**, select **AI interpolation · RIFE**, an inference width and a processing backend. The application attempts DirectML on the selected adapter. Automatic backend selection tries the available GPU adapters and can fall back to CPU with an on-screen explanation. Manual backend failures are reported as errors. Two-frame capture buffering permits a late generated frame at the cost of greater latency and possible loss of input frames.

**Settings → Select ONNX model** accepts a local model with float32 NCHW RGB inputs named **img0** and **img1**, a scalar or singleton **timestep** input and one float32 NCHW RGB output matching the inference dimensions. Pixel values are in the range 0–1. Models are executable computation graphs and should come from a trusted publisher. FrameBridge uses a midpoint of 0.5. Dynamic images are scaled and edge-padded. A fixed-shape model uses its declared dimensions. Unsupported operators, provider failures and incompatible tensor signatures surface in the dashboard.

The DirectML provider requires sequential execution and disabled memory-pattern optimization. FrameBridge sets both on the inference session. [Microsoft provider documentation](https://onnxruntime.ai/docs/execution-providers/DirectML-ExecutionProvider.html)
