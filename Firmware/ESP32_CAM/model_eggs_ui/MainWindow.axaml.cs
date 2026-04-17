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
    private const byte PacketStartOfFrame = 0xAA;

    private const byte PktImgSize = 0xB0;
    private const byte PktImgChunk = 0xB1;

    private const byte PktVVel = 0xF1;
    private const byte PktAccel = 0xF2;
    private const byte PktTemp = 0xF3;
    private const byte PktPress = 0xF4;

    private const byte CameraLeftId = 0x00;
    private const byte CameraRightId = 0x01;

    private const string DefaultPortName = "/dev/ttyACM0";
    private const int DefaultBaudRate = 115200;
    private const int SerialReconnectDelayMilliseconds = 2000;

    private readonly string _leftImagePath = Path.Combine(AppContext.BaseDirectory, "left.jpg");
    private readonly string _rightImagePath = Path.Combine(AppContext.BaseDirectory, "right.jpg");
    private readonly string _anaglyphImagePath = Path.Combine(AppContext.BaseDirectory, "anaglyph.jpg");

    private readonly CancellationTokenSource _cancellationTokenSource = new();

    private SerialPort? _serialPort;

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

    private int _rawLogLineCount = 0;
    private const int MaxRawLogLines = 300;

    private record ChunkIssue(uint Offset, byte Size);

    public MainWindow()
    {
        InitializeComponent();

        InitializeUiDefaults();
        SetLeftPanelMode(showRaw: false);
        LoadExistingImages();

        StartSerialListenerLoop(DefaultPortName, DefaultBaudRate, _cancellationTokenSource.Token);
    }

    private void InitializeUiDefaults()
    {
        VerticalVelocityValueText.Text = "0.00 m/s";
        AccelerationValueText.Text = "0.00 m/s²";
        TemperatureValueText.Text = "0.00 °C";
        PressureValueText.Text = "0.00 kPa";

        RawLogTextBlock.Text = string.Empty;
        AppendRawLog("SYSTEM", $"Waiting for serial port {DefaultPortName}...");
    }

    private void DataTabButton_Click(object? sender, RoutedEventArgs e)
    {
        SetLeftPanelMode(showRaw: false);
    }

    private void RawTabButton_Click(object? sender, RoutedEventArgs e)
    {
        SetLeftPanelMode(showRaw: true);
    }

    private void SetLeftPanelMode(bool showRaw)
    {
        DataView.IsVisible = !showRaw;
        DataView.IsEnabled = !showRaw;

        RawView.IsVisible = showRaw;
        RawView.IsEnabled = showRaw;

        if (showRaw)
        {
            RawTabButton.Background = GetBrush("WidgetBackground1");
            DataTabButton.Background = GetBrush("WidgetBackground2");
        }
        else
        {
            DataTabButton.Background = GetBrush("WidgetBackground1");
            RawTabButton.Background = GetBrush("WidgetBackground2");
        }
    }

    private IBrush GetBrush(string resourceKey)
    {
        if (Application.Current?.Resources.TryGetResource(resourceKey, ActualThemeVariant, out var resource) == true
            && resource is IBrush brush)
        {
            return brush;
        }

        return Brushes.Transparent;
    }

    private void StartSerialListenerLoop(string portName, int baudRate, CancellationToken cancellationToken)
    {
        Task.Run(async () =>
        {
            bool waitingMessageShown = false;

            while (!cancellationToken.IsCancellationRequested)
            {
                try
                {
                    if (!IsPortCurrentlyAvailable(portName))
                    {
                        if (!waitingMessageShown)
                        {
                            AppendRawLog("SYSTEM", $"Serial {portName} not detected. Retrying...");
                            waitingMessageShown = true;
                        }

                        await Task.Delay(SerialReconnectDelayMilliseconds, cancellationToken);
                        continue;
                    }

                    waitingMessageShown = false;

                    using SerialPort serialPort = new SerialPort(
                        portName,
                        baudRate,
                        Parity.None,
                        8,
                        StopBits.One
                    );

                    serialPort.ReadTimeout = 10000;
                    serialPort.WriteTimeout = 5000;
                    serialPort.Open();

                    _serialPort = serialPort;

                    AppendRawLog("SYSTEM", $"Connected to {portName} @ {baudRate}");
                    ResetImageReceptionState();

                    while (!cancellationToken.IsCancellationRequested && serialPort.IsOpen)
                    {
                        byte sof = ReadExact(serialPort, 1)[0];

                        if (sof != PacketStartOfFrame)
                        {
                            continue;
                        }

                        byte type = ReadExact(serialPort, 1)[0];
                        byte[] lenBytes = ReadExact(serialPort, 2);
                        ushort payloadLength = ReadUInt16BigEndian(lenBytes, 0);
                        byte[] payload = ReadExact(serialPort, payloadLength);
                        byte receivedChecksum = ReadExact(serialPort, 1)[0];

                        byte computedChecksum = ComputeChecksum(type, lenBytes[0], lenBytes[1], payload);
                        bool checksumOk = receivedChecksum == computedChecksum;

                        switch (type)
                        {
                            case PktVVel:
                                ProcessVerticalVelocity(payload, payloadLength, checksumOk);
                                break;

                            case PktAccel:
                                ProcessAcceleration(payload, payloadLength, checksumOk);
                                break;

                            case PktTemp:
                                ProcessTemperature(payload, payloadLength, checksumOk);
                                break;

                            case PktPress:
                                ProcessPressure(payload, payloadLength, checksumOk);
                                break;

                            case PktImgSize:
                                ProcessImageSize(payload, payloadLength, checksumOk);
                                break;

                            case PktImgChunk:
                                ProcessImageChunk(payload, payloadLength, checksumOk);
                                break;

                            default:
                                AppendRawLog("UNKNOWN", $"Unknown packet type: 0x{type:X2}");
                                break;
                        }

                        TryFinalizeImages();
                    }
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (TimeoutException)
                {
                    AppendRawLog("SYSTEM", "Serial timeout. Reconnecting...");
                }
                catch (IOException)
                {
                    AppendRawLog("SYSTEM", "Serial disconnected. Waiting for reconnection...");
                }
                catch (UnauthorizedAccessException)
                {
                    AppendRawLog("SYSTEM", $"Serial {DefaultPortName} is busy. Retrying...");
                }
                catch (Exception ex)
                {
                    AppendRawLog("SYSTEM", $"Serial issue: {ex.Message}");
                }
                finally
                {
                    CleanupSerialPortReference();
                }

                await Task.Delay(SerialReconnectDelayMilliseconds, cancellationToken);
            }
        }, cancellationToken);
    }

    private static bool IsPortCurrentlyAvailable(string portName)
    {
        string[] availablePorts = SerialPort.GetPortNames();

        for (int i = 0; i < availablePorts.Length; i++)
        {
            if (string.Equals(availablePorts[i], portName, StringComparison.OrdinalIgnoreCase))
            {
                return true;
            }
        }

        return false;
    }

    private void ResetImageReceptionState()
    {
        _leftImageSize = 0;
        _rightImageSize = 0;

        _leftReceived = 0;
        _rightReceived = 0;

        _leftBuffer = Array.Empty<byte>();
        _rightBuffer = Array.Empty<byte>();

        _leftSaved = false;
        _rightSaved = false;
        _anaglyphSaved = false;

        _leftIssues.Clear();
        _rightIssues.Clear();
    }

    private void CleanupSerialPortReference()
    {
        try
        {
            if (_serialPort is not null)
            {
                if (_serialPort.IsOpen)
                {
                    _serialPort.Close();
                }

                _serialPort.Dispose();
            }
        }
        catch
        {
        }
        finally
        {
            _serialPort = null;
        }
    }

    private void ProcessVerticalVelocity(byte[] payload, ushort payloadLength, bool checksumOk)
    {
        if (checksumOk && payloadLength == 2)
        {
            short raw = ReadInt16BigEndian(payload, 0);
            double value = raw / 100.0;

            Dispatcher.UIThread.Post(() =>
            {
                VerticalVelocityValueText.Text = $"{value:F2} m/s";
            });

            AppendRawLog("VerticalVelocity", $"{value:F2} m/s");
        }
        else
        {
            AppendRawLog("VerticalVelocity", "checksum mismatch");
        }
    }

    private void ProcessAcceleration(byte[] payload, ushort payloadLength, bool checksumOk)
    {
        if (checksumOk && payloadLength == 2)
        {
            short raw = ReadInt16BigEndian(payload, 0);
            double value = raw / 100.0;

            Dispatcher.UIThread.Post(() =>
            {
                AccelerationValueText.Text = $"{value:F2} m/s²";
            });

            AppendRawLog("Acceleration", $"{value:F2} m/s²");
        }
        else
        {
            AppendRawLog("Acceleration", "checksum mismatch");
        }
    }

    private void ProcessTemperature(byte[] payload, ushort payloadLength, bool checksumOk)
    {
        if (checksumOk && payloadLength == 2)
        {
            short raw = ReadInt16BigEndian(payload, 0);
            double value = raw / 100.0;

            Dispatcher.UIThread.Post(() =>
            {
                TemperatureValueText.Text = $"{value:F2} °C";
            });

            AppendRawLog("Temperature", $"{value:F2} °C");
        }
        else
        {
            AppendRawLog("Temperature", "checksum mismatch");
        }
    }

    private void ProcessPressure(byte[] payload, ushort payloadLength, bool checksumOk)
    {
        if (checksumOk && payloadLength == 2)
        {
            short raw = ReadInt16BigEndian(payload, 0);
            double value = raw / 100.0;

            Dispatcher.UIThread.Post(() =>
            {
                PressureValueText.Text = $"{value:F2} kPa";
            });

            AppendRawLog("Pressure", $"{value:F2} kPa");
        }
        else
        {
            AppendRawLog("Pressure", "checksum mismatch");
        }
    }

    private void ProcessImageSize(byte[] payload, ushort payloadLength, bool checksumOk)
    {
        if (!checksumOk)
        {
            AppendRawLog("IMAGE", "Image size checksum mismatch");
            return;
        }

        if (payloadLength != 5)
        {
            AppendRawLog("IMAGE", "Invalid image size payload length");
            return;
        }

        byte cameraId = payload[0];
        uint imageSize = ReadUInt32BigEndian(payload, 1);

        _anaglyphSaved = false;

        if (cameraId == CameraLeftId)
        {
            _leftImageSize = imageSize;
            _leftReceived = 0;
            _leftBuffer = new byte[_leftImageSize];
            _leftIssues.Clear();
            _leftSaved = false;

            AppendRawLog("LEFT_IMAGE_SIZE", $"{_leftImageSize} bytes");
        }
        else if (cameraId == CameraRightId)
        {
            _rightImageSize = imageSize;
            _rightReceived = 0;
            _rightBuffer = new byte[_rightImageSize];
            _rightIssues.Clear();
            _rightSaved = false;

            AppendRawLog("RIGHT_IMAGE_SIZE", $"{_rightImageSize} bytes");
        }
        else
        {
            AppendRawLog("IMAGE", $"Unknown camera id: {cameraId}");
        }
    }

    private void ProcessImageChunk(byte[] payload, ushort payloadLength, bool checksumOk)
    {
        if (payloadLength < 6)
        {
            AppendRawLog("IMAGE", "Invalid image chunk payload length");
            return;
        }

        byte cameraId = payload[0];
        uint offset = ReadUInt32BigEndian(payload, 1);
        byte chunkSize = payload[5];

        if (payloadLength != (ushort)(6 + chunkSize))
        {
            AppendRawLog("IMAGE", "Chunk size mismatch against LEN");
            return;
        }

        byte[] data = new byte[chunkSize];
        Array.Copy(payload, 6, data, 0, chunkSize);

        if (cameraId == CameraLeftId)
        {
            ProcessLeftChunk(offset, chunkSize, data, checksumOk);
        }
        else if (cameraId == CameraRightId)
        {
            ProcessRightChunk(offset, chunkSize, data, checksumOk);
        }
        else
        {
            AppendRawLog("IMAGE", $"Unknown camera id in chunk: {cameraId}");
        }
    }

    private void ProcessLeftChunk(uint offset, byte chunkSize, byte[] data, bool checksumOk)
    {
        if (_leftBuffer.Length == 0)
        {
            AppendRawLog("LEFT_IMAGE", "Chunk received before image size");
            return;
        }

        if ((offset + chunkSize) > _leftBuffer.Length)
        {
            AppendRawLog("LEFT_IMAGE", $"Chunk out of bounds | offset={offset} size={chunkSize}");
            return;
        }

        Array.Copy(data, 0, _leftBuffer, offset, chunkSize);
        _leftReceived += chunkSize;

        if (!checksumOk)
        {
            _leftIssues.Add(new ChunkIssue(offset, chunkSize));
            AppendRawLog("LEFT_IMAGE", $"Checksum mismatch | offset={offset} size={chunkSize}");
        }
    }

    private void ProcessRightChunk(uint offset, byte chunkSize, byte[] data, bool checksumOk)
    {
        if (_rightBuffer.Length == 0)
        {
            AppendRawLog("RIGHT_IMAGE", "Chunk received before image size");
            return;
        }

        if ((offset + chunkSize) > _rightBuffer.Length)
        {
            AppendRawLog("RIGHT_IMAGE", $"Chunk out of bounds | offset={offset} size={chunkSize}");
            return;
        }

        Array.Copy(data, 0, _rightBuffer, offset, chunkSize);
        _rightReceived += chunkSize;

        if (!checksumOk)
        {
            _rightIssues.Add(new ChunkIssue(offset, chunkSize));
            AppendRawLog("RIGHT_IMAGE", $"Checksum mismatch | offset={offset} size={chunkSize}");
        }
    }

    private void TryFinalizeImages()
    {
        bool leftDone = _leftImageSize > 0 && _leftReceived >= _leftImageSize;
        bool rightDone = _rightImageSize > 0 && _rightReceived >= _rightImageSize;

        if (leftDone && !_leftSaved)
        {
            File.WriteAllBytes(_leftImagePath, _leftBuffer);
            _leftSaved = true;

            AppendRawLog("LEFT_IMAGE", $"Saved {_leftImagePath}");
            LoadImageIntoControl(_leftImagePath, LeftImageControl);

            if (_leftIssues.Count > 0)
            {
                foreach (ChunkIssue issue in _leftIssues)
                {
                    AppendRawLog("LEFT_IMAGE_CORRUPT", $"offset={issue.Offset} size={issue.Size}");
                }
            }
        }

        if (rightDone && !_rightSaved)
        {
            File.WriteAllBytes(_rightImagePath, _rightBuffer);
            _rightSaved = true;

            AppendRawLog("RIGHT_IMAGE", $"Saved {_rightImagePath}");
            LoadImageIntoControl(_rightImagePath, RightImageControl);

            if (_rightIssues.Count > 0)
            {
                foreach (ChunkIssue issue in _rightIssues)
                {
                    AppendRawLog("RIGHT_IMAGE_CORRUPT", $"offset={issue.Offset} size={issue.Size}");
                }
            }
        }

        if (_leftSaved && _rightSaved && !_anaglyphSaved)
        {
            TryCreateAndLoadAnaglyph();
            _anaglyphSaved = true;
        }
    }

    private void TryCreateAndLoadAnaglyph()
    {
        try
        {
            if (!File.Exists(_leftImagePath) || !File.Exists(_rightImagePath))
            {
                return;
            }

            using SixLabors.ImageSharp.Image<L8> leftImage =
                SixLabors.ImageSharp.Image.Load<L8>(_leftImagePath);

            using SixLabors.ImageSharp.Image<L8> rightImage =
                SixLabors.ImageSharp.Image.Load<L8>(_rightImagePath);

            int outputWidth = Math.Min(leftImage.Width, rightImage.Width);
            int outputHeight = Math.Min(leftImage.Height, rightImage.Height);

            using SixLabors.ImageSharp.Image<L8> croppedLeft =
                leftImage.Clone(context => context.Crop(outputWidth, outputHeight));

            using SixLabors.ImageSharp.Image<L8> croppedRight =
                rightImage.Clone(context => context.Crop(outputWidth, outputHeight));

            using SixLabors.ImageSharp.Image<Rgba32> anaglyph =
                CreateAnaglyph(croppedLeft, croppedRight);

            anaglyph.SaveAsJpeg(_anaglyphImagePath, new JpegEncoder());

            AppendRawLog("ANAGLYPH", $"Saved {_anaglyphImagePath}");
            LoadImageIntoControl(_anaglyphImagePath, OutputImageControl);
        }
        catch (Exception ex)
        {
            AppendRawLog("ANAGLYPH", $"Creation failed: {ex.Message}");
        }
    }

    private static SixLabors.ImageSharp.Image<Rgba32> CreateAnaglyph(
        SixLabors.ImageSharp.Image<L8> left,
        SixLabors.ImageSharp.Image<L8> right)
    {
        int width = left.Width;
        int height = left.Height;

        SixLabors.ImageSharp.Image<Rgba32> output =
            new SixLabors.ImageSharp.Image<Rgba32>(width, height);

        output.ProcessPixelRows(
            left,
            right,
            (outputAccessor, leftAccessor, rightAccessor) =>
            {
                for (int row = 0; row < outputAccessor.Height; row++)
                {
                    Span<Rgba32> outputRow = outputAccessor.GetRowSpan(row);
                    Span<L8> leftRow = leftAccessor.GetRowSpan(row);
                    Span<L8> rightRow = rightAccessor.GetRowSpan(row);

                    for (int col = 0; col < outputRow.Length; col++)
                    {
                        byte leftValue = leftRow[col].PackedValue;
                        byte rightValue = rightRow[col].PackedValue;

                        outputRow[col] = new Rgba32(leftValue, rightValue, rightValue);
                    }
                }
            });

        return output;
    }

    private void LoadExistingImages()
    {
        if (File.Exists(_leftImagePath))
        {
            LoadImageIntoControl(_leftImagePath, LeftImageControl);
        }

        if (File.Exists(_rightImagePath))
        {
            LoadImageIntoControl(_rightImagePath, RightImageControl);
        }

        if (File.Exists(_anaglyphImagePath))
        {
            LoadImageIntoControl(_anaglyphImagePath, OutputImageControl);
        }
    }

    private void LoadImageIntoControl(string imagePath, AvaImage targetControl)
    {
        try
        {
            if (!File.Exists(imagePath))
            {
                return;
            }

            byte[] imageBytes = File.ReadAllBytes(imagePath);

            using MemoryStream memoryStream = new MemoryStream(imageBytes);
            Bitmap bitmap = new Bitmap(memoryStream);

            Dispatcher.UIThread.Post(() =>
            {
                targetControl.Source = bitmap;
            });
        }
        catch (Exception ex)
        {
            AppendRawLog("IMAGE_LOAD", $"{Path.GetFileName(imagePath)} load failed: {ex.Message}");
        }
    }

    private void AppendRawLog(string dataName, string dataValue)
    {
        string timestamp = DateTime.Now.ToString("HH:mm:ss");

        Dispatcher.UIThread.Post(() =>
        {
            RawLogTextBlock.Text += $"{timestamp} - {dataName} - {dataValue}{Environment.NewLine}";
            _rawLogLineCount++;

            if (_rawLogLineCount > MaxRawLogLines)
            {
                string[] lines = RawLogTextBlock.Text.Split(Environment.NewLine, StringSplitOptions.None);

                if (lines.Length > 50)
                {
                    RawLogTextBlock.Text = string.Join(Environment.NewLine, lines[50..]);
                    _rawLogLineCount = Math.Max(0, _rawLogLineCount - 50);
                }
            }

            RawScrollViewer.Offset = new Vector(0, double.MaxValue);
        });
    }

    private static byte[] ReadExact(SerialPort port, int length)
    {
        byte[] buffer = new byte[length];
        int totalRead = 0;

        while (totalRead < length)
        {
            int bytesRead = port.Read(buffer, totalRead, length - totalRead);

            if (bytesRead <= 0)
            {
                throw new IOException("No serial data received.");
            }

            totalRead += bytesRead;
        }

        return buffer;
    }

    private static ushort ReadUInt16BigEndian(byte[] data, int index)
    {
        return (ushort)(((ushort)data[index] << 8) | data[index + 1]);
    }

    private static uint ReadUInt32BigEndian(byte[] data, int index)
    {
        return ((uint)data[index] << 24)
             | ((uint)data[index + 1] << 16)
             | ((uint)data[index + 2] << 8)
             | data[index + 3];
    }

    private static short ReadInt16BigEndian(byte[] data, int index)
    {
        return (short)(((ushort)data[index] << 8) | data[index + 1]);
    }

    private static byte ComputeChecksum(byte type, byte lenH, byte lenL, byte[] payload)
    {
        byte checksum = 0;
        checksum ^= type;
        checksum ^= lenH;
        checksum ^= lenL;

        for (int i = 0; i < payload.Length; i++)
        {
            checksum ^= payload[i];
        }

        return checksum;
    }

    protected override void OnClosed(EventArgs e)
    {
        _cancellationTokenSource.Cancel();
        CleanupSerialPortReference();
        base.OnClosed(e);
    }
}