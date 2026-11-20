// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Shapes;
using Microsoft.Win32;
using Forms = System.Windows.Forms;
namespace FrameBridge
{
internal sealed partial class Shell
{
    StackPanel Fields
    {
        get {
            return F<StackPanel>("Fields");
        }
    }
    void Navigate(string name)
    {
        page = name;
        F<TextBlock>("PageTitle").Text = name;
        var descriptions = new Dictionary<string, string> {
            { "Dashboard", "Monitor the source, interpolation engine and output in one place." },
            { "Video inputs", "Choose a test signal, a local video or a connected capture device." },
            { "Generation", "Configure how FrameBridge estimates motion between original frames." },
            { "Output", "Choose where the processed signal appears." },
            { "Performance", "Live measurements from the current processing session." },
            { "Settings", "Application preferences on this Windows account." },
            { "About", "Product information, acknowledgements and licensing." }
        };
        F<TextBlock>("Subtitle").Text = descriptions[name];
        bool dash = page == "Dashboard";
        var g = F<Grid>("Workspace");
        g.ColumnDefinitions[0].Width = new GridLength(dash ? 1.68 : 1.35, GridUnitType.Star);
        g.ColumnDefinitions[2].Width = new GridLength(1, GridUnitType.Star);
        Grid.SetColumn(F<Border>("PreviewCard"), dash ? 0 : 2);
        Grid.SetColumn(F<Border>("Inspector"), dash ? 2 : 0);
        foreach (Button b in F<StackPanel>("Nav").Children)
        {
            bool chosen = (string)b.Tag == name;
            b.Background = chosen ? Brush("#263D41") : Brushes.Transparent;
            b.Foreground = chosen ? Brush("#91EACF") : (Brush)root.Resources["Muted"];
        }
        DrawFields();
        Decorate();
        F<Border>("Inspector")
            .BeginAnimation(UIElement.OpacityProperty, new System.Windows.Media.Animation.DoubleAnimation(
                                                           .82, 1, TimeSpan.FromMilliseconds(140)));
    }
    readonly Dictionary<string, TextBlock> values = new Dictionary<string, TextBlock>();
    Canvas graph;
    Polyline graphLine;
    TextBlock graphEmpty;
    void DrawFields()
    {
        values.Clear();
        graph = null;
        Fields.Children.Clear();
        switch (page)
        {
        case "Dashboard":
            Dashboard();
            break;
        case "Video inputs":
            Inputs();
            break;
        case "Generation":
            Generation();
            break;
        case "Output":
            Output();
            break;
        case "Performance":
            Performance();
            break;
        case "Settings":
            Settings();
            break;
        case "About":
            About();
            break;
        }
    }
    TextBlock Text(string text, double size = 13, string color = "Ink", double top = 0, bool bold = false)
    {
        var t = new TextBlock { Text = text,
                                FontSize = size,
                                TextWrapping = TextWrapping.Wrap,
                                Foreground = (Brush)root.Resources[color],
                                Margin = new Thickness(0, top, 0, 0),
                                FontWeight = bold ? FontWeights.SemiBold : FontWeights.Normal };
        Fields.Children.Add(t);
        return t;
    }
    void Heading(string title)
    {
        Text(title, 18, "Ink", 0, true);
    }
    void Label(string text)
    {
        Text(text, 12, "Muted", 12, true);
    }
    void Divider()
    {
        Fields.Children.Add(new Border { Background = (Brush)root.Resources["Stroke"], Height = 1,
                                         Margin = new Thickness(0, 13, 0, 8) });
    }
    Button Action(string name, Action act, bool enabled = true)
    {
        var b = new Button { Content = name, HorizontalAlignment = HorizontalAlignment.Left,
                             Margin = new Thickness(0, 9, 0, 0), IsEnabled = enabled };
        b.Click += delegate
        {
            act();
        };
        Fields.Children.Add(b);
        return b;
    }
    ComboBox Pick(string title, IEnumerable<Choice> choices, int selected, Action<Choice> changed,
                  bool enabled = true)
    {
        Label(title);
        var box = new ComboBox { Margin = new Thickness(0, 5, 0, 0), IsEnabled = enabled && !Busy };
        foreach (var c in choices)
        {
            box.Items.Add(c);
            if (c.Id == selected)
                box.SelectedItem = c;
        }
        box.SelectionChanged += delegate
        {
            var value = box.SelectedItem as Choice;
            if (value != null)
                changed(value);
        };
        Fields.Children.Add(box);
        return box;
    }
    static List<Choice> Options(params string[] labels)
    {
        return labels.Select((s, i) => new Choice(s, i)).ToList();
    }
    void Check(string title, bool value, Action<bool> changed, bool lockWhileRunning = true)
    {
        var b = new CheckBox { Content = title, IsChecked = value, IsEnabled = !lockWhileRunning || !Busy };
        b.Click += delegate
        {
            changed(b.IsChecked == true);
        };
        Fields.Children.Add(b);
    }
    void Data(string label, string value)
    {
        var grid = new Grid { Margin = new Thickness(0, 6, 0, 2) };
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1.35, GridUnitType.Star) });
        var a = new TextBlock { Text = label, Foreground = (Brush)root.Resources["Muted"],
                                TextWrapping = TextWrapping.Wrap, FontSize = 12 };
        var b = new TextBlock { Text = value, TextWrapping = TextWrapping.Wrap, FontSize = 12,
                                TextAlignment = TextAlignment.Right, FontWeight = FontWeights.SemiBold };
        Grid.SetColumn(b, 1);
        grid.Children.Add(a);
        grid.Children.Add(b);
        Fields.Children.Add(grid);
        values[label] = b;
    }
    string AdapterName()
    {
        var a = adapters.Find(x => x.Id == backend);
        if (a != null)
            return a.Text;
        return backend == -2 ? "CPU" : adapters.Count > 0 ? adapters[0].Text : "CPU";
    }
    void Dashboard()
    {
        Heading("Signal path");
        Label("INPUT");
        Text(SourceName(), 15, "Ink", 3, true);
        Label("GENERATION");
        Text(pref.Generate ? (nvidiaMode ? "NVIDIA Optical Flow"
                              : aiMode   ? "AI interpolation · RIFE"
                              : hudAware ? "HUD-aware optical flow"
                                         : "Traditional interpolation")
                           : "Original frames",
             15, "Ink", 3, true);
        Label("OUTPUT");
        Text(outputMode == 0   ? "Dashboard preview"
             : outputMode == 1 ? "Windowed output"
             : outputMode == 2 ? "Borderless fullscreen"
             : outputMode == 4 ? "Fullscreen · selected display mode"
                               : "Exclusive fullscreen",
             15, "Ink", 3, true);
        Divider();
        Data("Detected GPU", AdapterName());
        Data("Output target", pref.Generate ? TargetName() : "Source rate");
        Data("GPU utilization", GpuText());
        Action("Configure input", delegate { Navigate("Video inputs"); });
    }
    void ChangeSource()
    {
        ResetMetrics();
        knownRate = 0;
        target = 0;
        Navigate(page);
    }
    [System.Runtime.InteropServices.DllImport("FrameBridge.Engine.dll",
                                              CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
    static extern double fb_file_rate(string path);
    void Inputs()
    {
        Heading("Video source");
        Pick("Input",
             Options("Synthetic motion test", "Video file", "Capture card / webcam", "Desktop capture"), kind,
             c =>
             {
                 kind = c.Id;
                 ChangeSource();
             });
        if (kind == 0)
        {
            Pick("Test resolution", Options("1280 × 720", "1920 × 1080"), patternSize,
                 c =>
                 {
                     patternSize = c.Id;
                     Decorate();
                 });
            Text("Moving textured targets and a position marker.", 12, "Muted", 9);
        }
        if (kind == 0 || kind == 3)
            Pick("Input FPS",
                 new[] { 15, 24, 25, 29.97, 30, 37.5, 45, 50, 59.94, 60 }.Select(
                     n => new Choice(n + " fps", (int)Math.Round(n * 1000))),
                 (int)Math.Round(sourceFps * 1000),
                 c =>
                 {
                     sourceFps = c.Id / 1000.0;
                     ChangeSource();
                 });
        if (kind == 1)
        {
            Text(file.Length > 0 ? file : "Choose a local video supported by Windows Media Foundation.", 13,
                 "Muted", 10);
            Action("Browse video…", async delegate {
                var d = new OpenFileDialog {
                    Filter = "Video files|*.mp4;*.mov;*.m4v;*.avi;*.wmv;*.mkv;*.mpg;*.mpeg|All files|*.*"
                };
                if (d.ShowDialog(this) == true)
                {
                    string selected = d.FileName;
                    file = selected;
                    ChangeSource();
                    double rate = await System.Threading.Tasks.Task.Run(() => fb_file_rate(selected));
                    if (!closing && file == selected && kind == 1)
                    {
                        knownRate = rate;
                        if (!Busy)
                            Navigate(page);
                    }
                }
            }, !Busy);
        }
        if (kind == 1)
            Data("File input FPS", Rate() > 0 ? Rate().ToString("0.###") : "N/A");
        if (kind == 2)
        {
            Text(sourceNote, 12, "Muted", 8);
            Pick("Device", devices, devices.FindIndex(x => x.Key == deviceKey),
                 c =>
                 {
                     deviceKey = c.Key;
                     profile = -1;
                     profiles.Clear();
                     profileNote = "";
                     ChangeSource();
                 });
            var list = new List<Choice> { new Choice("Device default", -1) };
            list.AddRange(profiles);
            Pick("Resolution, rate and color format", list, profile,
                 c =>
                 {
                     profile = c.Id;
                     ChangeSource();
                 });
            Action("Read device formats", LoadProfiles, !Busy && deviceKey.Length > 0);
            if (profileNote != "")
                Text(profileNote, 12, "Muted", 6);
            Action("Refresh devices", Discover, !Busy && !discovering);
        }
        if (kind == 3)
            Pick("Desktop display", MonitorChoices(), desktopIndex,
                 c =>
                 {
                     desktopIndex = c.Id;
                     ChangeSource();
                 });
        Pick("Capture buffering", Options("Latest frame · low latency", "Two frames · absorb jitter"),
             pref.Buffer == 2 ? 1 : 0,
             c =>
             {
                 pref.Buffer = c.Id + 1;
                 Save();
             });
        Divider();
        Text("Console input", 14, "Ink", 0, true);
        Text("Connect the console’s HDMI output to a compatible capture card. Select the card under " +
                 "Capture card / webcam. An ordinary PC HDMI output cannot receive video.",
             12, "Muted", 5);
        if (kind != 2)
            Text(sourceNote, 12, "Muted", 8);
        Data("HDR input metadata", kind == 0 || stats.hdr == 1 ? "SDR" : "N/A");
    }
    void Generation()
    {
        Heading("Frame generation");
        Check("Enable frame generation", pref.Generate,
              v =>
              {
                  pref.Generate = v;
                  Save();
                  Navigate(page);
              });
        var modes = Options("Traditional interpolation");
        modes.Add(new Choice("HUD-aware optical flow", 2));
        if (opticalAdapters.Count > 0)
            modes.Add(new Choice("NVIDIA Optical Flow", 3));
        if (AiAvailable())
            modes.Add(new Choice("AI interpolation · RIFE", 1));
        var mode = Pick("Generation mode", modes,
                        nvidiaMode ? 3
                        : aiMode   ? 1
                        : hudAware ? 2
                                   : 0,
                        c =>
                        {
                            aiMode = c.Id == 1;
                            hudAware = c.Id == 2;
                            nvidiaMode = c.Id == 3;
                            if (nvidiaMode && !opticalAdapters.Exists(a => a.Id == backend))
                            {
                                backend = opticalAdapters[0].Id;
                                if (outputMode >= 3)
                                    ReadModes();
                            }
                            if (aiMode && outputMode >= 3)
                                outputMode = 0;
                            Navigate(page);
                        });
        if (opticalAdapters.Count == 0)
            mode.Items.Add(new ComboBoxItem { Content = "NVIDIA Optical Flow · compatible driver and GPU required", IsEnabled = false });
        if (!AiAvailable())
            mode.Items.Add(
                new ComboBoxItem { Content = "AI interpolation · install model", IsEnabled = false });
        foreach (string name in new[] { "NVIDIA DLSS Integration", "NVIDIA Smooth Motion" })
            mode.Items.Add(new ComboBoxItem { Content = name, IsEnabled = false });
        if (nvidiaMode)
        {
            Pick("Optical flow quality", Options("Performance", "Balanced", "Quality"), nvidiaQuality,
                 c => nvidiaQuality = c.Id);
            Text("The NVIDIA optical flow accelerator tracks motion in both directions between source frames.", 12, "Muted", 6);
            Check("Reduce ghosting at occlusions", pref.Artifact, v => { pref.Artifact = v; Save(); });
        }
        else if (aiMode)
        {
            Pick("AI inference width",
                 Options("Performance · up to 256 px", "Balanced · up to 512 px", "Quality · up to 960 px",
                         "Full · up to 1920 px"),
                 aiQuality, c => aiQuality = c.Id);
            Text("Original frames keep their input resolution. The model estimates the midpoint at the " +
                     "selected width.",
                 12, "Muted", 6);
        }
        else
        {
            Pick("Interpolation quality",
                 Options("Performance", "Balanced", "Quality", "Custom", "Wide motion search"), qualityMode,
                 c =>
                 {
                     qualityMode = c.Id;
                     pref.Quality = c.Id;
                     Save();
                     Navigate(page);
                 });
            if (qualityMode == 3)
            {
                Label("Motion search radius");
                var slider = new Slider { Minimum = 0,
                                          Maximum = 4,
                                          Value = customQuality,
                                          TickFrequency = 1,
                                          IsSnapToTickEnabled = true,
                                          IsEnabled = !Busy,
                                          Margin = new Thickness(0, 7, 0, 2) };
                var text = Text((customQuality + 1) + " search steps", 12, "Muted");
                slider.ValueChanged += delegate
                {
                    customQuality = (int)slider.Value;
                    text.Text = (customQuality + 1) + " search steps";
                };
                Fields.Children.Add(slider);
            }
            Check("Reduce artifacts on poor motion matches", pref.Artifact,
                  v =>
                  {
                      pref.Artifact = v;
                      Save();
                  });
        }
        var g =
            new List<Choice> { new Choice("Automatic · GPU with CPU fallback", -1), new Choice("CPU", -2) };
        g.AddRange(adapters);
        Pick("Processing backend", nvidiaMode ? opticalAdapters : g, backend,
             c =>
             {
                 backend = c.Id;
                 if (outputMode >= 3)
                 {
                     ReadModes();
                     if (exclusive == null)
                         outputMode = 0;
                 }
             });
        Divider();
        Text("Additional modes", 14, "Ink", 0, true);
        if (!AiAvailable())
            Text("AI interpolation · Install the optional runtime and select an ONNX model in Settings.", 12,
                 "Muted", 6);
        Text(NvidiaStatus(0), 12, "Muted", 6);
        Text(NvidiaStatus(1), 12, "Muted", 6);
    }
    List<Choice> MonitorChoices()
    {
        var l = new List<Choice>();
        for (int i = 0; i < screens.Length; i++)
        {
            var b = screens[i].Bounds;
            var hz = fb_refresh(screens[i].DeviceName);
            l.Add(new Choice(String.Format("Display {0}   ·   {1} × {2}   ·   {3}", i + 1, b.Width, b.Height,
                                           hz > 0 ? hz + " Hz" : "refresh N/A"),
                             i));
        }
        return l;
    }
    void Output()
    {
        Heading("Presentation");
        ReadModes();
        var modes = Options("Windowed dashboard preview", "Separate output window", "Borderless fullscreen");
        if (exclusive != null)
        {
            modes.Add(new Choice("Fullscreen · selected display mode", 4));
            modes.Add(new Choice("DXGI exclusive fullscreen", 3));
        }
        else if (outputMode >= 3)
            outputMode = 0;
        Pick("Output mode", modes, outputMode,
             c =>
             {
                 outputMode = c.Id;
                 target = 0;
                 Navigate(page);
             });
        if (outputMode != 0)
            Pick("Output monitor", MonitorChoices(), outputIndex,
                 c =>
                 {
                     outputIndex = c.Id;
                     target = 0;
                     Navigate(page);
                 });
        if (outputMode >= 3)
            ExclusiveControls();
        if (outputMode == 1)
            Pick("Output resolution · window client area",
                 outputWidths.Select((w, i) => new Choice(w + " × " + outputHeights[i], i))
                     .Where(c => outputWidths[c.Id] <= OutputScreen().WorkingArea.Width - 64 &&
                                 outputHeights[c.Id] <= OutputScreen().WorkingArea.Height - 80),
                 windowSize,
                 c =>
                 { windowSize = c.Id; });
        var mon = OutputScreen();
        int hz = outputMode >= 3 && exclusive != null ? (int)Math.Round(exclusive.Rate)
                                                      : fb_refresh(mon.DeviceName);
        double rate = Rate();
        var choices =
            new List<Choice> { new Choice("Automatic", 0), new Choice("Source FPS · originals", -1) };
        foreach (int v in new[] { 15, 24, 25, 30, 45, 48, 50, 60, 75, 90, 100, 120 })
            if (hz > 0 && v <= hz + 1 && rate > 0 &&
                (Math.Abs(v - rate) < 1 || pref.Generate && Math.Abs(v - rate * 2) < 1))
                choices.Add(new Choice(v + " fps", v));
        if (!choices.Exists(c => c.Id == target))
            target = 0;
        Pick("Output FPS", choices, target, c => target = c.Id);
        if (!aiMode && backend != -2)
            Check("Synchronize Direct3D presentation to vertical blank", vsync, v => vsync = v);
        Data("Active display", mon.Bounds.Width + " × " + mon.Bounds.Height);
        Data("Active refresh", hz > 0 ? hz + " Hz" : "N/A");
        Data("Synchronization", "Source timestamps");
        Text("Automatic preserves the source cadence or doubles it when the active display supports the " +
                 "target. Output keeps the source aspect ratio.",
             12, "Muted", 9);
    }
    void Performance()
    {
        Heading("Session telemetry");
        Label("Output presentations per second");
        var plot = new Canvas { Height = 94, Background = (Brush)root.Resources["Field"],
                                Margin = new Thickness(0, 7, 0, 5), ClipToBounds = true };
        graph = plot;
        graphLine = new Polyline { Stroke = (Brush)root.Resources["Mint"], StrokeThickness = 2 };
        graphEmpty = new TextBlock { Text = "Waiting for measurements", Margin = new Thickness(12),
                                     Foreground = (Brush)root.Resources["Muted"] };
        plot.Children.Add(graphLine);
        plot.Children.Add(graphEmpty);
        plot.SizeChanged += delegate
        {
            RefreshGraph();
        };
        Fields.Children.Add(plot);
        RefreshGraph();
        Data("Input frames", stats.inputs.ToString());
        Data("Output presentations", stats.outputs.ToString());
        Data("Generated presentations", stats.generated.ToString());
        Data("Dropped frames", stats.dropped.ToString());
        Data("Average engine preparation",
             stats.processed > 0 ? Number(stats.workMs / stats.processed) + " ms" : "N/A");
        Data("Mean pacing error",
             stats.intervals > 0 ? Number(stats.pacingErrorMs / stats.intervals) + " ms" : "N/A");
        Data("GPU motion kernels",
             stats.gpuJobs > 0 ? Number(stats.gpuTotalMs / stats.gpuJobs) + " ms" : "N/A");
        Data("Midpoint preparation",
             stats.jobs > 0 ? Number(stats.generationMs / stats.jobs) + " ms" : "N/A");
        Data("Input to presentation",
             stats.processed > 0 ? Number(stats.latencyMs / stats.processed) + " ms" : "N/A");
        Data("GPU utilization", GpuText());
        Data("Working set", (process.WorkingSet64 / 1048576) + " MiB");
        Text("Latency starts when an input frame reaches the pipeline and ends when its original is " +
                 "submitted for display. Panel scanout and source-device latency are outside this " +
                 "measurement.",
             12, "Muted", 8);
    }
    void RefreshGraph()
    {
        if (graph == null)
            return;
        graphEmpty.Visibility = history.Count < 2 ? Visibility.Visible : Visibility.Collapsed;
        if (history.Count < 2)
            return;
        var pts = new PointCollection();
        double high = Math.Max(60, history.Max());
        for (int i = 0; i < history.Count; i++)
            pts.Add(new Point(i * graph.ActualWidth / (history.Count - 1), 90 - history[i] * 84 / high));
        graphLine.Points = pts;
    }
    void RefreshFieldValues()
    {
        if (values.ContainsKey("GPU utilization"))
            values["GPU utilization"].Text = GpuText();
        if (page != "Performance")
            return;
        RefreshGraph();
        values["Input frames"].Text = stats.inputs.ToString();
        values["Output presentations"].Text = stats.outputs.ToString();
        values["Generated presentations"].Text = stats.generated.ToString();
        values["Dropped frames"].Text = stats.dropped.ToString();
        values["Average engine preparation"].Text =
            stats.processed > 0 ? Number(stats.workMs / stats.processed) + " ms" : "N/A";
        values["Mean pacing error"].Text =
            stats.intervals > 0 ? Number(stats.pacingErrorMs / stats.intervals) + " ms" : "N/A";
        values["GPU motion kernels"].Text =
            stats.gpuJobs > 0 ? Number(stats.gpuTotalMs / stats.gpuJobs) + " ms" : "N/A";
        values["Midpoint preparation"].Text =
            stats.jobs > 0 ? Number(stats.generationMs / stats.jobs) + " ms" : "N/A";
        values["Input to presentation"].Text =
            stats.processed > 0 ? Number(stats.latencyMs / stats.processed) + " ms" : "N/A";
        values["Working set"].Text = (process.WorkingSet64 / 1048576) + " MiB";
    }
    void Settings()
    {
        Heading("Application");
        Pick("Theme", Options("Midnight", "Graphite"), pref.Theme == "Graphite" ? 1 : 0,
             c =>
             {
                 pref.Theme = c.Text;
                 Save();
                 Theme();
                 Navigate(page);
             });
        Check("Start with a maximized window", pref.Maximize,
              v =>
              {
                  pref.Maximize = v;
                  Save();
              },
              false);
        Check("Write local session logs", pref.Logging,
              v =>
              {
                  pref.Logging = v;
                  Save();
              },
              false);
        if (pref.Logging)
            Text(logFolder, 12, "Muted", 6);
        Divider();
        Text("Graphics hardware", 15, "Ink", 0, true);
        foreach (var a in adapters)
        {
            Text(a.Text, 13, "Ink", 8, true);
            Text(a.Details, 12, "Muted", 2);
        }
        Divider();
        Data("Updates", "Manual local builds");
        Models();
    }
    [System.Runtime.InteropServices.DllImport("FrameBridge.Engine.dll",
                                              CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
    static extern void fb_nvidia_status(int mode, System.Text.StringBuilder text, int cap);
    static string NvidiaStatus(int mode)
    {
        var b = new System.Text.StringBuilder(512);
        fb_nvidia_status(mode, b, b.Capacity);
        return b.ToString();
    }
    string ModelPath()
    {
        return String.IsNullOrEmpty(pref.Model)
                   ? System.IO.Path.Combine(
                         Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                         "FrameBridge", "Models", "RIFE-4.9", "rife49.onnx")
                   : pref.Model;
    }
    bool AiAvailable()
    {
        return File.Exists(ModelPath()) &&
               File.Exists(System.IO.Path.Combine(
                   Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "FrameBridge",
                   "AI", "1.24.4", "onnxruntime.dll"));
    }
    void Models()
    {
        Divider();
        Text("AI models", 15, "Ink", 0, true);
        Data("Selected model",
             File.Exists(ModelPath()) ? System.IO.Path.GetFileName(ModelPath()) : "None installed");
        Text(AiAvailable()
                 ? "ONNX Runtime installed. Model inputs and outputs are checked when inference starts."
                 : "Run Tools/Install-AI.ps1 to install the pinned RIFE model and Microsoft inference " +
                       "libraries.",
             12, "Muted", 5);
        Action("Select ONNX model…", delegate {
            var d = new OpenFileDialog { Filter = "ONNX model|*.onnx" };
            if (d.ShowDialog(this) == true)
            {
                pref.Model = d.FileName;
                aiMode = false;
                Save();
                Navigate(page);
            }
        }, !Busy);
        if (!String.IsNullOrEmpty(pref.Model))
            Action("Use installed RIFE model", delegate {
                pref.Model = "";
                Save();
                aiMode = false;
                Navigate(page);
            }, !Busy);
    }
    void About()
    {
        Heading("FrameBridge");
        Text("Created by Faris Alsari / FMSGaming", 15, "Ink", 10, true);
        Text("Famous Gaming", 14, "Mint", 2);
        Data("Project version", "0.1.0");
        Data("Original project license", "Selection pending");
        Divider();
        Text("Third-party acknowledgements", 15, "Ink", 0, true);
        Text("NVIDIA Optical Flow interface headers · NVIDIA Corporation & Affiliates · MIT License.", 12, "Muted", 6);
        Text("Microsoft Windows provides WPF, Media Foundation, DXGI, Direct3D 11 and the HLSL compiler. " +
                 "FrameBridge uses LLVM-MinGW to build the native engine. Optional AI inference uses " +
                 "Microsoft " +
                 "ONNX Runtime and DirectML. RIFE is by Megvii and hzwer, with an ONNX export by yuvraj108c.",
             13, "Muted", 8);
        Text("Third-party components retain their respective licenses. Notices are included with the local " +
                 "build.",
             12, "Muted", 8);
    }
}
}
