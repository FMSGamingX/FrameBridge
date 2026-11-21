// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Windows.Interop;
namespace FrameBridge
{
[StructLayout(LayoutKind.Sequential)]
internal struct Config
{
    public int kind, width, height, profile, backend, quality, reduceArtifacts, generate, refresh, buffer,
        desktopX, desktopY, desktopW, desktopH, vsync, exclusive, outputW, outputH, rateNum, rateDen,
        inputMilliFps, hudAware;
}
[StructLayout(LayoutKind.Sequential)]
internal struct Stats
{
    public int state, width, height, activeGeneration;
    public long inputs, outputs, generated, dropped, processed, jobs;
    public double workMs, generationMs, latencyMs, rate, gpuTotalMs;
    public long gpuJobs;
    public double pacingErrorMs;
    public long intervals;
    public int hdr;
}
internal static partial class Native
{
    [DllImport(Library, CharSet = CharSet.Unicode)]
    internal static extern int fb_device(int i, StringBuilder name, int nc, StringBuilder id, int ic);
    [DllImport(Library, CharSet = CharSet.Unicode)]
    internal static extern int fb_profiles(string key, StringBuilder profiles, int length);
    [DllImport(Library, CharSet = CharSet.Unicode)]
    internal static extern IntPtr fb_start(IntPtr hwnd, ref Config config, string key);
    [DllImport(Library, CharSet = CharSet.Unicode)]
    internal static extern IntPtr fb_start_ai(IntPtr hwnd, ref Config config, string key, string model);
    [DllImport(Library, CharSet = CharSet.Unicode)]
    internal static extern IntPtr fb_start_nvidia_flow(IntPtr hwnd, ref Config config, string key);
    [DllImport(Library)]
    internal static extern int fb_nvidia_flow_adapter(int index);
    [DllImport(Library)]
    internal static extern int fb_adapter_vendor(int index);
    [DllImport(Library, CharSet = CharSet.Unicode)]
    internal static extern void fb_stats(IntPtr s, out Stats stats, StringBuilder backend, int bc,
                                         StringBuilder msg, int mc);
    [DllImport(Library)]
    internal static extern void fb_refresh_limit(IntPtr s, int hz);
    [DllImport(Library)]
    internal static extern void fb_cancel(IntPtr s);
    [DllImport(Library)]
    internal static extern void fb_close(IntPtr s);
    [DllImport(Library)]
    internal static extern void fb_still(int w, int h, [Out] int[] pixels);
}
internal sealed class Surface : HwndHost
{
    [DllImport("user32", CharSet = CharSet.Unicode)]
    static extern IntPtr CreateWindowEx(int ex, string cls, string title, int style, int x, int y, int w,
                                        int h, IntPtr parent, IntPtr menu, IntPtr instance, IntPtr data);
    [DllImport("user32")]
    static extern bool DestroyWindow(IntPtr hwnd);
    protected override HandleRef BuildWindowCore(HandleRef parent)
    {
        var h = CreateWindowEx(0, "STATIC", "FrameBridge video", 0x50000004, 0, 0, 2, 2, parent.Handle,
                               IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
        return new HandleRef(this, h);
    }
    protected override void DestroyWindowCore(HandleRef h)
    {
        DestroyWindow(h.Handle);
    }
}
}
