// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
namespace FrameBridge
{
internal sealed partial class Shell
{
    [DllImport("user32")]
    static extern bool PrintWindow(IntPtr hwnd, IntPtr dc, uint flags);
    [DllImport("FrameBridge.Engine.dll", CharSet = CharSet.Unicode)]
    static extern int fb_test_display(System.Text.StringBuilder name, int cap, out RECT rect);
    internal void PlaceTestWindow()
    {
        var name = new System.Text.StringBuilder(64);
        RECT rect;
        if (fb_test_display(name, name.Capacity, out rect) == 0)
            throw new InvalidOperationException("Secondary 75 Hz monitor unavailable.");
        outputIndex = Array.FindIndex(screens, s => s.DeviceName == name.ToString());
        if (outputIndex < 0)
            throw new InvalidOperationException("Test monitor disconnected.");
        desktopIndex = outputIndex;
        WindowState = WindowState.Normal;
        WindowStartupLocation = WindowStartupLocation.Manual;
        Left = rect.left + 30;
        Top = rect.top + 30;
        SourceInitialized += delegate
        {
            SetWindowPos(new WindowInteropHelper(this).Handle, IntPtr.Zero, rect.left + 30, rect.top + 30, 0,
                         0, 0x15);
        };
    }
    internal async Task Exercise(string directory)
    {
        Directory.CreateDirectory(directory);
        var errorFile = System.IO.Path.Combine(directory, "ui-error.txt");
        if (File.Exists(errorFile))
            File.Delete(errorFile);
        var lines = new System.Collections.Generic.List<string>();
        try
        {
            if (OutputScreen().DeviceName != screens[outputIndex].DeviceName)
                throw new Exception("Dashboard test window on wrong monitor.");
            lines.Add("Test monitor " + OutputScreen().DeviceName + " at " +
                      fb_refresh(OutputScreen().DeviceName) + " Hz");
            foreach (var p in new[] { "Video inputs", "Generation", "Output", "Performance", "Settings",
                                      "About", "Dashboard" })
            {
                Navigate(p);
                UpdateLayout();
                if (Fields.Children.Count == 0)
                    throw new Exception(p + " is empty.");
                lines.Add(p + " opened");
            }
            Navigate("Video inputs");
            var sourcePicker = Fields.Children.OfType<ComboBox>().First();
            sourcePicker.SelectedItem = sourcePicker.Items.OfType<Choice>().Single(c => c.Id == 4);
            if (F<Button>("StartButton").IsEnabled || kind != 4)
                throw new Exception("PS5 discovery allowed an unavailable streaming session.");
            var addr = System.Net.IPAddress.Loopback;
            string reply = "HTTP/1.1 620 Standby\ndevice-discovery-protocol-version:00030010\nhost-name:Fixture\n";
            if (PlayStationDiscovery.Parse(reply,addr) != "Fixture · 127.0.0.1 · Rest mode" ||
                PlayStationDiscovery.Parse("HTTP/1.1 200 OK\n",addr) != null)
                throw new Exception("PS5 discovery response filtering failed.");
            var find = Fields.Children.OfType<Button>().Single(b => (string)b.Content == "Find PS5 consoles");
            find.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            while (scanningPs5) await Task.Delay(50);
            lines.Add("PS5 LAN discovery: " + psStatus);
            kind = 0;
            ChangeSource();
            Navigate("Dashboard");
            await Task.Delay(400);
            SaveView(directory, "dashboard");
            Start();
            await WaitForFrames();
            SaveView(directory, "processing");
            Navigate("Performance");
            await Task.Delay(1000);
            SaveView(directory, "performance");
            if (stats.generated < 1)
                throw new Exception("No interpolated presentations in dashboard test. " +
                                    F<TextBlock>("Notice").Text);
            lines.Add(String.Format("Dashboard: {0} original inputs, {1} presentations, {2} generated.",
                                    stats.inputs, stats.outputs, stats.generated));
            await Finish("Test stopped.");
            if (opticalAdapters.Count > 0)
            {
                Navigate("Generation");
                var picker = Fields.Children.OfType<ComboBox>().First();
                picker.SelectedItem = picker.Items.OfType<Choice>().Single(c => c.Text == "NVIDIA Optical Flow");
                Start();
                await WaitForFrames();
                if (!engineName.Contains("NVIDIA Optical Flow") || stats.generated == 0)
                    throw new Exception("NVIDIA generation selection did not produce midpoints.");
                lines.Add("NVIDIA Optical Flow selected and presented " + stats.generated + " midpoints.");
                await Finish("Test stopped.");
            }
            sourceFps = 24;
            target = 48;
            windowSize = 2;
            outputMode = 1;
            Navigate("Output");
            Start();
            await WaitForFrames();
            RECT client;
            if (!GetClientRect(outputSurface.Handle, out client) || client.right != 1280 ||
                client.bottom != 720 || stats.rate != 24 || stats.generated == 0)
                throw new Exception("Selected resolution or frame rate failed.");
            if (System.Windows.Forms.Screen.FromHandle(outputSurface.Handle).DeviceName !=
                screens[outputIndex].DeviceName)
                throw new Exception("Output window on wrong monitor.");
            await Finish("Test stopped.");
            lines.Add("24 → 48 fps selection and 1280 × 720 client output passed.");
            var initialHz = fb_refresh(screens[outputIndex].DeviceName);
            if (Math.Abs(initialHz - 75) <= 1)
            {
                sourceFps = 37.5;
                target = 75;
            }
            outputMode = 2;
            Start();
            await WaitForFrames();
            if (sourceFps == 37.5)
            {
                await Task.Delay(1500);
                if (stats.generated < 1)
                    throw new Exception("No 75 fps borderless midpoints");
                lines.Add("Borderless 37.5 fps input, " + Number(outputFps) + " presentations/s.");
            }
            await Finish("Test stopped.");
            lines.Add("Borderless output and shutdown passed.");
            ReadModes();
            if (exclusive != null)
            {
                var targetMode = exclusiveModes.Find(m => m.Width == screens[outputIndex].Bounds.Width &&
                                                          m.Height == screens[outputIndex].Bounds.Height &&
                                                          Math.Abs(m.Rate - 75) < 0.1);
                if (targetMode == null)
                    throw new Exception("75 Hz exclusive mode unavailable.");
                exclusive = targetMode;
                outputMode = 4;
                sourceFps = 37.5;
                target = 75;
                Start();
                await WaitForFrames();
                int actualHz = fb_refresh(screens[outputIndex].DeviceName);
                if (Math.Abs(actualHz - 75) > 1)
                    throw new Exception("Unexpected fullscreen refresh " + actualHz);
                if (stats.rate != 37.5 || stats.generated == 0)
                    throw new Exception("37.5 to 75 conversion failed.");
                await Task.Delay(2000);
                lines.Add("Windows display-mode fullscreen " + targetMode.Rate + " Hz, " + stats.rate +
                          " fps input, " + Number(outputFps) + " presentations/s.");
                await Finish("Test stopped.");
                if (fb_refresh(screens[outputIndex].DeviceName) != initialHz)
                    throw new Exception("Previous refresh was not restored.");
                sourceFps = 24;
                target = 48;
                outputMode = 3;
                Start();
                await WaitForFrames();
                lines.Add("DXGI exclusive presented frames. Requested " + targetMode.Rate +
                          " Hz, Windows reported " + fb_refresh(screens[outputIndex].DeviceName) + " Hz.");
                await Finish("Test stopped.");
            }
        }
        finally
        {
            File.WriteAllLines(System.IO.Path.Combine(directory, "ui-test.txt"), lines);
        }
    }
    async Task WaitForFrames()
    {
        for (int i = 0; i < 100; i++)
        {
            await Task.Delay(100);
            if (session == IntPtr.Zero)
                throw new Exception("Session stopped. " + F<TextBlock>("Notice").Text);
            if (stats.outputs >= 30)
                return;
        }
        throw new TimeoutException(F<TextBlock>("Notice").Text);
    }
    void SaveView(string dir, string name)
    {
        var hwnd = new WindowInteropHelper(this).Handle;
        using (var bitmap = new Bitmap((int)ActualWidth, (int)ActualHeight))
        {
            using (var g = Graphics.FromImage(bitmap))
            {
                var dc = g.GetHdc();
                try
                {
                    PrintWindow(hwnd, dc, 2);
                }
                finally
                {
                    g.ReleaseHdc(dc);
                }
            }
            bitmap.Save(System.IO.Path.Combine(dir, name + ".png"), ImageFormat.Png);
        }
    }
}
}
