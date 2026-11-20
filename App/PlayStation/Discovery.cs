// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text;
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
            if (nic.OperationalStatus != OperationalStatus.Up || nic.NetworkInterfaceType == NetworkInterfaceType.Loopback)
                continue;
            foreach (var local in nic.GetIPProperties().UnicastAddresses)
            {
                if (local.Address.AddressFamily != AddressFamily.InterNetwork || local.IPv4Mask == null)
                    continue;
                var addr = local.Address.GetAddressBytes();
                var mask = local.IPv4Mask.GetAddressBytes();
                if (mask.All(v => v == 255) || mask.All(v => v == 0)) continue;
                for (int i = 0; i < addr.Length; i++) addr[i] |= (byte)~mask[i];
                targets.Add(new IPAddress(addr));
            }
        }
        return targets;
    }
    internal static string Parse(string message, IPAddress address)
    {
        var lines = message.TrimEnd('\0').Split('\n').Select(l => l.Trim()).ToArray();
        if (lines.Length < 2) return null;
        var status = lines[0].Split(' ');
        if (status.Length < 2 || status[0] != "HTTP/1.1" || (status[1] != "200" && status[1] != "620")) return null;
        var headers = new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        foreach (var line in lines.Skip(1))
        {
            int split = line.IndexOf(':');
            if (split > 0) headers[line.Substring(0,split)] = line.Substring(split+1).Trim();
        }
        string version,name;
        if (!headers.TryGetValue("device-discovery-protocol-version",out version) || version != Version) return null;
        if (!headers.TryGetValue("host-name",out name) || name.Length == 0) name = "PS5";
        name = new string(name.Where(c => !Char.IsControl(c)).Take(80).ToArray());
        return name + " · " + address + (status[1]=="620" ? " · Rest mode" : " · Awake");
    }
    internal static List<string> Scan()
    {
        var found = new Dictionary<IPAddress,string>();
        var request = Encoding.ASCII.GetBytes("SRCH * HTTP/1.1\ndevice-discovery-protocol-version:"+Version+"\n");
        using (var udp = new UdpClient(new IPEndPoint(IPAddress.Any,0)))
        {
            udp.EnableBroadcast=true;
            int sent=0;
            foreach(var address in Broadcasts())
                try { udp.Send(request,request.Length,new IPEndPoint(address,Port)); ++sent; }
                catch(SocketException) { }
            if(sent==0) throw new InvalidOperationException("No IPv4 LAN interface could send a discovery request.");
            var watch=Stopwatch.StartNew();
            while(watch.ElapsedMilliseconds<1600)
            {
                if(!udp.Client.Poll(50000,SelectMode.SelectRead)) continue;
                var sender=new IPEndPoint(IPAddress.Any,0);
                var bytes=udp.Receive(ref sender);
                if(bytes.Length>4096) continue;
                var console=Parse(Encoding.UTF8.GetString(bytes),sender.Address);
                if(console!=null) found[sender.Address]=console;
            }
        }
        return found.Values.ToList();
    }
}
internal sealed partial class Shell
{
    string psStatus="Ready to search the local network.";
    List<string> consoles=new List<string>();
    bool scanningPs5;
    void PlayStationInput()
    {
        Data("Connection","Disconnected");
        Data("Stream","No active stream");
        Text(psStatus,12,"Muted",8);
        foreach(var console in consoles) Text(console);
        Action("Find PS5 consoles",async delegate {
            scanningPs5=true;
            psStatus="Searching the local network…";
            consoles.Clear();
            Navigate(page);
            try {
                consoles=await System.Threading.Tasks.Task.Run((Func<List<string>>)PlayStationDiscovery.Scan);
                psStatus=consoles.Count==0 ? "No PS5 replied. Check the LAN and the console’s Remote Play setting." : "Discovery complete.";
            } catch(Exception ex) { psStatus=ex.Message; }
            finally { scanningPs5=false; if(!closing && page=="Video inputs" && kind==4) Navigate(page); }
        }, !scanningPs5);
        Text("Pairing, streaming and controller input require a session backend. Discovery does not register the console.",12,"Muted",8);
    }
}
}
