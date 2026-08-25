using System.Text.Json.Serialization;

namespace DCCExpressLite.Installer.Models;

public sealed class FirmwareManifest
{
    [JsonPropertyName("schemaVersion")]
    public int SchemaVersion { get; set; } = 1;

    [JsonPropertyName("product")]
    public string Product { get; set; } = "DCCExpressLite";

    [JsonPropertyName("version")]
    public string Version { get; set; } = "unknown";

    [JsonPropertyName("channel")]
    public string Channel { get; set; } = "stable";

    [JsonPropertyName("publishedAt")]
    public DateTimeOffset? PublishedAt { get; set; }

    [JsonPropertyName("releaseNotes")]
    public string ReleaseNotes { get; set; } = "";

    [JsonPropertyName("images")]
    public List<FirmwareImage> Images { get; set; } = [];

    [JsonPropertyName("tools")]
    public Dictionary<string, FlashTool> Tools { get; set; } = new(StringComparer.OrdinalIgnoreCase);
}

public sealed class FirmwareImage
{
    [JsonPropertyName("name")]
    public string Name { get; set; } = "";

    [JsonPropertyName("url")]
    public string Url { get; set; } = "";

    [JsonPropertyName("sha256")]
    public string Sha256 { get; set; } = "";

    [JsonPropertyName("offset")]
    public string Offset { get; set; } = "";

    [JsonPropertyName("kind")]
    public string Kind { get; set; } = "firmware";
}

public sealed class FlashTool
{
    [JsonPropertyName("url")]
    public string Url { get; set; } = "";

    [JsonPropertyName("sha256")]
    public string Sha256 { get; set; } = "";

    [JsonPropertyName("archiveEntry")]
    public string? ArchiveEntry { get; set; }

    [JsonPropertyName("argumentsPrefix")]
    public string? ArgumentsPrefix { get; set; }
}

public sealed record ResolvedFirmware(FirmwareManifest Manifest, Uri SourceUri, string? LocalManifestPath);
public sealed record DownloadedImage(FirmwareImage Definition, string LocalPath);
public sealed record PreparedFlash(string ToolPath, string? ArgumentsPrefix, IReadOnlyList<DownloadedImage> Images);
