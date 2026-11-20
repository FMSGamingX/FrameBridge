// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "Inference.hpp"
#define ORT_API_MANUAL_INIT
#include "onnxruntime_cxx_api.h"
#include <filesystem>
namespace fb
{
namespace
{
struct Runtime
{
    HMODULE ort = nullptr, dml = nullptr;
    using Append = OrtStatus *(ORT_API_CALL *)(OrtSessionOptions *, int);
    Append append = nullptr;
    Runtime()
    {
        wchar_t local[32768]{};
        auto n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, 32768);
        if (!n || n >= 32768)
            throw std::runtime_error("Local application data is unavailable.");
        auto dir = std::filesystem::path(local) / L"FrameBridge" / L"AI" / L"1.24.4";
        auto load = [&](const wchar_t *file) {
            return LoadLibraryExW((dir / file).c_str(), nullptr,
                                  LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        };
        dml = load(L"DirectML.dll");
        ort = load(L"onnxruntime.dll");
        if (!ort)
        {
            if (dml)
                FreeLibrary(dml);
            throw std::runtime_error(
                "Install FrameBridge's optional ONNX Runtime libraries using Tools/Install-AI.ps1.");
        }
        auto get = reinterpret_cast<decltype(&OrtGetApiBase)>(GetProcAddress(ort, "OrtGetApiBase"));
        auto api = get ? get()->GetApi(ORT_API_VERSION) : nullptr;
        if (!api)
        {
            FreeLibrary(ort);
            if (dml)
                FreeLibrary(dml);
            throw std::runtime_error("Incompatible ONNX Runtime API version.");
        }
        Ort::InitApi(api);
        append =
            reinterpret_cast<Append>(GetProcAddress(ort, "OrtSessionOptionsAppendExecutionProvider_DML"));
    }
    // Keep the runtime resident for the process lifetime; ORT owns provider TLS.
};
Runtime &runtime()
{
    static Runtime r;
    return r;
}
struct Rife final : Inference
{
    Runtime &rt;
    Ort::Env env;
    Ort::Session session{nullptr};
    Ort::MemoryInfo memory;
    int quality;
    std::wstring label;
    std::vector<float> left, right;
    std::vector<std::string> names;
    std::string output;
    std::vector<int64_t> timestepShape;
    int fixedW = 0, fixedH = 0;
    Rife(const std::wstring &path, int adapter, int q)
        : rt(runtime()), env(ORT_LOGGING_LEVEL_ERROR, "FrameBridge"),
          memory(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)), quality(q)
    {
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(3);
        options.SetInterOpNumThreads(1);
        if (adapter >= 0)
        {
            if (!rt.dml || !rt.append)
                throw std::runtime_error("DirectML inference library is unavailable.");
            options.DisableMemPattern();
            options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
            Ort::ThrowOnError(rt.append(options, adapter));
            label = L"RIFE · DirectML";
        }
        else
            label = L"RIFE · CPU";
        session = Ort::Session(env, path.c_str(), options);
        if (session.GetInputCount() != 3 || session.GetOutputCount() != 1)
            throw std::runtime_error(
                "Expected three model inputs (img0, img1, timestep) and one RGB output.");
        Ort::AllocatorWithDefaultOptions alloc;
        for (size_t i = 0; i < 3; i++)
        {
            std::string name = session.GetInputNameAllocated(i, alloc).get();
            names.push_back(name);
            auto info = session.GetInputTypeInfo(i);
            auto tensor = info.GetTensorTypeAndShapeInfo();
            auto shape = tensor.GetShape();
            if (tensor.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
                throw std::runtime_error("The model must accept float32 RGB tensors.");
            if (name == "img0" || name == "img1")
            {
                if (shape.size() != 4 || (shape[0] > 0 && shape[0] != 1) || (shape[1] > 0 && shape[1] != 3))
                    throw std::runtime_error("Expected NCHW RGB input.");
                if (shape[2] > 0)
                    fixedH = (int)shape[2];
                if (shape[3] > 0)
                    fixedW = (int)shape[3];
            }
            else if (name == "timestep")
            {
                timestepShape = shape;
                for (auto &d : timestepShape)
                    if (d < 0)
                        d = 1;
                for (auto d : timestepShape)
                    if (d != 1)
                        throw std::runtime_error("Expected a scalar midpoint input.");
            }
            else
                throw std::runtime_error("The model must expose img0, img1 and timestep.");
        }
        output = session.GetOutputNameAllocated(0, alloc).get();
    }
    std::wstring name() const override
    {
        return label;
    }
    void generate(const Frame &a, const Frame &b, Frame &out) override
    {
        // Padding repeats the edge. Downscaled inference preserves both full-size originals.
        int cap = quality == 0 ? 256 : quality == 1 ? 512 : quality == 2 ? 960 : 1920;
        int iw = std::min(b.width, cap), ih = std::max(1, int(int64_t(b.height) * iw / b.width));
        if (fixedW || fixedH)
        {
            if (!fixedW || !fixedH)
                throw std::runtime_error("Partially fixed model shapes are unsupported.");
            iw = fixedW;
            ih = fixedH;
        }
        int w = fixedW ? iw : ((iw + 31) / 32) * 32, h = fixedH ? ih : ((ih + 31) / 32) * 32;
        if (w <= 0 || h <= 0 || int64_t(w) * h > 4194304)
            throw std::runtime_error("Model tensor exceeds the supported size.");
        size_t plane = size_t(w) * h;
        std::array<int64_t, 4> shape{1, 3, h, w};
        auto pack = [&](const Frame &f, std::vector<float> &v) {
            v.resize(3 * plane);
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    uint32_t p = f.pixels[size_t(std::min(y, ih - 1) * f.height / ih) * f.width +
                                          std::min(x, iw - 1) * f.width / iw];
                    size_t i = size_t(y) * w + x;
                    v[i] = ((p >> 16) & 255) / 255.f;
                    v[plane + i] = ((p >> 8) & 255) / 255.f;
                    v[2 * plane + i] = (p & 255) / 255.f;
                }
        };
        pack(a, left);
        pack(b, right);
        float t = .5f;
        std::vector<const char *> inputNames;
        std::vector<Ort::Value> tensors;
        for (auto &n : names)
        {
            inputNames.push_back(n.c_str());
            if (n == "timestep")
                tensors.push_back(Ort::Value::CreateTensor<float>(memory, &t, 1, timestepShape.data(),
                                                                  timestepShape.size()));
            else
            {
                auto &v = n == "img0" ? left : right;
                tensors.push_back(
                    Ort::Value::CreateTensor<float>(memory, v.data(), v.size(), shape.data(), 4));
            }
        }
        const char *outputName = output.c_str();
        auto result = session.Run(Ort::RunOptions{nullptr}, inputNames.data(), tensors.data(), tensors.size(),
                                  &outputName, 1);
        auto info = result[0].GetTensorTypeAndShapeInfo();
        if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            info.GetShape() != std::vector<int64_t>(shape.begin(), shape.end()))
            throw std::runtime_error("Model returned an incompatible RGB output shape.");
        auto *pixels = result[0].GetTensorData<float>();
        out.size(b.width, b.height);
        auto byte = [](float v) { return uint32_t(std::clamp(v, 0.f, 1.f) * 255.f + .5f); };
        for (int y = 0; y < b.height; y++)
            for (int x = 0; x < b.width; x++)
            {
                size_t i = size_t(y * ih / b.height) * w + x * iw / b.width;
                out.pixels[size_t(y) * b.width + x] = 0xff000000u | (byte(pixels[i]) << 16) |
                                                      (byte(pixels[plane + i]) << 8) |
                                                      byte(pixels[2 * plane + i]);
            }
    }
};
} // namespace
std::unique_ptr<Inference> openModel(const std::wstring &path, int adapter, int quality)
{
    return std::make_unique<Rife>(path, adapter, quality);
}
} // namespace fb
