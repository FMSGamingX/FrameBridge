// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows;
[assembly:AssemblyTitle("FrameBridge")]
[assembly:AssemblyProduct("FrameBridge")]
[assembly:AssemblyCompany("Famous Gaming")]
[assembly:AssemblyCopyright("Created by Faris Alsari / FMSGaming — Famous Gaming")]
[assembly:AssemblyVersion("0.1.0.0")]
namespace FrameBridge
{
internal static partial class Native
{
    const string Library = "FrameBridge.Engine.dll";
    [DllImport(Library)]
    internal static extern int fb_version();
    [DllImport(Library, CharSet = CharSet.Unicode)]
    internal static extern int fb_adapter(int index, StringBuilder name, int length);
}
internal static class Program
{
    [STAThread]
    public static int Main(string[] args)
    {
        try
        {
            if (Native.fb_version() != 2)
                throw new Exception("The engine and application versions differ.");
            var app = new Application();
            var window = new Shell();
            if (args.Length > 0 && (args[0] == "--shell-test" || args[0] == "--ui-test"))
                window.PlaceTestWindow();
            if (args.Length > 0 && args[0] == "--shell-test")
            {
                window.Show();
                window.UpdateLayout();
                window.Close();
                return 0;
            }
            if (args.Length == 2 && args[0] == "--ui-test")
            {
                int result = 1;
                window.Loaded += async delegate
                {
                    try
                    {
                        await window.Exercise(args[1]);
                        result = 0;
                    }
                    catch (Exception e)
                    {
                        System.IO.File.WriteAllText(System.IO.Path.Combine(args[1], "ui-error.txt"),
                                                    e.ToString());
                    }
                    finally
                    {
                        window.Close();
                    }
                };
                app.Run(window);
                return result;
            }
            app.Run(window);
            return 0;
        }
        catch (Exception e)
        {
            if (args.Length > 0)
            {
                Console.Error.WriteLine(e);
                return 1;
            }
            MessageBox.Show(e.Message, "FrameBridge", MessageBoxButton.OK, MessageBoxImage.Error);
            return 1;
        }
    }
}
}
