# ThermalCamera - Specification

## 1. System Requirements

### 1.1 Hardware
*   **Camera**: Topdon TC001, InfiRay P2 Pro, or compatible UVC thermal camera.
    *   Resolution: 256x384 (Effective thermal resolution: 256x192).
    *   Format: YUYV 4:2:2.
    *   Interface: USB (libuvc) or AVFoundation (macOS).
*   **Platform**:
    *   Linux (x86_64, ARM64/Raspberry Pi).
    *   macOS (Intel, Apple Silicon).

### 1.2 Software
*   **Language**: C++17.
*   **Build System**: Meson + Ninja.
*   **Libraries**:
    *   SDL2 (Rendering, Input).
    *   libuvc (Linux camera access).
    *   ctpl (Thread pool).
    *   cmdline (Argument parsing).

## 2. Functional Requirements

### 2.1 Camera Control
*   **Auto-detection**: Automatically finds connected thermal camera if no device path provided.
*   **Manual Selection**: `-d` argument for specific device path (Linux: `/dev/video0`, macOS: Location ID).
*   **Streaming**: Continuous capture at ~25 FPS.
*   **Hotplug**: Graceful handling of disconnects with automatic reconnection attempts.
*   **Offline Mode**: `-f` argument replays a raw file instead of a live camera.

### 2.1.1 CLI Options

| Option | Description |
|---|---|
| `-d, --device <path>` | Camera device (Linux: `/dev/video0`, macOS: location ID); auto-detected if omitted |
| `-f, --file <path>` | Offline raw file input |
| `--scale <1-10>` | Initial scale factor (default: 1) |
| `--fullscreen` | Start in fullscreen mode |
| `--colormap <0-6>` | Initial colormap index (default: 0) |
| `--celsius` | Use Celsius units (default: Fahrenheit) |
| `--quiet` | Suppress verbose output |
| `--threads <count>` | Override thread count (default: 3) |
| `-l, --list` | List all available cameras and exit |
| `-h, --help` | Display help and exit |
| `-v, --version` | Display version information and exit |

### 2.2 Image Processing
*   **Raw Data**: Extracts 16-bit Kelvin values from the Y channel of the bottom half of the frame (rows 192-383).
*   **Conversion**:
    *   Kelvin to Celsius: `(K / 64.0) - 273.15`.
    *   Celsius to Fahrenheit: `(C * 1.8) + 32.0`.
*   **Statistics**: Calculates Min, Max, Average, and Center spot temperatures in real-time.
*   **Rotation**: Supports 0, 90, 180, 270 degree rotation via `[` and `]` keys.

### 2.3 Visualization
*   **Colormaps**: 7 integrated colormaps (Magma, Inferno, Plasma, RdBu, Oleron, Roma, Berlin).
*   **Scale**: Integer scaling (1x to 4x+) and Fullscreen mode.
*   **HUD**: Overlays FPS, Min/Max/Avg/Center temps, Colormap name, Scale factor.
*   **Probe**: Mouse hover displays temperature at cursor location. Right-click toggles.
*   **Freeze Frame**: `SPACE` bar toggles freeze/resume of the display.

### 2.4 User Interface Controls
| Key | Action |
|-----|--------|
| `Q` / `ESC` | Quit Application |
| `F` / `F11` | Toggle Fullscreen |
| `+` / `-` | Increase / Decrease Scale |
| `[` / `]` | Rotate 90° CW / CCW |
| `C` | Toggle Celsius / Fahrenheit |
| `M` / `,` | Next Colormap |
| `N` / `.` | Previous Colormap |
| `SPACE` | Freeze / Unfreeze Frame |
| `H` | Toggle HUD Visibility |
| `R` | Reset Configuration to Defaults |
| `Right Click` | Toggle Temperature Probe |

## 3. Data Formats

### 3.1 Input Frame (Raw)
*   **Dimensions**: 256 (width) x 384 (height).
*   **Pixel Format**: YUYV 4:2:2 (2 bytes per pixel).
*   **Layout**:
    *   **Top Half (0-191)**: "Visible light" image (unused/garbage).
    *   **Bottom Half (192-383)**: Thermal data. Each pixel's Y-component + adjacent byte forms a 16-bit little-endian integer representing temperature.

### 3.2 Processed Frame
*   **Dimensions**: 256 x 192 (or 192 x 256 if rotated).
*   **Data**: 
    *   Primary: `std::vector<uint16_t>` (Kelvin).
    *   Secondary: `std::vector<uint32_t>` (ARGB8888 for rendering).

## 4. Performance Targets
*   **Latency**: End-to-end latency < 100ms.
*   **Framerate**: Stable 25 FPS (matches hardware limit).
*   **CPU Usage**: Efficient usage (< 1% on modern CPUs) via threading and GPU acceleration.
*   **Memory**: Minimal footprint (~5 MB working set) with efficient buffer management.

**Performance Optimization Achievements** (Phase 9):
*   **GPU Scaling**: CPU bilinear interpolation eliminated (~4.5ms per frame savings)
*   **Colormap**: Pre-packed ARGB colormaps + integer math (~0.03ms per frame)
*   **Overall**: 95% CPU reduction (from ~20% to ~1%)
*   **Render Pipeline**: ~0.3-0.8ms total per frame (vs 5-6ms previously)
