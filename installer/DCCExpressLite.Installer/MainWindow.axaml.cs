using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using DCCExpressLite.Installer.Models;
using DCCExpressLite.Installer.Services;
using System.Diagnostics;

namespace DCCExpressLite.Installer;

public sealed partial class MainWindow : Window
{
    private readonly FirmwarePackageService _packages = new();
    private readonly FlashService _flasher = new();
    private readonly DeviceBackupService _backups = new();
    private ResolvedFirmware? _release;
    private CancellationTokenSource? _operationCancellation;

    public MainWindow()
    {
        InitializeComponent();
        PlatformText.Text = $"{PlatformName()} · {SerialPortDiscovery.RuntimeId()}";
        RefreshPorts();
        var initialManifest = Environment.GetEnvironmentVariable("DCCEXPRESS_MANIFEST");
        if (!string.IsNullOrWhiteSpace(initialManifest))
        {
            ManifestSourceTextBox.Text = initialManifest;
            Opened += async (_, _) => await LoadReleaseAsync();
        }
    }

    private void RefreshPorts_Click(object? sender, RoutedEventArgs e) => RefreshPorts();

    private void RefreshPorts()
    {
        var previous = PortComboBox.SelectedItem as string;
        var ports = SerialPortDiscovery.GetPorts();
        PortComboBox.ItemsSource = ports;
        PortComboBox.SelectedItem = previous is not null && ports.Contains(previous) ? previous : ports.LastOrDefault();
        AppendLog(ports.Count == 0 ? "No serial ports found. Connect the EX-CSB1 and rescan." : $"Found {ports.Count} serial port(s): {string.Join(", ", ports)}");
        UpdateReadyState();
    }

    private async void BrowseManifest_Click(object? sender, RoutedEventArgs e)
    {
        var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Open DCCExpressLite firmware manifest",
            AllowMultiple = false,
            FileTypeFilter = [new FilePickerFileType("Firmware manifest") { Patterns = ["*.json"] }],
        });
        var path = files.FirstOrDefault()?.TryGetLocalPath();
        if (path is null) return;
        ManifestSourceTextBox.Text = path;
        await LoadReleaseAsync();
    }

    private async void LoadRelease_Click(object? sender, RoutedEventArgs e) => await LoadReleaseAsync();

    private async Task LoadReleaseAsync()
    {
        var source = ManifestSourceTextBox.Text?.Trim();
        if (string.IsNullOrWhiteSpace(source))
        {
            SetError("Enter a GitHub manifest URL or choose a local manifest.json file.");
            return;
        }

        SetBusy(true, "Loading release…", "Reading firmware manifest and release metadata.");
        try
        {
            _release = await _packages.LoadManifestAsync(source, CancellationToken.None);
            ReleaseTitleText.Text = $"{_release.Manifest.Product} · {_release.Manifest.Channel}";
            ReleaseVersionText.Text = _release.Manifest.Version;
            ReleaseNotesText.Text = string.IsNullOrWhiteSpace(_release.Manifest.ReleaseNotes)
                ? $"{_release.Manifest.Images.Count} flash image(s) are available."
                : _release.Manifest.ReleaseNotes;
            ReleaseSummaryBorder.IsVisible = true;
            AppendLog($"Loaded {_release.Manifest.Product} {_release.Manifest.Version} ({_release.Manifest.Images.Count} images). ");
            SetSuccess("Release ready", $"Version {_release.Manifest.Version} is ready to install.");
        }
        catch (Exception exception)
        {
            _release = null;
            ReleaseSummaryBorder.IsVisible = false;
            SetError(exception.Message);
        }
        finally
        {
            SetBusy(false);
            UpdateReadyState();
        }
    }

    private async void Install_Click(object? sender, RoutedEventArgs e)
    {
        if (_release is null || PortComboBox.SelectedItem is not string port) return;
        var includeFirmware = FirmwareCheckBox.IsChecked == true;
        var includeFilesystem = WebUiCheckBox.IsChecked == true;
        var selected = _release.Manifest.Images.Where(image =>
            image.Kind.Equals("filesystem", StringComparison.OrdinalIgnoreCase) ? includeFilesystem : includeFirmware).ToArray();
        if (selected.Length == 0)
        {
            SetError("Select firmware/system or Web UI/LittleFS before installing.");
            return;
        }

        _operationCancellation = new CancellationTokenSource();
        var token = _operationCancellation.Token;
        DeviceBackup? backup = null;
        SetBusy(true, "Preparing installation…", "Checking release files and protecting device data.");
        InstallProgressBar.Value = 1;
        AppendLog($"Starting installation on {port}.");

        try
        {
            if (includeFilesystem && BackupCheckBox.IsChecked == true)
            {
                AppendLog($"Backing up layout and locomotives from {DeviceAddressTextBox.Text}...");
                backup = await _backups.BackupAsync(DeviceAddressTextBox.Text ?? "", token);
                AppendLog("Device data backup completed.");
            }

            var prepareProgress = new Progress<(double Value, string Message)>(value =>
            {
                InstallProgressBar.Value = value.Value;
                OperationDetailText.Text = value.Message;
                AppendLog(value.Message);
            });
            var prepared = await _packages.PrepareAsync(_release, selected, prepareProgress, token);
            OperationTitleText.Text = "Flashing EX-CSB1…";
            OperationDetailText.Text = "Do not disconnect USB power.";
            var flashProgress = new Progress<double>(value => InstallProgressBar.Value = value);
            await _flasher.FlashAsync(prepared, port, AppendLog, flashProgress, token);

            if (backup is not null)
            {
                OperationTitleText.Text = "Restoring device data…";
                OperationDetailText.Text = "Waiting for Wi-Fi and the web server to return.";
                await _backups.RestoreAsync(DeviceAddressTextBox.Text ?? "", backup, AppendLog, token);
                AppendLog("Layout and locomotives restored.");
            }

            InstallProgressBar.Value = 100;
            SetSuccess("Installation complete", $"DCCExpressLite {_release.Manifest.Version} was installed successfully.");
            AppendLog("SUCCESS: EX-CSB1 is ready.");
        }
        catch (OperationCanceledException)
        {
            SetError("Installation cancelled. Use Recovery if the device no longer starts.");
            AppendLog("CANCELLED by user.");
        }
        catch (Exception exception)
        {
            SetError(exception.Message);
            AppendLog($"ERROR: {exception}");
        }
        finally
        {
            _operationCancellation.Dispose();
            _operationCancellation = null;
            SetBusy(false);
            UpdateReadyState();
        }
    }

    private void Cancel_Click(object? sender, RoutedEventArgs e) => _operationCancellation?.Cancel();
    private void PortComboBox_SelectionChanged(object? sender, SelectionChangedEventArgs e) => UpdateReadyState();

    private void SetBusy(bool busy, string? title = null, string? detail = null)
    {
        LoadReleaseButton.IsEnabled = !busy;
        RefreshPortsButton.IsEnabled = !busy;
        CancelButton.IsVisible = busy && _operationCancellation is not null;
        if (title is not null) OperationTitleText.Text = title;
        if (detail is not null) OperationDetailText.Text = detail;
        if (busy) TopStatusText.Text = "WORKING";
    }

    private void UpdateReadyState()
    {
        var ready = _release is not null && PortComboBox.SelectedItem is string && _operationCancellation is null;
        InstallButton.IsEnabled = ready;
        if (_operationCancellation is null) TopStatusText.Text = ready ? "READY TO FLASH" : "READY";
    }

    private void SetSuccess(string title, string detail)
    {
        OperationTitleText.Text = title;
        OperationDetailText.Text = detail;
        TopStatusText.Text = "SUCCESS";
    }

    private void SetError(string message)
    {
        OperationTitleText.Text = "Action required";
        OperationDetailText.Text = message;
        TopStatusText.Text = "CHECK SETTINGS";
        AppendLog($"ERROR: {message}");
    }

    private void AppendLog(string message)
    {
        Dispatcher.UIThread.Post(() =>
        {
            LogTextBox.Text += $"[{DateTime.Now:HH:mm:ss}] {message}{Environment.NewLine}";
            LogTextBox.CaretIndex = LogTextBox.Text?.Length ?? 0;
        });
    }

    private static string PlatformName() => OperatingSystem.IsWindows() ? "Windows" : OperatingSystem.IsMacOS() ? "macOS" : "Linux";
}
