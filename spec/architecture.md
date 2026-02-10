# ThermalCamera - Architecture

## System Overview

ThermalCamera employs a multi-threaded architecture optimized for low-latency video processing and rendering. The core design pattern is a producer-consumer pipeline with double buffering at each stage to minimize locking and maximize throughput.

### Core Components

1.  **Camera Thread (Producer)**
    -   **Responsibility**: Interfaces with hardware (libuvc/AVFoundation).
    -   **Behavior**:
        -   Runs in a dedicated thread to service camera driver callbacks or polling loops.
        -   Writes raw YUYV frames into a `SharedState` buffer managed by `FrameBuffer`.
        -   Handles platform-specific camera initialization and error recovery.
    -   **Synchronization**: Uses a mutex to swap the "write" buffer with the "read" buffer when a full frame is ready.

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

### Error Handling & Reliability

*   **Global Exception Handling**: Top-level try-catch blocks in `main` and thread workers to prevent crashes.
*   **Graceful Degredation**:
    *   **Camera Disconnect**: Automatically enters a "frozen" state if the camera is unplugged, with a visual indicator.
    *   **Resource Cleanup**: RAII wrappers ensure file handles, textures, and memory are released on exit or error.
*   **Logging**: Structured logging to `stderr` for runtime errors and debugging information.

