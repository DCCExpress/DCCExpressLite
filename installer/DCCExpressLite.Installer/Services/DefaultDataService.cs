using System.IO.Compression;
using System.Net.Http.Headers;
using System.Text;

namespace DCCExpressLite.Installer.Services;

public sealed class DefaultDataService
{
    private readonly HttpClient _http = new() { Timeout = TimeSpan.FromSeconds(8) };

    public async Task InstallAsync(
        string deviceAddress,
        string archivePath,
        Action<string> log,
        CancellationToken cancellationToken)
    {
        var root = Normalize(deviceAddress);
        await WaitUntilReadyAsync(root, log, cancellationToken);

        var extractionRoot = Path.Combine(Path.GetTempPath(), "DCCExpressLite", "default-data", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(extractionRoot);
        try
        {
            ZipFile.ExtractToDirectory(archivePath, extractionRoot);
            var layoutPath = Path.Combine(extractionRoot, "layout.json");
            var locosPath = Path.Combine(extractionRoot, "locos.json");
            if (!File.Exists(layoutPath) || !File.Exists(locosPath))
                throw new InvalidDataException("The default data package must contain layout.json and locos.json.");

            await SendJsonAsync(new Uri(root, "/api/layout"), await File.ReadAllTextAsync(layoutPath, cancellationToken), cancellationToken);
            log("Default layout uploaded.");
            await SendJsonAsync(new Uri(root, "/api/locos"), await File.ReadAllTextAsync(locosPath, cancellationToken), cancellationToken);
            log("Default locomotives uploaded.");

            foreach (var imagePath in Directory.EnumerateFiles(extractionRoot, "*.png", SearchOption.TopDirectoryOnly))
            {
                await UploadFileAsync(new Uri(root, "/upload"), imagePath, cancellationToken);
                log($"Locomotive image uploaded: {Path.GetFileName(imagePath)}");
            }
        }
        finally
        {
            if (Directory.Exists(extractionRoot)) Directory.Delete(extractionRoot, true);
        }
    }

    private async Task WaitUntilReadyAsync(Uri root, Action<string> log, CancellationToken cancellationToken)
    {
        for (var attempt = 1; attempt <= 30; attempt++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            try
            {
                using var response = await _http.GetAsync(root, cancellationToken);
                if (response.IsSuccessStatusCode) return;
            }
            catch when (attempt < 30) { }
            log($"Waiting for EX-CSB1 to restart ({attempt}/30)...");
            await Task.Delay(1000, cancellationToken);
        }
        throw new TimeoutException($"EX-CSB1 did not become reachable at {root}.");
    }

    private async Task SendJsonAsync(Uri uri, string json, CancellationToken cancellationToken)
    {
        using var content = new StringContent(json, Encoding.UTF8, "application/json");
        using var response = await _http.PostAsync(uri, content, cancellationToken);
        response.EnsureSuccessStatusCode();
    }

    private async Task UploadFileAsync(Uri uri, string path, CancellationToken cancellationToken)
    {
        await using var stream = File.OpenRead(path);
        using var file = new StreamContent(stream);
        file.Headers.ContentType = new MediaTypeHeaderValue("image/png");
        using var form = new MultipartFormDataContent();
        form.Add(file, "file", Path.GetFileName(path));
        form.Add(new StringContent(""), "target");
        using var response = await _http.PostAsync(uri, form, cancellationToken);
        response.EnsureSuccessStatusCode();
    }

    private static Uri Normalize(string value)
    {
        var text = value.Trim().TrimEnd('/');
        if (!text.Contains("://", StringComparison.Ordinal)) text = "http://" + text;
        return new Uri(text + "/");
    }
}
