// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Markup;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using Forms = System.Windows.Forms;
namespace FrameBridge
{
internal class Choice
{
    public string Text, Key, Details;
    public int Id;
    public Choice(string text, int id, string key = "")
    {
        Text = text;
        Id = id;
        Key = key;
    }
    public override string ToString()
    {
        return Text;
    }
}
internal sealed partial class Shell : Window
{
    readonly Preferences pref = new Preferences();
    readonly FrameworkElement root;
    readonly Surface surface = new Surface();
    readonly DispatcherTimer timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(500) };
    readonly List<Choice> adapters = new List<Choice>(), devices = new List<Choice>(),
                          profiles = new List<Choice>();
    readonly List<double> history = new List<double>();
    readonly Process process = Process.GetCurrentProcess();
    readonly Stopwatch clock = Stopwatch.StartNew();
    readonly Forms.Screen[] screens = Forms.Screen.AllScreens;
    readonly string logFolder =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                     "Famous Gaming", "FrameBridge");
    string page = "Dashboard", file = "", deviceKey = "", engineName = "",
           sourceNote = "Discovering video inputs…", profileNote = "", runName = "";
    int kind = 0, patternSize = 1, desktopIndex = 0, outputIndex = 0, outputMode = 0, backend = -1,
        profile = -1, target = 0, customQuality = 2, qualityMode, windowSize = 1;
    double sourceFps = 30;
    bool hudAware = true;
    bool nvidiaMode;
    bool smoothMotion;
    readonly List<Choice> nvidiaAdapters = new List<Choice>();
    int presentationAdapter = -1;
    int EffectiveBackend { get { return smoothMotion ? presentationAdapter : backend; } }
    int nvidiaQuality = 1;
    readonly List<Choice> opticalAdapters = new List<Choice>();
    static readonly int[] outputWidths = { 640, 960, 1280, 1600, 1920, 2560 };
    static readonly int[] outputHeights = { 360, 540, 720, 900, 1080, 1440 };
    bool vsync = false, aiMode = false;
    int aiQuality = 0;
    bool profiling, discovering, closing, allowClose, finishing;
    double lastTick, lastCpu, inputFps = double.NaN, outputFps = double.NaN, cpuPct = double.NaN;
    IntPtr session;
    Window outputWindow;
    Surface outputSurface;
    Stats stats, previous;
    double knownRate = 0;
    T F<T>(string n)
        where T : FrameworkElement
    {
        return (T)root.FindName(n);
    }
    bool Busy
    {
        get {
            return session != IntPtr.Zero || finishing;
        }
    }
    internal Shell()
    {
        Title = "FrameBridge";
        Width = 1240;
        Height = 790;
        MinWidth = 930;
        MinHeight = 610;
        WindowStartupLocation = WindowStartupLocation.CenterScreen;
        using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("Shell.xaml")) root =
            (FrameworkElement)XamlReader.Load(stream);
        Content = root;
        Background = Brushes.Black;
        if (pref.Maximize)
            WindowState = WindowState.Maximized;
        qualityMode = Math.Max(0, Math.Min(4, pref.Quality));
        Theme();
        foreach (Button b in F<StackPanel>("Nav").Children)
            b.Click += delegate(object o, RoutedEventArgs e)
            {
                Navigate((string)((Button)o).Tag);
            };
        surface.Visibility = Visibility.Hidden;
        F<Grid>("VideoStage").Children.Add(surface);
        F<Button>("StartButton").Click += delegate
        {
            Start();
        };
        F<Button>("StopButton").Click += async delegate
        {
            await Finish("Stopped.");
        };
        SourceInitialized += delegate
        {
            int dark = 1;
            DwmSetWindowAttribute(new WindowInteropHelper(this).Handle, 20, ref dark, 4);
        };
        Loaded += delegate
        {
            Discover();
        };
        Closing += async delegate(object s, System.ComponentModel.CancelEventArgs e)
        {
            if (allowClose)
                return;
            if (Busy)
            {
                e.Cancel = true;
                closing = true;
                await Finish("Stopped.");
            }
            else
                closing = true;
        };
        timer.Tick += delegate
        {
            Poll();
        };
        timer.Start();
        for (int i = 0;; i++)
        {
            var n = new StringBuilder(256);
            int t = Native.fb_adapter(i, n, n.Capacity);
            if (t == 0)
                break;
            if (t != 1)
                continue;
            var d = new StringBuilder(512);
            int ok = fb_adapter_info(i, d, d.Capacity);
            if (ok != 0)
            {
                var choice = new Choice(n.ToString(), i) { Details = d.ToString() };
                adapters.Add(choice);
                if (ok == 1 && Native.fb_adapter_vendor(i) == 0x10de)
                {
                    nvidiaAdapters.Add(choice);
                    if (presentationAdapter < 0)
                        presentationAdapter = i;
                }
                if (Native.fb_nvidia_flow_adapter(i) == 1)
                    opticalAdapters.Add(choice);
            }
        }
        SetStill();
        Navigate("Dashboard");
        ResetMetrics();
        Message("Ready to start.");
    }
    [DllImport("dwmapi")]
    static extern int DwmSetWindowAttribute(IntPtr h, int a, ref int v, int size);
    [DllImport("FrameBridge.Engine.dll", CharSet = CharSet.Unicode)]
    static extern int fb_refresh(string display);
    [DllImport("FrameBridge.Engine.dll", CharSet = CharSet.Unicode)]
    static extern int fb_adapter_info(int i, StringBuilder s, int len);
    void Theme()
    {
        bool graphite = pref.Theme == "Graphite";
        var colors = new Dictionary<string, string> { { "Base", graphite ? "#1A1B1E" : "#10151D" },
                                                      { "Rail", graphite ? "#202125" : "#141B25" },
                                                      { "Panel", graphite ? "#282A30" : "#19222E" },
                                                      { "Field", graphite ? "#34363D" : "#222E3D" },
                                                      { "Stroke", graphite ? "#474A52" : "#303E50" } };
        foreach (var c in colors)
            root.Resources[c.Key] = Brush(c.Value);
    }
    static SolidColorBrush Brush(string color)
    {
        return new SolidColorBrush((Color)ColorConverter.ConvertFromString(color));
    }
    void Save()
    {
        try
        {
            pref.Save();
        }
        catch (System.Configuration.ConfigurationErrorsException e)
        {
            Message(e.Message);
        }
    }
    void Log(string s)
    {
        if (!pref.Logging)
            return;
        try
        {
            Directory.CreateDirectory(logFolder);
            string p = Path.Combine(logFolder, "sessions.log");
            if (File.Exists(p) && new FileInfo(p).Length > 1024 * 1024)
                File.Copy(p, p + ".previous", true);
            if (File.Exists(p) && new FileInfo(p).Length > 1024 * 1024)
                File.WriteAllText(p, "");
            File.AppendAllText(p, DateTimeOffset.Now.ToString("u") + "  " + s + Environment.NewLine);
        }
        catch (IOException e)
        {
            Message("Log unavailable. " + e.Message);
        }
        catch (UnauthorizedAccessException e)
        {
            Message(e.Message);
        }
    }
    void Message(string s)
    {
        F<TextBlock>("Notice").Text = s;
    }
    void SetStill()
    {
        int w = 640, h = 360;
        int[] p = new int[w * h];
        Native.fb_still(w, h, p);
        F<Image>("Still").Source = BitmapSource.Create(w, h, 96, 96, PixelFormats.Bgr32, null, p, w * 4);
    }
    string SourceName()
    {
        if (kind == 4) return "PS5 Network Connection";
        if (kind == 0)
            return "Synthetic motion test";
        if (kind == 3)
            return "Desktop · " + screens[desktopIndex].DeviceName;
        if (kind == 1)
            return file.Length == 0 ? "Video file" : Path.GetFileName(file);
        var d = devices.Find(i => i.Key == deviceKey);
        return d == null ? "Capture device" : d.Text;
    }
    string TargetName()
    {
        return target == 0 ? "Automatic" : target == -1 ? "Source FPS" : target + " fps";
    }
    Forms.Screen OutputScreen()
    {
        return outputMode != 0 ? screens[outputIndex]
                               : Forms.Screen.FromHandle(new WindowInteropHelper(this).Handle);
    }
    void Decorate()
    {
        bool live = Busy && !finishing;
        F<TextBlock>("PreviewTitle").Text = live ? runName : SourceName();
        F<TextBlock>("PreviewInfo").Text =
            live && stats.width > 0 ? String.Format("{0} × {1}   ·   {2:0.##} fps source   ·   SDR RGB",
                                                    stats.width, stats.height, stats.rate)
            : kind == 0             ? "Predictable moving targets   ·   " + sourceFps + " fps source"
            : kind == 4             ? "PlayStation network stream"
            : kind == 2             ? "Media Foundation capture device"
            : kind == 1             ? "Local file   ·   Windows Media Foundation"
                                    : "Windows desktop capture";
        F<TextBlock>("PreviewTag").Text = live ? (smoothMotion ? "OUTPUT WINDOW" : outputMode == 0 ? "LIVE OUTPUT" : "INPUT PREVIEW")
                                               : (kind == 0 ? "REFERENCE FRAME" : "STANDBY");
        F<Image>("Still").Visibility =
            (!live && kind == 0) || (live && outputMode != 0 && !smoothMotion) ? Visibility.Visible : Visibility.Hidden;
        F<TextBlock>("NoSignal").Text = live && smoothMotion ? "Playing in the output window" : "Waiting for an input signal";
        F<TextBlock>("NoSignal").Visibility = (!live && kind != 0) || (live && smoothMotion) ? Visibility.Visible : Visibility.Collapsed;
        surface.Visibility = live && outputMode == 0 ? Visibility.Visible : Visibility.Hidden;
        F<Button>("StartButton").IsEnabled =
            !Busy && (kind != 4 || (pairedPs5 && !pairingPs5)) && !profiling && (kind != 1 || file.Length > 0) && (kind != 2 || deviceKey.Length > 0);
        F<Button>("StopButton").IsEnabled = live;
    }
    async void Discover()
    {
        if (discovering)
            return;
        discovering = true;
        var list = await Task.Run(delegate {
            var all = new List<Choice>();
            for (int i = 0;; i++)
            {
                var n = new StringBuilder(512);
                var k = new StringBuilder(4096);
                int result = Native.fb_device(i, n, n.Capacity, k, k.Capacity);
                if (result < 0)
                    return null;
                if (result == 0)
                    break;
                all.Add(new Choice(n.ToString(), i, k.ToString()));
            }
            return all;
        });
        discovering = false;
        if (closing)
            return;
        devices.Clear();
        if (list != null)
            devices.AddRange(list);
        sourceNote = list == null ? "Windows could not enumerate video inputs."
                     : devices.Count == 0
                         ? "No video devices detected. Video files and the motion test are available."
                         : devices.Count + " video input" + (devices.Count == 1 ? "" : "s") + " detected";
        if (devices.Find(d => d.Key == deviceKey) == null)
        {
            deviceKey = devices.Count > 0 ? devices[0].Key : "";
            profiles.Clear();
            profile = -1;
        }
        if (page == "Video inputs")
            Navigate(page);
    }
    async void LoadProfiles()
    {
        string key = deviceKey;
        if (key == "" || profiling)
            return;
        profiling = true;
        profileNote = "Reading device formats…";
        Navigate(page);
        string text = await Task.Run(delegate {
            var s = new StringBuilder(65000);
            return Native.fb_profiles(key, s, s.Capacity) == 1 ? s.ToString() : null;
        });
        profiling = false;
        if (closing)
            return;
        if (key != deviceKey)
        {
            Decorate();
            return;
        }
        profiles.Clear();
        profile = -1;
        if (text != null)
            foreach (string row in text.Split('\n'))
            {
                int at = row.IndexOf('|'), id;
                if (at > 0 && int.TryParse(row.Substring(0, at), out id))
                    profiles.Add(new Choice(row.Substring(at + 1).Trim(), id));
            }
        profileNote = profiles.Count > 0 ? "Formats reported by the device."
                                         : "Windows could not read the formats. Try the device default.";
        if (page == "Video inputs")
            Navigate(page);
    }
    void ResetMetrics()
    {
        stats = new Stats();
        previous = stats;
        inputFps = outputFps = cpuPct = double.NaN;
        history.Clear();
        gpu = -1;
        UpdateMetrics();
    }
    static string Number(double n)
    {
        return double.IsNaN(n) ? "N/A" : n.ToString("0.0");
    }
    void UpdateMetrics()
    {
        F<TextBlock>("InputRate").Text = Number(inputFps);
        F<TextBlock>("OutputRate").Text = Number(outputFps);
        F<TextBlock>("Cpu").Text = Number(cpuPct);
        F<TextBlock>("Latency").Text =
            Number(stats.processed == 0 ? double.NaN : stats.latencyMs / stats.processed);
    }
    void Start()
    {
        if (Busy || (kind == 4 && (!pairedPs5 || pairingPs5)))
            return;
        try
        {
            ResetMetrics();
            runName = SourceName();
            var monitor = OutputScreen();
            int refresh = fb_refresh(monitor.DeviceName);
            var bounds = screens[desktopIndex].Bounds;
            var c = new Config {
                inputMilliFps = kind==4 ? psFps*1000 : (int)Math.Round(sourceFps * 1000),
                hudAware = !smoothMotion && hudAware ? 1 : 0,
                vsync = !smoothMotion && vsync ? 1 : 0,
                kind = kind,
                width = patternSize == 0 ? 1280 : 1920,
                height = patternSize == 0 ? 720 : 1080,
                profile = profile,
                backend = EffectiveBackend,
                quality = nvidiaMode         ? nvidiaQuality
                          : aiMode           ? aiQuality
                          : qualityMode == 3 ? customQuality
                                             : qualityMode,
                reduceArtifacts = !smoothMotion && pref.Artifact ? 1 : 0,
                generate =
                    !smoothMotion && pref.Generate && (target != -1 && (target == 0 || Math.Abs(target - Rate()) > 1)) ? 1 : 0,
                refresh = refresh,
                buffer = pref.Buffer == 2 ? 2 : 1,
                desktopX = bounds.X,
                desktopY = bounds.Y,
                desktopW = bounds.Width,
                desktopH = bounds.Height
            };
            Surface active = surface;
            if (outputMode != 0)
                F<Image>("Still").Source = null;
            IntPtr exclusiveHandle = IntPtr.Zero;
            if (outputMode >= 3)
            {
                exclusiveHandle = OpenExclusive(monitor, ref c);
            }
            else if (outputMode != 0)
            {
                outputSurface = new Surface();
                active = outputSurface;
                outputWindow =
                    new Window { Title = "FrameBridge · Output",
                                 Content = active,
                                 Background = Brushes.Black,
                                 WindowStartupLocation = WindowStartupLocation.Manual,
                                 Width = outputWidths[windowSize],
                                 Height = outputHeights[windowSize],
                                 WindowStyle =
                                     outputMode == 2 ? WindowStyle.None : WindowStyle.SingleBorderWindow,
                                 ResizeMode = outputMode == 2 ? ResizeMode.NoResize : ResizeMode.CanResize };
                outputWindow.SourceInitialized += delegate
                {
                    var h = new WindowInteropHelper(outputWindow).Handle;
                    var b = monitor.Bounds;
                    RECT outer, client;
                    GetWindowRect(h, out outer);
                    GetClientRect(h, out client);
                    int borderW = outer.right - outer.left - client.right,
                        borderH = outer.bottom - outer.top - client.bottom;
                    SetWindowPos(h, IntPtr.Zero, b.X + (outputMode == 2 ? 0 : 32),
                                 b.Y + (outputMode == 2 ? 0 : 32),
                                 outputMode == 2 ? b.Width : outputWidths[windowSize] + borderW,
                                 outputMode == 2 ? b.Height : outputHeights[windowSize] + borderH, 0x14);
                };
                outputWindow.Closing += async delegate(object o, System.ComponentModel.CancelEventArgs e)
                {
                    if (session != IntPtr.Zero)
                    {
                        e.Cancel = true;
                        await Finish("Output window closed.");
                    }
                };
                outputWindow.KeyDown += async delegate(object o, System.Windows.Input.KeyEventArgs e)
                {
                    if (e.Key == System.Windows.Input.Key.Escape)
                        await Finish("Output window closed.");
                };
                outputWindow.Show();
                outputWindow.UpdateLayout();
            }
            else
            {
                surface.Visibility = Visibility.Visible;
                UpdateLayout();
            }
            IntPtr handle = outputMode >= 3 ? exclusiveHandle : active.Handle;
            string key = kind == 4 ? psHost : kind == 1 ? file : deviceKey;
            session = nvidiaMode && c.generate != 0
                          ? Native.fb_start_nvidia_flow(handle, ref c, key)
                          : Native.fb_start_ai(handle, ref c, key, aiMode && c.generate != 0 ? ModelPath() : null);
            if (session == IntPtr.Zero)
                throw new InvalidOperationException("Unable to start the video engine.");
            lastTick = clock.Elapsed.TotalSeconds;
            lastCpu = process.TotalProcessorTime.TotalSeconds;
            engineName = "Starting…";
            Status("Opening input", "#EAC887");
            Message("Opening " + runName + "…");
            Log("Start · " + new[] { "synthetic", "file", "device", "desktop", "PS5 LAN" }[kind]);
            Navigate(page);
        }
        catch (Exception e)
        {
            CloseExclusive();
            if (outputWindow != null)
            {
                outputWindow.Close();
                outputWindow = null;
            }
            Decorate();
            Status("Unavailable", "#E9A9A3");
            Message(e.Message);
        }
    }
    [DllImport("user32")]
    static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int width, int height,
                                    uint flags);
    [StructLayout(LayoutKind.Sequential)]
    struct RECT
    {
        public int left, top, right, bottom;
    }
    [DllImport("user32")]
    static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32")]
    static extern bool GetClientRect(IntPtr hwnd, out RECT rect);
    double Rate()
    {
        return kind==4 ? psFps : (kind == 0 || kind == 3) ? sourceFps : knownRate;
    }
    void Status(string text, string color)
    {
        F<TextBlock>("State").Text = text;
        F<System.Windows.Shapes.Ellipse>("Lamp").Fill = Brush(color);
    }
    async Task Finish(string reason)
    {
        if (session == IntPtr.Zero || finishing)
            return;
        finishing = true;
        IntPtr old = session;
        session = IntPtr.Zero;
        Native.fb_cancel(old);
        Decorate();
        Status("Stopping", "#EAC887");
        await Task.Run(delegate { Native.fb_close(old); });
        CloseExclusive();
        if (outputWindow != null)
        {
            outputWindow.Close();
            outputWindow = null;
            outputSurface = null;
        }
        finishing = false;
        engineName = "";
        Status(reason.StartsWith("Session error") ? "Session error" : "Standby",
               reason.StartsWith("Session error") ? "#E9A9A3" : "#8291A4");
        Message(reason);
        Log("Stop · " + reason);
        SetStill();
        Navigate(page);
        if (closing && !allowClose)
        {
            allowClose = true;
            Close();
        }
    }
    async void Poll()
    {
        if (session == IntPtr.Zero || finishing)
            return;
        process.Refresh();
        var display =
            Forms.Screen.FromHandle(exclusiveWindow != null ? exclusiveWindow.Handle
                                    : outputWindow != null  ? new WindowInteropHelper(outputWindow).Handle
                                                            : new WindowInteropHelper(this).Handle);
        Native.fb_refresh_limit(session, fb_refresh(display.DeviceName));
        var name = new StringBuilder(512);
        var note = new StringBuilder(1024);
        Native.fb_stats(session, out stats, name, name.Capacity, note, note.Capacity);
        engineName = name.ToString();
        double now = clock.Elapsed.TotalSeconds, dt = now - lastTick,
               used = process.TotalProcessorTime.TotalSeconds;
        if (dt > 0 && stats.processed > 0)
        {
            inputFps = (stats.inputs - previous.inputs) / dt;
            outputFps = (stats.outputs - previous.outputs) / dt;
            cpuPct = 100 * (used - lastCpu) / (dt * Environment.ProcessorCount);
            history.Add(outputFps);
            if (history.Count > 60)
                history.RemoveAt(0);
        }
        lastCpu = used;
        lastTick = now;
        previous = stats;
        UpdateMetrics();
        SampleGpu();
        Decorate();
        if (stats.rate > 0)
            knownRate = stats.rate;
        if (stats.state == 4)
        {
            await Finish("Session error · " + note);
            return;
        }
        if (stats.state == 3)
        {
            await Finish("End of video · " + stats.outputs + " presentations.");
            return;
        }
        if (stats.state == 2)
        {
            Status("Processing", "#79E6C8");
            Message(note.Length > 0
                        ? note.ToString()
                        : engineName + "   ·   " +
                              (stats.activeGeneration != 0 ? "2× interpolation" : "Original frames"));
        }
        if (stats.activeGeneration != 0 && stats.jobs > 15 && stats.generated == 0)
            Message("Midpoints missed their deadlines. Lower quality or select two-frame buffering.");
        if (outputMode == 3 && exclusive != null && stats.state == 2 &&
            Math.Abs(fb_refresh(display.DeviceName) - exclusive.Rate) > 1)
            Message("Windows selected a different exclusive refresh. Use Fullscreen · selected display " +
                    "mode for the requested timing.");
        RefreshFieldValues();
        if (outputMode != 0 && !smoothMotion)
            UpdateThumbnail();
    }
    void UpdateThumbnail()
    {
        int w = 320, h = 180;
        var data = new int[w * h];
        if (fb_preview(session, data, w, h) == 1)
            F<Image>("Still").Source =
                BitmapSource.Create(w, h, 96, 96, PixelFormats.Bgr32, null, data, w * 4);
    }
    [DllImport("FrameBridge.Engine.dll")]
    static extern int fb_preview(IntPtr s, [Out] int[] p, int w, int h);
}
}
