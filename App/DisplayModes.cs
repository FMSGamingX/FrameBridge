// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using Forms = System.Windows.Forms;
namespace FrameBridge
{
internal sealed partial class Shell
{
    sealed class OutputOwner : Forms.IWin32Window
    {
        public IntPtr Handle { get; set; }
    }
    class DisplayMode
    {
        public int Width, Height, Num, Den, Adapter;
        public double Rate
        {
            get {
                return (double)Num / Den;
            }
        }
    }
    List<DisplayMode> exclusiveModes = new List<DisplayMode>();
    DisplayMode exclusive;
    Forms.Form exclusiveWindow;
    [DllImport("FrameBridge.Engine.dll", CharSet = CharSet.Unicode)]
    static extern int fb_display_modes(string name, int adapter, StringBuilder list, int cap);
    void ReadModes()
    {
        var screen = screens[outputIndex];
        var text = new StringBuilder(150000);
        exclusiveModes.Clear();
        if (backend == -2 || aiMode || fb_display_modes(screen.DeviceName, backend, text, text.Capacity) <= 0)
        {
            exclusive = null;
            return;
        }
        foreach (var line in text.ToString().Split('\n'))
        {
            if (line.Length == 0)
                continue;
            var n = line.Split('|').Select(int.Parse).ToArray();
            exclusiveModes.Add(
                new DisplayMode { Width = n[0], Height = n[1], Num = n[2], Den = n[3], Adapter = n[4] });
        }
        if (exclusive == null ||
            !exclusiveModes.Any(m => m.Width == exclusive.Width && m.Height == exclusive.Height &&
                                     m.Num == exclusive.Num && m.Den == exclusive.Den &&
                                     m.Adapter == exclusive.Adapter))
        {
            int hz = fb_refresh(screen.DeviceName);
            exclusive = exclusiveModes
                            .OrderBy(m => Math.Abs(m.Width - screen.Bounds.Width) +
                                          Math.Abs(m.Height - screen.Bounds.Height))
                            .ThenBy(m => Math.Abs(m.Rate - hz))
                            .FirstOrDefault();
        }
    }
    void ExclusiveControls()
    {
        if (exclusive == null)
            return;
        var sizes =
            exclusiveModes.GroupBy(m => Tuple.Create(m.Width, m.Height)).Select(g => g.First()).ToList();
        var opts = sizes.Select((m, i) => new Choice(m.Width + " × " + m.Height, i));
        Pick("Output resolution", opts,
             sizes.FindIndex(m => m.Width == exclusive.Width && m.Height == exclusive.Height),
             c =>
             {
                 exclusive = sizes[c.Id];
                 target = 0;
                 Navigate(page);
             });
        var rates = exclusiveModes.Where(m => m.Width == exclusive.Width && m.Height == exclusive.Height)
                        .OrderBy(m => m.Rate)
                        .ToList();
        Pick("Output refresh rate", rates.Select((m, i) => new Choice(m.Rate.ToString("0.###") + " Hz", i)),
             rates.FindIndex(m => m.Num == exclusive.Num && m.Den == exclusive.Den),
             c =>
             {
                 exclusive = rates[c.Id];
                 target = 0;
                 Navigate(page);
             });
    }
    IntPtr savedDisplay;
    [DllImport("FrameBridge.Engine.dll", CharSet = CharSet.Unicode)]
    static extern IntPtr fb_select_display(string name, int width, int height, int num, int den);
    [DllImport("FrameBridge.Engine.dll")]
    static extern int fb_restore_display(IntPtr saved);
    IntPtr OpenExclusive(Forms.Screen monitor, ref Config c)
    {
        ReadModes();
        if (exclusive == null)
            throw new InvalidOperationException(
                "This output has no compatible exclusive display mode on the selected backend.");
        bool managed = outputMode == 4;
        c.exclusive = managed ? 0 : 1;
        c.outputW = exclusive.Width;
        c.outputH = exclusive.Height;
        c.rateNum = exclusive.Num;
        c.rateDen = exclusive.Den;
        c.refresh = (int)Math.Round(exclusive.Rate);
        c.backend = exclusive.Adapter;
        if (managed)
        {
            savedDisplay = fb_select_display(monitor.DeviceName, c.outputW, c.outputH, c.rateNum, c.rateDen);
            if (savedDisplay == IntPtr.Zero)
                throw new InvalidOperationException("Windows declined the requested monitor mode.");
        }
        exclusiveWindow =
            new Forms.Form { Text = managed ? "FrameBridge · Display-mode output"
                                            : "FrameBridge · Fullscreen output",
                             FormBorderStyle = Forms.FormBorderStyle.None,
                             StartPosition = Forms.FormStartPosition.Manual,
                             Bounds = new System.Drawing.Rectangle(
                                 monitor.Bounds.Location, new System.Drawing.Size(c.outputW, c.outputH)),
                             KeyPreview = true,
                             BackColor = System.Drawing.Color.Black };
        exclusiveWindow.FormClosing += async delegate(object sender, Forms.FormClosingEventArgs e)
        {
            if (session != IntPtr.Zero)
            {
                e.Cancel = true;
                await Finish("Output window closed.");
            }
        };
        exclusiveWindow.KeyDown += async delegate(object sender, Forms.KeyEventArgs e)
        {
            if (e.KeyCode == Forms.Keys.Escape)
                await Finish("Output window closed.");
        };
        exclusiveWindow.Show(
            new OutputOwner { Handle = new System.Windows.Interop.WindowInteropHelper(this).Handle });
        exclusiveWindow.BringToFront();
        exclusiveWindow.Activate();
        exclusiveWindow.Focus();
        exclusiveWindow.Update();
        return exclusiveWindow.Handle;
    }
    void CloseExclusive()
    {
        if (exclusiveWindow != null)
        {
            exclusiveWindow.Dispose();
            exclusiveWindow = null;
        }
        if (savedDisplay != IntPtr.Zero)
        {
            int restored = fb_restore_display(savedDisplay);
            savedDisplay = IntPtr.Zero;
            if (restored == 0)
                Message("Windows could not restore the previous monitor mode.");
        }
    }
}
}
