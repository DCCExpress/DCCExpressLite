using DCCExpressLite.Installer.Models;
using System.IO.Compression;
using System.Formats.Tar;
using System.Security.Cryptography;
using System.Text.Json;

namespace DCCExpressLite.Installer.Services;

public sealed class FirmwarePackageService
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        PropertyNameCaseInsensitive = true,
    };
    private readonly HttpClient _http = new() { Timeout = TimeSpan.FromMinutes(10) };

    public FirmwarePackageService()
    {
        _http.DefaultRequestHeaders.UserAgent.ParseAdd("DCCExpressLite-Installer/1.0");
    }

    public async Task<ResolvedFirmware> LoadManifestAsync(string source, CancellationToken cancellationToken)
    {
        string json;
        Uri sourceUri;
        string? localPath = null;

        if (Uri.TryCreate(source, UriKind.Absolute, out var uri) && (uri.Scheme == "http" || uri.Scheme == "https"))
        {
            sourceUri = uri;
            json = await _http.GetStringAsync(uri, cancellationToken);
        }
        else
        {
            localPath = Path.GetFullPath(source);
            json = await File.ReadAllTextAsync(localPath, cancellationToken);
            sourceUri = new Uri(localPath);
        }

        var manifest = JsonSerializer.Deserialize<FirmwareManifest>(json, JsonOptions)
            ?? throw new InvalidDataException("The firmware manifest is empty or invalid.");
        if (manifest.SchemaVersion != 1) throw new InvalidDataException($"Unsupported manifest schema: {manifest.SchemaVersion}.");
        if (manifest.Images.Count == 0) throw new InvalidDataException("The manifest contains no flash images.");
        return new ResolvedFirmware(manifest, sourceUri, localPath);
    }

    public async Task<PreparedFlash> PrepareAsync(
        ResolvedFirmware release,
        IEnumerable<FirmwareImage> selectedImages,
        IProgress<(double Value, string Message)> progress,
        CancellationToken cancellationToken)
    {
        var cache = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "DCCExpressLite", "cache", Sanitize(release.Manifest.Version));
        Directory.CreateDirectory(cache);

        var selected = selectedImages.ToArray();
        var downloaded = new List<DownloadedImage>();
        for (var index = 0; index < selected.Length; index++)
        {
            var image = selected[index];
            progress.Report(((double)index / Math.Max(1, selected.Length + 1) * 35, $"Downloading {image.Name}..."));
            var path = Path.Combine(cache, Path.GetFileName(image.Name));
            await DownloadOrCopyAsync(release, image.Url, path, cancellationToken);
            await VerifyHashAsync(path, image.Sha256, cancellationToken);
            downloaded.Add(new DownloadedImage(image, path));
        }

        var runtimeId = SerialPortDiscovery.RuntimeId();
        if (!release.Manifest.Tools.TryGetValue(runtimeId, out var tool))
        {
            var compatibleKey = release.Manifest.Tools.Keys.FirstOrDefault(key =>
                key.StartsWith(runtimeId.Split('-')[0] + "-", StringComparison.OrdinalIgnoreCase));
            if (compatibleKey is null) throw new InvalidDataException($"No esptool is defined for {runtimeId}.");
            tool = release.Manifest.Tools[compatibleKey];
        }

        progress.Report((38, $"Preparing flash tool for {runtimeId}..."));
        var toolDownload = Path.Combine(cache, Path.GetFileName(ResolveUri(release, tool.Url).LocalPath));
        await DownloadOrCopyAsync(release, tool.Url, toolDownload, cancellationToken);
        await VerifyHashAsync(toolDownload, tool.Sha256, cancellationToken);

        var toolPath = toolDownload;
        if (!string.IsNullOrWhiteSpace(tool.ArchiveEntry))
        {
            var toolsDirectory = Path.Combine(cache, "tools", runtimeId);
            Directory.CreateDirectory(toolsDirectory);
            ExtractArchive(toolDownload, toolsDirectory);
            toolPath = Path.Combine(toolsDirectory, tool.ArchiveEntry.Replace('/', Path.DirectorySeparatorChar));
        }
        if (!File.Exists(toolPath)) throw new FileNotFoundException("The configured esptool executable was not found.", toolPath);

        if (!OperatingSystem.IsWindows())
        {
            try { File.SetUnixFileMode(toolPath, File.GetUnixFileMode(toolPath) | UnixFileMode.UserExecute); }
            catch (PlatformNotSupportedException) { }
        }

        return new PreparedFlash(toolPath, tool.ArgumentsPrefix, downloaded);
    }

    private async Task DownloadOrCopyAsync(ResolvedFirmware release, string source, string destination, CancellationToken cancellationToken)
    {
        var uri = ResolveUri(release, source);
        if (uri.IsFile)
        {
            File.Copy(uri.LocalPath, destination, true);
            return;
        }

        await using var input = await _http.GetStreamAsync(uri, cancellationToken);
        await using var output = File.Create(destination);
        await input.CopyToAsync(output, cancellationToken);
    }

    private static Uri ResolveUri(ResolvedFirmware release, string value)
    {
        if (Uri.TryCreate(value, UriKind.Absolute, out var absolute)) return absolute;
        if (release.LocalManifestPath is not null)
            return new Uri(Path.GetFullPath(Path.Combine(Path.GetDirectoryName(release.LocalManifestPath)!, value)));
        return new Uri(release.SourceUri, value);
    }

    private static async Task VerifyHashAsync(string path, string expected, CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(expected)) return;
        await using var stream = File.OpenRead(path);
        var actual = Convert.ToHexString(await SHA256.HashDataAsync(stream, cancellationToken));
        var normalized = expected.Replace("sha256:", "", StringComparison.OrdinalIgnoreCase).Trim();
        if (!actual.Equals(normalized, StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException($"SHA-256 mismatch for {Path.GetFileName(path)}.");
    }

    private static void ExtractArchive(string archivePath, string destination)
    {
        if (archivePath.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
        {
            ZipFile.ExtractToDirectory(archivePath, destination, true);
            return;
        }
        if (archivePath.EndsWith(".tar.gz", StringComparison.OrdinalIgnoreCase) || archivePath.EndsWith(".tgz", StringComparison.OrdinalIgnoreCase))
        {
            using var file = File.OpenRead(archivePath);
            using var gzip = new GZipStream(file, CompressionMode.Decompress);
            TarFile.ExtractToDirectory(gzip, destination, true);
            return;
        }
        throw new InvalidDataException($"Unsupported tool archive: {Path.GetFileName(archivePath)}.");
    }

    private static string Sanitize(string value) => string.Concat(value.Select(c => Path.GetInvalidFileNameChars().Contains(c) ? '_' : c));
}
