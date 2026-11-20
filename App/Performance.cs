// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
namespace FrameBridge
{
internal sealed partial class Shell
{
    double gpu = -1;
    bool sampling;
    [DllImport("FrameBridge.Engine.dll")]
    static extern double fb_gpu_usage();
    string GpuText()
    {
        return gpu < 0 ? "N/A" : Number(gpu) + " %";
    }
    async void SampleGpu()
    {
        if (sampling)
            return;
        sampling = true;
        try
        {
            gpu = await Task.Run((Func<double>)fb_gpu_usage);
        }
        finally
        {
            sampling = false;
        }
    }
}
}
