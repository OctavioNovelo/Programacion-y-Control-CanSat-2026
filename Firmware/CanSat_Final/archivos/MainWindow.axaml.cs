using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Ports;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.Formats.Jpeg;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;

using AvaImage = Avalonia.Controls.Image;

namespace model_eggs_ui;

public partial class MainWindow : Window
{
    // Protocol Constants
    private const byte PacketStartOfFrame = 0xAA;
    private const byte PktVVel = 0xF1;
    private const byte PktAccel = 0xF2;
    private const byte PktTemp = 0xF3;
    private const byte PktPress = 0xF4;
    private const byte PktImgSize = 0xB0;
    private const byte PktImgChunk = 0xB1;

    // Camera IDs
    private const byte CameraLeftId = 0x00;
    private const byte CameraRightId = 0x01;

    // Serial Settings
    private const string DefaultPortName = "/dev/ttyUSB0";
    private const int DefaultBaudRate = 115200;
    private const int SerialReconnectDelayMilliseconds = 2000;

    // State Machine
    private enum SerialState
    {
        WaitStart,
        ReadType,
        ReadLengthMsb,
        ReadLengthLsb,
        ReadPayload,
        ReadChecksum
    }

    private SerialState _currentState = SerialState.WaitStart;
    private byte _packetType;
    private byte _lenMsb;
    private byte _lenLsb;
    private ushort _payloadLength;
    private byte[] _payloadBuffer = Array.Empty<byte>();
    private int _payloadIndex = 0;

    // Image Buffering & Logic
    private readonly string _leftImagePath = Path.Combine(AppContext.BaseDirectory, "left.jpg");
    private readonly string _rightImagePath = Path.Combine(AppContext.BaseDirectory, "right.jpg");
    private readonly string _anaglyphImagePath = Path.Combine(AppContext.BaseDirectory, "anaglyph.jpg");

    private uint _leftImageSize = 0;
    private uint _rightImageSize = 0;
    private uint _leftReceived = 0;
    private uint _rightReceived = 0;
    private byte[] _leftBuffer = Array.Empty<byte>();
    private byte[] _rightBuffer = Array.Empty<byte>();
    private bool _leftSaved = false;
    private bool _rightSaved = false;
    private bool _anaglyphSaved = false;

    private readonly List<ChunkIssue> _leftIssues = new();
    private readonly List<ChunkIssue> _rightIssues = new();
    private record ChunkIssue(uint Offset, byte Size);

    // Logging & Control
    private SerialPort? _serialPort;
    private readonly CancellationTokenSource _cancellationTokenSource = new();
    private int _rawLogLineCount = 0;
    private const int MaxRawLogLines = 300;

    public MainWindow()
    {
        InitializeComponent();
        InitializeUiDefaults();
        SetLeftPanelMode(showRaw: false);
        LoadExistingImages();

        StartSerialConnectionManager(DefaultPortName, DefaultBaudRate, _cancellationTokenSource.Token);
    }

    private void InitializeUiDefaults()
    {
        VerticalVelocityLabel.Content = "0.00 m/s";
        AccelerationLabel.Content = "0.00 m/s²";
        TemperatureLabel.Content = "0.00 °C";
        PressureLabel.Content = "0.00 kPa";

        RawLogTextBlock.Text = string.Empty;
        AppendRawLog("SYSTEM", $"Waiting for serial port {DefaultPortName}...");
    }

    private void StartSerialConnectionManager(string portName, int baudRate, CancellationToken cancellationToken)
    {
        Task.Run(async () =>
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                try
                {
                    if (!IsPortCurrentlyAvailable(portName))
                    {
                        await Task.Delay(SerialReconnectDelayMilliseconds, cancellationToken);
                        continue;
                    }

                    using SerialPort serialPort = new SerialPort(portName, baudRate, Parity.None, 8, StopBits.One);
                    serialPort.ReadTimeout = 500;
                    serialPort.DataReceived += SerialPort_DataReceived;
                    serialPort.Open();

                    _serialPort = serialPort;
                    AppendRawLog("SYSTEM", $"Connected to {portName} @ {baudRate}");
                    ResetImageReceptionState();

                    while (!cancellationToken.IsCancellationRequested && serialPort.IsOpen)
                    {
                        await Task.Delay(500, cancellationToken);
                    }
                }
                catch (OperationCanceledException) { break; }
                catch (Exception ex)
                {
                    AppendRawLog("SYSTEM", $"Serial error: {ex.Message}");
                }
                finally
                {
                    CleanupSerialPortReference();
                }

                await Task.Delay(SerialReconnectDelayMilliseconds, cancellationToken);
            }
        }, cancellationToken);
    }

    private void SerialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
    {
        if (_serialPort == null || !_serialPort.IsOpen) return;

        try
        {
            int bytesToRead = _serialPort.BytesToRead;
            if (bytesToRead <= 0) return;

            byte[] buffer = new byte[bytesToRead];
            int read = _serialPort.Read(buffer, 0, bytesToRead);

            for (int i = 0; i < read; i++)
            {
                ProcessIncomingByte(buffer[i]);
            }
        }
        catch (Exception ex)
        {
            AppendRawLog("SERIAL_READ_ERR", ex.Message);
        }
    }

    private void ProcessIncomingByte(byte b)
    {
        switch (_currentState)
        {
            case SerialState.WaitStart:
                if (b == PacketStartOfFrame)
                {
                    _currentState = SerialState.ReadType;
                }
                break;

            case SerialState.ReadType:
                _packetType = b;
                _currentState = SerialState.ReadLengthMsb;
                break;

            case SerialState.ReadLengthMsb:
                _lenMsb = b;
                _currentState = SerialState.ReadLengthLsb;
                break;

            case SerialState.ReadLengthLsb:
                _lenLsb = b;
                _payloadLength = (ushort)((_lenMsb << 8) | _lenLsb);
                _payloadIndex = 0;
                
                if (_payloadLength > 0)
                {
                    _payloadBuffer = new byte[_payloadLength];
                    _currentState = SerialState.ReadPayload;
                }
                else
                {
                    _payloadBuffer = Array.Empty<byte>();
                    _currentState = SerialState.ReadChecksum;
                }
                break;

            case SerialState.ReadPayload:
                if (_payloadIndex < _payloadBuffer.Length)
                {
                    _payloadBuffer[_payloadIndex++] = b;
                }
                
                if (_payloadIndex >= _payloadLength)
                {
                    _currentState = SerialState.ReadChecksum;
                }
                break;

            case SerialState.ReadChecksum:
                byte computedChecksum = ComputeChecksum(_packetType, _lenMsb, _lenLsb, _payloadBuffer);
                if (b == computedChecksum)
                {
                    HandleValidPacket(_packetType, _payloadBuffer);
                }
                else
                {
                    AppendRawLog("SERIAL", $"Checksum mismatch for type 0x{_packetType:X2}. Expected 0x{computedChecksum:X2}, got 0x{b:X2}");
                }
                _currentState = SerialState.WaitStart;
                break;
        }
    }

    private void HandleValidPacket(byte type, byte[] payload)
    {
        switch (type)
        {
            case PktVVel:
                ProcessTelemetry(payload, VerticalVelocityLabel, "{0:F2} m/s", "Vertical Velocity");
                break;
            case PktAccel:
                ProcessTelemetry(payload, AccelerationLabel, "{0:F2} m/s²", "Acceleration");
                break;
            case PktTemp:
                ProcessTelemetry(payload, TemperatureLabel, "{0:F2} °C", "Temperature");
                break;
            case PktPress:
                ProcessTelemetry(payload, PressureLabel, "{0:F2} kPa", "Pressure");
                break;
            case PktImgSize:
                ProcessImageSize(payload);
                break;
            case PktImgChunk:
                ProcessImageChunk(payload);
                break;
            default:
                AppendRawLog("UNKNOWN", $"Type 0x{type:X2} Length {payload.Length}");
                break;
        }
    }

    private async void ProcessTelemetry(byte[] payload, Label targetLabel, string format, string logName)
    {
        if (payload.Length != 2) return;

        short raw = (short)((payload[0] << 8) | payload[1]);
        float value = raw / 100.0f;

        await Dispatcher.UIThread.InvokeAsync(() =>
        {
            targetLabel.Content = string.Format(format, value);
        });

        AppendRawLog(logName, string.Format(format, value));
    }

    private void ProcessImageSize(byte[] payload)
    {
        if (payload.Length != 5) return;

        byte cameraId = payload[0];
        uint imageSize = ((uint)payload[1] << 24) | ((uint)payload[2] << 16) | ((uint)payload[3] << 8) | payload[4];

        _anaglyphSaved = false;

        if (cameraId == CameraLeftId)
        {
            _leftImageSize = imageSize;
            _leftReceived = 0;
            _leftBuffer = new byte[_leftImageSize];
            _leftIssues.Clear();
            _leftSaved = false;
            AppendRawLog("LEFT_IMG", $"Size: {imageSize} bytes");
        }
        else if (cameraId == CameraRightId)
        {
            _rightImageSize = imageSize;
            _rightReceived = 0;
            _rightBuffer = new byte[_rightImageSize];
            _rightIssues.Clear();
            _rightSaved = false;
            AppendRawLog("RIGHT_IMG", $"Size: {imageSize} bytes");
        }
    }

    private void ProcessImageChunk(byte[] payload)
    {
        if (payload.Length < 6) return;

        byte cameraId = payload[0];
        uint offset = ((uint)payload[1] << 24) | ((uint)payload[2] << 16) | ((uint)payload[3] << 8) | payload[4];
        byte chunkSize = payload[5];

        if (payload.Length != 6 + chunkSize) return;

        byte[] data = new byte[chunkSize];
        Array.Copy(payload, 6, data, 0, chunkSize);

        if (cameraId == CameraLeftId && _leftBuffer.Length > 0)
        {
            if (offset + chunkSize <= _leftBuffer.Length)
            {
                Array.Copy(data, 0, _leftBuffer, offset, chunkSize);
                _leftReceived += chunkSize;
                TryFinalizeImages();
            }
        }
        else if (cameraId == CameraRightId && _rightBuffer.Length > 0)
        {
            if (offset + chunkSize <= _rightBuffer.Length)
            {
                Array.Copy(data, 0, _rightBuffer, offset, chunkSize);
                _rightReceived += chunkSize;
                TryFinalizeImages();
            }
        }
    }

    private void TryFinalizeImages()
    {
        if (_leftImageSize > 0 && _leftReceived >= _leftImageSize && !_leftSaved)
        {
            File.WriteAllBytes(_leftImagePath, _leftBuffer);
            _leftSaved = true;
            AppendRawLog("LEFT_IMG", "Saved successfully");
            LoadImageIntoControl(_leftImagePath, LeftImageControl);
        }

        if (_rightImageSize > 0 && _rightReceived >= _rightImageSize && !_rightSaved)
        {
            File.WriteAllBytes(_rightImagePath, _rightBuffer);
            _rightSaved = true;
            AppendRawLog("RIGHT_IMG", "Saved successfully");
            LoadImageIntoControl(_rightImagePath, RightImageControl);
        }

        if (_leftSaved && _rightSaved && !_anaglyphSaved)
        {
            CreateAnaglyphProcess();
            _anaglyphSaved = true;
        }
    }

    private async void CreateAnaglyphProcess()
    {
        try
        {
            await Task.Run(() =>
            {
                using var left = SixLabors.ImageSharp.Image.Load<L8>(_leftImagePath);
                using var right = SixLabors.ImageSharp.Image.Load<L8>(_rightImagePath);

                int w = Math.Min(left.Width, right.Width);
                int h = Math.Min(left.Height, right.Height);

                using var output = new SixLabors.ImageSharp.Image<Rgba32>(w, h);
                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        output[x, y] = new Rgba32(left[x, y].PackedValue, right[x, y].PackedValue, right[x, y].PackedValue);
                    }
                }
                output.SaveAsJpeg(_anaglyphImagePath, new JpegEncoder());
            });

            AppendRawLog("ANAGLYPH", "Created successfully");
            LoadImageIntoControl(_anaglyphImagePath, OutputImageControl);
        }
        catch (Exception ex)
        {
            AppendRawLog("ANAGLYPH_ERR", ex.Message);
        }
    }

    private async void LoadImageIntoControl(string path, AvaImage control)
    {
        try
        {
            byte[] data = await File.ReadAllBytesAsync(path);
            using var ms = new MemoryStream(data);
            var bitmap = new Bitmap(ms);

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                control.Source = bitmap;
            });
        }
        catch { }
    }

    private static byte ComputeChecksum(byte type, byte lenH, byte lenL, byte[] payload)
    {
        byte checksum = 0;
        checksum ^= type;
        checksum ^= lenH;
        checksum ^= lenL;
        foreach (byte b in payload) checksum ^= b;
        return checksum;
    }

    private void ResetImageReceptionState()
    {
        _leftImageSize = 0; _rightImageSize = 0;
        _leftReceived = 0; _rightReceived = 0;
        _leftSaved = false; _rightSaved = false; _anaglyphSaved = false;
    }

    private void CleanupSerialPortReference()
    {
        if (_serialPort != null)
        {
            _serialPort.DataReceived -= SerialPort_DataReceived;
            if (_serialPort.IsOpen) _serialPort.Close();
            _serialPort.Dispose();
            _serialPort = null;
        }
    }

    private static bool IsPortCurrentlyAvailable(string name)
    {
        foreach (var p in SerialPort.GetPortNames()) if (p.Equals(name, StringComparison.OrdinalIgnoreCase)) return true;
        return false;
    }

    private void DataTabButton_Click(object? s, RoutedEventArgs e) => SetLeftPanelMode(false);
    private void RawTabButton_Click(object? s, RoutedEventArgs e) => SetLeftPanelMode(true);

    private void SetLeftPanelMode(bool showRaw)
    {
        DataView.IsVisible = !showRaw;
        RawView.IsVisible = showRaw;
        DataTabButton.Background = GetBrush(showRaw ? "WidgetBackground2" : "WidgetBackground1");
        RawTabButton.Background = GetBrush(showRaw ? "WidgetBackground1" : "WidgetBackground2");
    }

    private IBrush GetBrush(string key) => (Application.Current?.Resources.TryGetResource(key, this.ActualThemeVariant, out var res) == true && res is IBrush b) ? b : Brushes.Transparent;

    private void AppendRawLog(string tag, string val)
    {
        string msg = $"{DateTime.Now:HH:mm:ss} - {tag} - {val}\n";
        Dispatcher.UIThread.Post(() =>
        {
            RawLogTextBlock.Text += msg;
            _rawLogLineCount++;
            if (_rawLogLineCount > MaxRawLogLines)
            {
                int firstNewline = RawLogTextBlock.Text.IndexOf('\n');
                if (firstNewline != -1) RawLogTextBlock.Text = RawLogTextBlock.Text.Substring(firstNewline + 1);
            }
            RawScrollViewer.Offset = new Vector(0, RawScrollViewer.Extent.Height);
        });
    }

    private void LoadExistingImages()
    {
        if (File.Exists(_leftImagePath)) LoadImageIntoControl(_leftImagePath, LeftImageControl);
        if (File.Exists(_rightImagePath)) LoadImageIntoControl(_rightImagePath, RightImageControl);
        if (File.Exists(_anaglyphImagePath)) LoadImageIntoControl(_anaglyphImagePath, OutputImageControl);
    }

    protected override void OnClosed(EventArgs e)
    {
        _cancellationTokenSource.Cancel();
        CleanupSerialPortReference();
        base.OnClosed(e);
    }
}