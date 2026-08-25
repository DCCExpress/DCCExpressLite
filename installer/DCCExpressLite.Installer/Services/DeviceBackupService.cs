using System.Net.Http.Json;
using System.Text;

namespace DCCExpressLite.Installer.Services;

public sealed record DeviceBackup(string Layout, string Locos);

public sealed class DeviceBackupService
{
    private readonly HttpClient _http = new() { Timeout = TimeSpan.FromSeconds(5) };

    public async Task<DeviceBackup> BackupAsync(string deviceAddress, CancellationToken cancellationToken)
    {
        var root = Normalize(deviceAddress);
        var layout = await _http.GetStringAsync(new Uri(root, "/api/layout"), cancellationToken);
        var locos = await _http.GetStringAsync(new Uri(root, "/api/locos"), cancellationToken);
        return new DeviceBackup(layout, locos);
    }

    public async Task RestoreAsync(string deviceAddress, DeviceBackup backup, Action<string> log, CancellationToken cancellationToken)
    {
        var root = Normalize(deviceAddress);
        for (var attempt = 1; attempt <= 30; attempt++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            try
            {
                using var ping = await _http.GetAsync(root, cancellationToken);
                if (ping.IsSuccessStatusCode) break;
            }
            catch when (attempt < 30) { }
            log($"Waiting for EX-CSB1 to restart ({attempt}/30)...");
            await Task.Delay(1000, cancellationToken);
        }

        await SendJsonAsync(new Uri(root, "/api/layout"), backup.Layout, cancellationToken);
        await SendJsonAsync(new Uri(root, "/api/locos"), backup.Locos, cancellationToken);
    }

    private async Task SendJsonAsync(Uri uri, string json, CancellationToken cancellationToken)
    {
        using var content = new StringContent(json, Encoding.UTF8, "application/json");
        using var response = await _http.PostAsync(uri, content, cancellationToken);
        response.EnsureSuccessStatusCode();
    }

    private static Uri Normalize(string value)
    {
        var text = value.Trim().TrimEnd('/');
        if (!text.Contains("://", StringComparison.Ordinal)) text = "http://" + text;
        return new Uri(text + "/");
    }
}
