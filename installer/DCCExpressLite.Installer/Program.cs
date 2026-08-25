using Avalonia;
using DCCExpressLite.Installer.Services;

namespace DCCExpressLite.Installer;

internal static class Program
{
    [STAThread]
    public static void Main(string[] args)
    {
        if (args.Length == 2 && args[0] == "--validate-manifest")
        {
            ValidateManifestAsync(args[1]).GetAwaiter().GetResult();
            return;
        }
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    public static AppBuilder BuildAvaloniaApp() => AppBuilder
        .Configure<App>()
        .UsePlatformDetect()
        .WithInterFont()
        .LogToTrace();

    private static async Task ValidateManifestAsync(string source)
    {
        var service = new FirmwarePackageService();
        var release = await service.LoadManifestAsync(source, CancellationToken.None);
        var progress = new Progress<(double Value, string Message)>(value => Console.WriteLine($"{value.Value,5:0}% {value.Message}"));
        var prepared = await service.PrepareAsync(release, release.Manifest.Images, progress, CancellationToken.None);
        Console.WriteLine($"VALID: {release.Manifest.Product} {release.Manifest.Version}");
        Console.WriteLine($"Tool: {prepared.ToolPath}");
        Console.WriteLine($"Images: {prepared.Images.Count}");
    }
}
