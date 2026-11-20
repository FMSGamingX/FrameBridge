// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#pragma once
#include "../Platform.hpp"
#include "../../ThirdParty/NvidiaOpticalFlow/nvOpticalFlowD3D11.h"

namespace fb
{
// Registered D3D11 surfaces and the driver session live on the render thread.
class NvidiaFlow
{
    HMODULE module = nullptr;
    NV_OF_D3D11_API_FUNCTION_LIST api{};
    NvOFHandle session = nullptr;
    NvOFGPUBufferHandle surfaces[4]{};

    void require(NV_OF_STATUS status, const char *operation)
    {
        if (status == NV_OF_SUCCESS)
            return;
        char detail[512]{};
        uint32_t size = sizeof detail;
        if (session)
            api.nvOFGetLastError(session, detail, &size);
        throw std::runtime_error(std::string("NVIDIA Optical Flow: ") + operation + " failed (" +
                                 std::to_string(status) + "). " + detail);
    }

public:
    static constexpr int grid = 4;
    NvidiaFlow() = default;
    NvidiaFlow(const NvidiaFlow &) = delete;
    ~NvidiaFlow()
    {
        for (auto surface : surfaces)
            if (surface)
                api.nvOFUnregisterResourceD3D11(surface);
        if (session)
            api.nvOFDestroy(session);
        if (module)
            FreeLibrary(module);
    }
    void open(ID3D11Device *device, ID3D11DeviceContext *ctx, int w, int h, int quality)
    {
        module = LoadLibraryExW(L"nvofapi64.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        auto create = module ? reinterpret_cast<decltype(&NvOFAPICreateInstanceD3D11)>(
            GetProcAddress(module, "NvOFAPICreateInstanceD3D11")) : nullptr;
        if (!create)
            throw std::runtime_error("The NVIDIA driver does not expose the D3D11 Optical Flow API.");
        require(create(NV_OF_API_VERSION, &api), "driver API initialization");
        require(api.nvCreateOpticalFlowD3D11(device, ctx, &session), "open selected adapter");
        NV_OF_INIT_PARAMS params{};
        params.width = w;
        params.height = h;
        params.outGridSize = NV_OF_OUTPUT_VECTOR_GRID_SIZE_4;
        params.mode = NV_OF_MODE_OPTICALFLOW;
        params.perfLevel = quality <= 0 ? NV_OF_PERF_LEVEL_FAST :
                           quality == 1 ? NV_OF_PERF_LEVEL_MEDIUM : NV_OF_PERF_LEVEL_SLOW;
        params.inputBufferFormat = NV_OF_BUFFER_FORMAT_ABGR8;
        params.predDirection = NV_OF_PRED_DIRECTION_BOTH;
        require(api.nvOFInit(session, &params), "initialize bidirectional flow at input resolution");
    }
    void bind(ID3D11Texture2D *a, ID3D11Texture2D *b, ID3D11Texture2D *forward, ID3D11Texture2D *backward)
    {
        ID3D11Texture2D *textures[]{a, b, forward, backward};
        for (int i = 0; i < 4; ++i)
            require(api.nvOFRegisterResourceD3D11(session, textures[i], &surfaces[i]), "register texture");
    }
    void execute(int current, bool resetHints)
    {
        NV_OF_EXECUTE_INPUT_PARAMS in{};
        in.inputFrame = surfaces[current ^ 1];
        in.referenceFrame = surfaces[current];
        in.disableTemporalHints = resetHints ? NV_OF_TRUE : NV_OF_FALSE;
        NV_OF_EXECUTE_OUTPUT_PARAMS out{};
        out.outputBuffer = surfaces[2];
        out.bwdOutputBuffer = surfaces[3];
        require(api.nvOFExecute(session, &in, &out), "estimate motion");
    }
};
}
