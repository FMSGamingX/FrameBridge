// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text;
using System.Runtime.InteropServices;
using System.Windows.Controls;
namespace FrameBridge
{
internal static class PlayStationDiscovery
{
    // The public chiaki-ng discovery header identifies this PS5 LAN protocol version and port.
    const string Version = "00030010";
    const int Port = 9302;
    internal static IEnumerable<IPAddress> Broadcasts()
    {
        var targets = new HashSet<IPAddress>();
        foreach (var nic in NetworkInterface.GetAllNetworkInterfaces())
        {
            if (nic.OperationalStatus != OperationalStatus.Up ||
                nic.NetworkInterfaceType == NetworkInterfaceType.Loopback)
                continue;
            foreach (var local in nic.GetIPProperties().UnicastAddresses)
            {
                if (local.Address.AddressFamily != AddressFamily.InterNetwork || local.IPv4Mask == null)
                    continue;
                var addr = local.Address.GetAddressBytes();
                var mask = local.IPv4Mask.GetAddressBytes();
                if (mask.All(v => v == 255) || mask.All(v => v == 0))
                    continue;
                for (int i = 0; i < addr.Length; i++)
                    addr[i] |= (byte)~mask[i];
                targets.Add(new IPAddress(addr));
            }
        }
        return targets;
    }
    internal static string Parse(string message, IPAddress address)
    {
        var lines = message.TrimEnd('\0').Split('\n').Select(l => l.Trim()).ToArray();
        if (lines.Length < 2)
            return null;
        var status = lines[0].Split(' ');
        if (status.Length < 2 || status[0] != "HTTP/1.1" || (status[1] != "200" && status[1] != "620"))
            return null;
        var headers = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (var line in lines.Skip(1))
        {
            int split = line.IndexOf(':');
            if (split > 0)
                headers[line.Substring(0, split)] = line.Substring(split + 1).Trim();
        }
        string version, name;
        if (!headers.TryGetValue("device-discovery-protocol-version", out version) || version != Version)
            return null;
        if (!headers.TryGetValue("host-name", out name) || name.Length == 0)
            name = "PS5";
        name = new string(name.Where(c => !Char.IsControl(c)).Take(80).ToArray());
        return name + " · " + address + (status[1] == "620" ? " · Rest mode" : " · Awake");
    }
    internal static List<string> Scan()
    {
        var found = new Dictionary<IPAddress, string>();
        var request =
            Encoding.ASCII.GetBytes("SRCH * HTTP/1.1\ndevice-discovery-protocol-version:" + Version + "\n");
        using (var udp = new UdpClient(new IPEndPoint(IPAddress.Any, 0)))
        {
            udp.EnableBroadcast = true;
            int sent = 0;
            foreach (var address in Broadcasts())
                try
                {
                    udp.Send(request, request.Length, new IPEndPoint(address, Port));
                    ++sent;
                }
                catch (SocketException)
                {
                }
            if (sent == 0)
                throw new InvalidOperationException("No IPv4 LAN interface could send a discovery request.");
            var watch = Stopwatch.StartNew();
            while (watch.ElapsedMilliseconds < 1600)
            {
                if (!udp.Client.Poll(50000, SelectMode.SelectRead))
                    continue;
                var sender = new IPEndPoint(IPAddress.Any, 0);
                var bytes = udp.Receive(ref sender);
                if (bytes.Length > 4096)
                    continue;
                var console = Parse(Encoding.UTF8.GetString(bytes), sender.Address);
                if (console != null)
                    found[sender.Address] = console;
            }
        }
        return found.Values.ToList();
    }
}
internal sealed partial class Shell
{
    string psStatus = "Ready to search the local network.";
    List<string> consoles = new List<string>();
    bool scanningPs5;
    bool pairingPs5, pairedPs5;
    string psHost = "";
    int psFps = 30;
    [DllImport("FrameBridge.Engine.dll")]
    static extern int fb_ps_available();
    [DllImport("FrameBridge.Engine.dll", CharSet = CharSet.Ansi)]
    static extern int fb_ps_paired(string host);
    [DllImport("FrameBridge.Engine.dll", CharSet = CharSet.Unicode)]
    static extern int fb_ps_pair([MarshalAs(UnmanagedType.LPStr)] string host, byte[] account, uint code,
                                 [MarshalAs(UnmanagedType.LPStr)] string login, StringBuilder message,
                                 int length);
    bool ValidPsAddress(string address)
    {
        IPAddress parsed;
        return IPAddress.TryParse(address, out parsed) &&
               parsed.AddressFamily == AddressFamily.InterNetwork && parsed.ToString() == address;
    }
    void SelectPsHost(string host)
    {
        psHost = host.Trim();
        pairedPs5 = ValidPsAddress(psHost) && fb_ps_paired(psHost) != 0;
    }
    void PlayStationInput()
    {
        Data("Connection", Busy ? (stats.inputs > 0 ? "Streaming" : "Connecting")
                                : (pairedPs5 ? "Paired · disconnected" : "Disconnected"));
        Data("Stream", Busy && stats.inputs > 0
                           ? stats.width + " × " + stats.height + " · " + stats.inputs + " frames received"
                           : "No active stream");
        Text(psStatus, 12, "Muted", 8);
        foreach (var console in consoles)
        {
            var parts = console.Split(new[] { " · " }, StringSplitOptions.None);
            var host = parts[parts.Length - 2];
            Action(console, delegate {
                SelectPsHost(host);
                Navigate(page);
            }, !Busy && !pairingPs5);
        }
        Action("Find PS5 consoles", async delegate {
            scanningPs5 = true;
            psStatus = "Searching the local network…";
            consoles.Clear();
            Navigate(page);
            try
            {
                consoles =
                    await System.Threading.Tasks.Task.Run((Func<List<string>>)PlayStationDiscovery.Scan);
                if (consoles.Count == 1 && psHost.Length == 0)
                {
                    var parts = consoles[0].Split(new[] { " · " }, StringSplitOptions.None);
                    SelectPsHost(parts[parts.Length - 2]);
                }
                psStatus = consoles.Count == 0
                               ? "No PS5 replied. Check the LAN and the console’s Remote Play setting."
                               : "Discovery complete.";
            }
            catch (Exception ex)
            {
                psStatus = ex.Message;
            }
            finally
            {
                scanningPs5 = false;
                if (!closing && page == "Video inputs" && kind == 4)
                    Navigate(page);
            }
        }, !scanningPs5 && !Busy && !pairingPs5);
        if (fb_ps_available() == 0)
        {
            Text("Optional PS5 module unavailable. Build it with Tools/Build-PlayStation.ps1, then restart " +
                 "FrameBridge.",
                 12, "Muted", 8);
            return;
        }
        Label("Console IPv4 address");
        var address = new TextBox { Text = psHost, IsEnabled = !Busy && !pairingPs5 };
        Fields.Children.Add(address);
        address.TextChanged += delegate
        {
            SelectPsHost(address.Text);
            Decorate();
        };
        Pick("Requested input FPS", new[] { new Choice("30 fps", 30), new Choice("60 fps", 60) }, psFps,
             c =>
             {
                 psFps = c.Id;
                 target = 0;
             });
        if (Busy)
        {
            Action("Disconnect PS5", async delegate { await Finish("Disconnected from PS5."); });
            return;
        }
        Label("PSN account ID · Base64");
        var account = new TextBox();
        Fields.Children.Add(account);
        Label("Pairing code · PS5 Settings → System → Remote Play → Pair Device");
        var code = new PasswordBox { MaxLength = 8 };
        Fields.Children.Add(code);
        Label("Console login passcode · optional");
        var login = new PasswordBox { MaxLength = 4 };
        Fields.Children.Add(login);
        Action("Pair PS5", async delegate {
            byte[] id;
            try
            {
                id = Convert.FromBase64String(account.Text.Trim());
            }
            catch (FormatException)
            {
                id = new byte[0];
            }
            uint number;
            string optional = login.Password;
            if (!ValidPsAddress(psHost) || id.Length != 8 || code.Password.Length != 8 ||
                !code.Password.All(ch => ch >= '0' && ch <= '9') ||
                !UInt32.TryParse(code.Password, out number) ||
                (optional.Length != 0 &&
                 (optional.Length != 4 || !optional.All(ch => ch >= '0' && ch <= '9'))))
            {
                Message("Enter the console IPv4 address, Base64 account ID, eight-digit pairing code and " +
                        "optional four-digit login passcode.");
                return;
            }
            string host = psHost;
            code.Clear();
            login.Clear();
            pairingPs5 = true;
            psStatus = "Pairing with the console…";
            Navigate(page);
            try
            {
                var result = new StringBuilder(384);
                await System.Threading.Tasks.Task.Run(
                    () => fb_ps_pair(host, id, number, optional, result, result.Capacity));
                SelectPsHost(host);
                psStatus = result.ToString();
            }
            finally
            {
                Array.Clear(id, 0, id.Length);
                pairingPs5 = false;
                if (!closing)
                    Navigate(page);
            }
        }, !scanningPs5 && !pairingPs5);
        Text("After pairing, choose Start processing. Registration keys are stored in Windows Credential " +
             "Manager.",
             12, "Muted", 8);
    }
}
}
