# ThermalCamera - Architecture

## System Overview

ThermalCamera employs a multi-threaded architecture optimized for low-latency video processing and rendering. The core design pattern is a producer-consumer pipeline with double buffering at each stage to minimize locking and maximize throughput.

### Core Components

1.  **Camera Thread (Producer)**
    -   **Responsibility**: Interfaces with hardware (libuvc/AVFoundation).
    -   **Behavior**:
        -   Runs in a dedicated thread with a **Blocking Loop** (waits for new frame or timeout) to minimize CPU usage.
        -   Writes raw YUYV frames into a `SharedState` buffer managed by `FrameBuffer`.
        -   **Reconnection**: Handles dynamic camera hotplugging by detecting disconnects and re-scanning for devices (essential for macOS Location ID changes).
    -   **Synchronization**: Uses a mutex/semaphore to block until hardware provides data.

2.  **Processing Thread (Consumer/Producer)**
    -   **Responsibility**: Converts raw data to thermal values (Kelvin).
    -   **Behavior**:
        -   Waits for a signal (`_frameReady` condition variable) from the Camera Thread.
        -   Reads the latest raw frame.
        -   Applies radiometric conversion (Raw YUYV -> Kelvin/Celsius).
        -   Calculates statistics (Min/Max/Avg/Center temperature).
        -   Writes processed `ThermalFrame` into a `SharedState` buffer.
    -   **Synchronization**: Uses `std::condition_variable` to wake up on new frames. Swaps its output buffer for the Render Thread.

3.  **Render Thread (Consumer)**
    -   **Responsibility**: Visualization and UI.
    -   **Behavior**:
        -   Runs on the **Main Thread** (required by SDL2 on macOS).
        -   Polls for the latest processed frame.
        -   Maps temperature values to colors (Colormap) using a lookup table.
        -   Performs bilinear scaling to the window size.
        -   Renders the HUD (Heads-Up Display) with temperature stats.
        -   Handles user input (Keyboard/Mouse) for controls.
    -   **Synchronization**: Accesses the latest "read" frame from the Processing Thread using a mutex.

### Data Flow

```mermaid
graph LR
    HW[Camera Hardware] -->|USB/PCIe| Driver[Driver Layer]
    Driver -->|Callback| CamThread[Camera Thread]
    CamThread -- Raw Frame (YUYV) --> Buffer1[Double Buffer]
    Buffer1 --> ProcThread[Processing Thread]
    ProcThread -- Thermal Frame (Kelvin) --> Buffer2[Double Buffer]
    Buffer2 --> RenderThread[Render Thread (Main)]
    RenderThread -->|SDL2| Display[Display]
```

### Platform Abstraction

The system uses an interface-based design for hardware abstraction:

*   **`ICamera`**: Pure virtual interface defining standard camera operations.
    *   `Initialize()`
    *   `StartStreaming()`
    *   `StopStreaming()`
    *   `GetFrame()` (Copy-based retrieval)
*   **`UvcCameraImpl` (Linux)**: Implements `ICamera` using `libuvc` for direct USB access.
    *   Manages `uvc_context` and `uvc_device_handle`.
    *   Uses a callback-based stream which copies data to the app's buffer.
*   **`MacCameraImpl` (macOS)**: Implements `ICamera` using:
    *   **`MacCameraProvider` (Obj-C)**: Bridges C++ to AVFoundation.
    *   **`MacAVFoundationStreamer`**: Handles video capture via CoreMedia.
    *   **`MacIOKitUvcController`**: Handles UVC extension units (controls) via IOKit.

### Data Structures

*   **`ThermalFrame`**:
    *   `_data`: Vector of 16-bit Kelvin values (256x192).
    *   `_min`, `_max`, `_avg`, `_center`: `Temperature` structs containing K/C/F values and coordinates.
*   **`Config`**:
    *   Centralized configuration (Colormap, Scale, Units, Rotation) used across threads.

### Build System

*   **Meson**: Primary build configuration.
*   **Dependencies**:
    *   **SDL2**: Graphics and input.
    *   **libuvc**: Camera access (Linux only).
    *   **Threads**: Standard C++11 threading.
    *   **Apple Frameworks**: AVFoundation, CoreMedia, IOKit (macOS only).

### Rendering Optimizations

*   **GPU Acceleration**:
    *   Texture stored at source resolution (256×192) with `SDL_TEXTUREACCESS_STATIC`
    *   Scaling delegated to GPU during `SDL_RenderCopy()` operation
    *   **Eliminated**: CPU bilinear interpolation (~4.5ms per frame, 13M pixels/sec at 30 FPS)
    *   **Memory Savings**: 1.7 MB buffer removed (no scaling buffer)

*   **Colormap Optimization**:
    *   Pre-packed colormaps to ARGB8888 format at initialization
    *   Eliminated per-pixel bitshift operations: `(255 << 24) | (r << 16) | (g << 8) | b`
    *   Integer math replaces floating-point: `(val - minK) * 255 / (maxK - minK)`
    *   **Result**: ~0.03ms per frame, 5% CPU reduction

*   **Rendering Pipeline**:
    *   Thermal frame → Colormap application (CPU, ~0.03ms)
    *   → Texture update (`SDL_UpdateTexture`, ~0.08ms)
    *   → GPU scaling + letterboxing (SDL hardware accelerated)
    *   → HUD rendering (scaled to actual viewport size)
    *   **Total Render Time**: ~0.3-0.8ms per frame (vs 5-6ms before)

### Error Handling & Reliability

*   **Global Exception Handling**: Top-level try-catch blocks in `main` and thread workers to prevent crashes.
*   **Platform-Specific Error Handling**:
    *   **macOS**: Listens for `AVCaptureDeviceWasDisconnectedNotification` to trigger immediate recovery.
    *   **Reconnection Loop**: Automatically retries connection every second if camera is lost, re-scanning device paths if necessary.
*   **Rendering Robustness**:
    *   **Letterboxing**: Uses `SDL_RenderSetViewport` to maintain correct aspect ratio regardless of window size or fullscreen state.
    *   **Aspect Ratio Handling**: Calculates proper viewport based on window vs image aspect ratio (4:3 for thermal).
    *   **Mouse Coordinate Mapping**: Transforms screen coordinates to thermal frame coordinates accounting for viewport and letterbox.
    *   **Probe Scaling**: Mouse probe text and cursor correctly positioned in rendered image space at any scale factor.
*   **Graceful Degredation**:
    *   **Camera Disconnect**: Automatically enters a "frozen" state if the camera is unplugged, with a visual indicator.
    *   **Resource Cleanup**: RAII wrappers ensure file handles, textures, and memory are released on exit or error.
*   **Logging**: Structured logging to `stderr` for runtime errors and debugging information.

