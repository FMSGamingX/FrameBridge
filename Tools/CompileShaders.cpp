// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "../Engine/GPU/FlowShader.hpp"
#include <windows.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <initializer_list>
#include <cstring>
int main()
{
    for (auto entry : {"estimate", "warp", "vertex", "pixel", "nvWarp", "nvOriginal"})
    {
        const char *profile = entry[0] == 'v' ? "vs_5_0" :
                              entry[0] == 'p' || strcmp(entry,"nvOriginal")==0 ? "ps_5_0" : "cs_5_0";
        ID3DBlob *code = nullptr, *error = nullptr;
        HRESULT hr = D3DCompile(fb::shader, strlen(fb::shader), "FrameBridge", nullptr, nullptr, entry,
                                profile, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &error);
        if (FAILED(hr))
        {
            if (error)
                fprintf(stderr, "%s", static_cast<char *>(error->GetBufferPointer()));
            return 1;
        }
        printf("inline constexpr unsigned char %sCode[]={", entry);
        auto data = static_cast<unsigned char *>(code->GetBufferPointer());
        for (size_t i = 0; i < code->GetBufferSize(); i++)
            printf("%u,", unsigned(data[i]));
        puts("};");
        code->Release();
        if (error)
            error->Release();
    }
}
