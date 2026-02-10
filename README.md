# ThermalCamera

A cross-platform thermal camera viewer application for Linux and macOS that captures thermal imaging data from USB thermal cameras and displays it with real-time temperature visualization.

## Features

- **Live Thermal Camera Display**: Real-time thermal imaging from USB thermal cameras
- **Temperature Unit Conversion**: Display temperatures in Celsius or Fahrenheit
- **Multiple Colormaps**: 7 built-in colormaps (Magma, Inferno, Plasma, RdBu, Oleron, Roma, Berlin)
- **Scaling & Rotation**: Adjustable scale factor (1-10X) and 90° rotation
- **Mouse Temperature Probe**: Point-and-click temperature measurement
- **Freeze Frame**: Pause live display to examine specific frames
- **On-Screen HUD**: Real-time temperature statistics (min, max, average, center)
- **Cross-Platform Support**: Runs on Linux and macOS
- **High Performance**: 3-threaded architecture (capture/process/render) for smooth operation
- **Embedded Fonts**: No external font dependencies

## Platform Support

### Linux
- Uses libuvc for USB camera access
- Supports V4L2-compatible thermal cameras
- Tested with Topdon TC001 and compatible clones

### macOS
- Uses AVFoundation/CoreMedia for camera access
- Bundled as macOS .app application
- Supports USB thermal cameras via IOKit

## Requirements

### Common
- C++14 compatible compiler (GCC 5+ or Clang 3.4+)
- Meson build system
- SDL2 (version 2.0.0 or higher)
- CMake (for subproject dependencies)

### Linux
- libuvc development headers
- libusb-1.0 development headers

### macOS
- Xcode command line tools
- macOS SDK

## Building

### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install meson ninja-build libsdl2-dev libusb-1.0-0-dev

# Build the project
meson setup builddir
meson compile -C builddir

# Run the application
./builddir/thermalcamera
```

### macOS

```bash
# Install dependencies
brew install meson sdl2 libuvc

# Build the project
meson setup builddir
meson compile -C builddir

# The build process creates a .app bundle
open builddir/ThermalCamera.app
```

## Usage

### Command-Line Options

```
thermalcamera [OPTIONS]

Options:
  -d, --device <path>       Camera device identifier
                            Linux: /dev/video0 (default)
                            macOS: location ID (auto-detected)
  -f, --file <path>         Offline raw file input
  --scale <1-10>            Initial scale factor (default: 1)
  --fullscreen              Start in fullscreen mode
  --colormap <0-6>          Initial colormap index (default: 0)
  --celsius                 Use Celsius units (default: Fahrenheit)
  --quiet                   Suppress verbose output
  --threads <count>         Override thread count (default: 3)
  -l, --list                List all available cameras and exit
  -h, --help                Display this help and exit
```

### Examples

```bash
# Run with default settings (auto-detect camera)
./thermalcamera

# Specify camera device (Linux)
./thermalcamera --device /dev/video1

# Use Celsius and start fullscreen
./thermalcamera --celsius --fullscreen

# Start with 2X scale and colormap #3
./thermalcamera --scale 2 --colormap 3

# List available cameras
./thermalcamera --list
```

## Key Bindings

| Key | Action |
|-----|--------|
| Q / ESC | Quit application |
| F / F11 | Toggle fullscreen |
| + / = | Scale up (max 10X) |
| - | Scale down (min 1X) |
| [ / ] | Rotate 90° (clockwise / counter-clockwise) |
| M / < | Next colormap |
| N / > | Previous colormap |
| C | Toggle Celsius/Fahrenheit |
| SPACE | Freeze/unfreeze frame |
| R | Reset to defaults |
| H | Toggle HUD |

## Camera Compatibility

### Tested Devices

- Topdon TC001 Thermal Camera
- TC001 Clones and compatible USB thermal cameras

### Requirements

- YUYV 4:2:2 video format support
- 384x256 frame resolution
- USB 2.0 or higher

### Platform-Specific Notes

#### Linux

You may need to configure udev rules to access the camera device without root privileges:

```bash
# Create udev rule
sudo nano /etc/udev/rules.d/99-thermal-camera.rules

# Add this line (replace XXXX:YYYY with your device's vendor/product IDs)
SUBSYSTEM=="video4linux", ATTR{idVendor}=="XXXX", ATTR{idProduct}=="YYYY", MODE="0666"

# Reload udev rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

#### macOS

On first run, you may need to grant camera permissions:

1. Open System Settings → Privacy & Security → Camera
2. Find ThermalCamera in the list and enable camera access

## Troubleshooting

### "No thermal camera found"
- Ensure the camera is properly connected
- On Linux, check `ls /dev/video*` for available devices
- On macOS, try specifying the device with `--device`
- Run with `--list` to see all available cameras

### "SDL_Init Error"
- Install SDL2 development libraries
- Ensure SDL2 is in your library path

### Poor performance
- Reduce scale factor with `-` key
- Disable HUD with `H` key
- Check system resources (CPU, memory)

### Camera authorization (macOS)
- Grant camera permissions in System Settings
- Ensure the app is not blocked by security settings

## Architecture

The application uses a 3-threaded architecture:

1. **Capture Thread**: Acquires raw video frames from the camera
2. **Processing Thread**: Converts raw data to thermal temperatures, applies colormaps
3. **Render Thread** (Main): Displays processed frames using SDL

### Double Buffering

Two double-buffer systems are used:
- Raw Frame Buffer: Between capture and processing threads
- Processed Frame Buffer: Between processing and render threads

### Platform Abstraction

The camera interface is abstracted behind `ICamera`, with platform-specific implementations:
- Linux: `UvcCameraImpl` (libuvc)
- macOS: `MacCameraImpl` (AVFoundation)

## Development

### Project Structure

```
ThermalCamera/
├── meson.build          # Build configuration
├── src/
│   ├── main.cpp         # Application entry point
│   ├── Error.hpp        # Error handling utilities
│   ├── Profile.hpp      # Performance profiling utilities
│   ├── FrameBuffer.hpp  # Double buffering implementation
│   ├── camera/          # Camera implementations
│   ├── thermal/         # Thermal data processing
│   ├── render/          # SDL rendering
│   ├── config/          # Configuration management
│   ├── fonts/           # Embedded fonts
│   └── vendor/          # Third-party dependencies
└── plans/               # Implementation plans
```

### Code Style

- Indentation: 4 spaces (no tabs)
- Naming: PascalCase for classes/functions, _camelCase for members
- C++ Standard: C++14
- See [CODING_GUIDELINES.md](CODING_GUIDELINES.md) for details

### Building for Development

```bash
# Debug build with symbols
meson setup builddir --buildtype=debug

# Release build with optimization
meson setup builddir --buildtype=release

# Verbose build output
meson compile -C builddir --verbose

# Run tests
meson test -C builddir
```

## Performance

On typical hardware, the application achieves:
- Capture: ~30 FPS
- Processing: < 20ms per frame
- Rendering: < 50ms per frame
- Total: ~15-20 FPS at 1X scale

## License

See LICENSE file for details.

## Contributing

Contributions are welcome! Please follow the coding guidelines and submit pull requests.

## Support

For issues and questions, please use the project's issue tracker.

## Acknowledgments

- **Thermal-Camera-Redux**: Base reference implementation by [92es](https://github.com/92es/Thermal-Camera-Redux)
- **PyThermalCamera**: Inspiration for temperature conversion logic by [leswright1977](https://github.com/leswright1977/PyThermalCamera)
- **SDL2**: Cross-platform graphics library
- **libuvc**: Linux USB camera support
- **AVFoundation**: macOS camera support
