using DCCExpressLite.Installer.Models;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;

namespace DCCExpressLite.Installer.Services;

public sealed partial class FlashService
{
    public async Task FlashAsync(
        PreparedFlash prepared,
        string port,
        Action<string> log,
        IProgress<double> progress,
        CancellationToken cancellationToken)
    {
        var arguments = new StringBuilder();
        if (!string.IsNullOrWhiteSpace(prepared.ArgumentsPrefix)) arguments.Append(prepared.ArgumentsPrefix).Append(' ');
        arguments.Append("--chip esp32 --port ").Append(Quote(port));
        arguments.Append(" --baud 460800 --before default-reset --after hard-reset write-flash -z --flash-mode dio --flash-freq 40m --flash-size detect");
        foreach (var image in prepared.Images)
            arguments.Append(' ').Append(image.Definition.Offset).Append(' ').Append(Quote(image.LocalPath));

        var info = new ProcessStartInfo
        {
            FileName = prepared.ToolPath,
            Arguments = arguments.ToString(),
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
            WorkingDirectory = Path.GetDirectoryName(prepared.ToolPath)!,
        };

        log($"> {Path.GetFileName(info.FileName)} {info.Arguments}");
        using var process = new Process { StartInfo = info, EnableRaisingEvents = true };
        process.OutputDataReceived += (_, eventArgs) => HandleLine(eventArgs.Data, log, progress);
        process.ErrorDataReceived += (_, eventArgs) => HandleLine(eventArgs.Data, log, progress);
        if (!process.Start()) throw new InvalidOperationException("Could not start esptool.");
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        using var registration = cancellationToken.Register(() =>
        {
            try { if (!process.HasExited) process.Kill(true); } catch { }
        });
        await process.WaitForExitAsync(cancellationToken);
        if (process.ExitCode != 0) throw new InvalidOperationException($"esptool exited with code {process.ExitCode}.");
        progress.Report(100);
    }

    private static void HandleLine(string? line, Action<string> log, IProgress<double> progress)
    {
        if (string.IsNullOrWhiteSpace(line)) return;
        log(line);
        var match = PercentRegex().Match(line);
        if (match.Success && double.TryParse(match.Groups[1].Value, out var value)) progress.Report(45 + value * .54);
    }

    private static string Quote(string value) => $"\"{value.Replace("\"", "\\\"")}\"";

    [GeneratedRegex(@"\((\d{1,3})\s*%\)")]
    private static partial Regex PercentRegex();
}
