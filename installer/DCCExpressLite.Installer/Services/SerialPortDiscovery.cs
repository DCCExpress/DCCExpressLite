using Microsoft.Win32;
using System.Runtime.InteropServices;

namespace DCCExpressLite.Installer.Services;

public static class SerialPortDiscovery
{
    public static IReadOnlyList<string> GetPorts()
    {
        var ports = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        if (OperatingSystem.IsWindows())
        {
            using var key = Registry.LocalMachine.OpenSubKey(@"HARDWARE\DEVICEMAP\SERIALCOMM");
            if (key is not null)
            {
                foreach (var name in key.GetValueNames())
                    if (key.GetValue(name) is string port && !string.IsNullOrWhiteSpace(port)) ports.Add(port);
            }
        }
        else
        {
            string[] patterns = OperatingSystem.IsMacOS()
                ? ["cu.usbserial*", "cu.usbmodem*", "cu.SLAB_USBtoUART*", "cu.wchusbserial*"]
                : ["ttyUSB*", "ttyACM*", "ttyAMA*"];

            foreach (var pattern in patterns)
                foreach (var path in Directory.EnumerateFiles("/dev", pattern)) ports.Add(path);
        }

        return ports.OrderBy(value => value, StringComparer.OrdinalIgnoreCase).ToArray();
    }

    public static string RuntimeId()
    {
        var architecture = RuntimeInformation.ProcessArchitecture switch
        {
            Architecture.Arm64 => "arm64",
            Architecture.X86 => "x86",
            _ => "x64",
        };
        if (OperatingSystem.IsWindows()) return $"win-{architecture}";
        if (OperatingSystem.IsMacOS()) return $"osx-{architecture}";
        return $"linux-{architecture}";
    }
}
