# Deviations from Implementation Plans

This document tracks technical and functional deviations between the original implementation plans (Phases 1-6) and the final codebase. These changes were made during development to improve performance, reduce dependencies, or enhance usability.

## 1. Technical Stack & Build System

### C++ Standard Upgrade
*   **Plan**: Target C++14.
*   **Implementation**: Upgraded to **C++17**.
*   **Reason**: To leverage modern language features (e.g., `if constexpr`, structured bindings, improved standard library support) and ensure better future compatibility.

### Build Configuration
*   **Plan**: Basic Meson setup.
*   **Implementation**: Added strict compiler warnings (`-Wall`, `-Wextra`) and optimization flags (`-O3`, `-march=native`) by default to ensure maximum performance for real-time image processing.

## 2. Dependencies

### Font Rendering (SDL2_ttf)
*   **Plan**: Use `SDL2_ttf` for rendering text (HUD, temperature values).
*   **Implementation**: Removed `SDL2_ttf` dependency. Implemented a lightweight, header-only bitmap font renderer (`src/fonts/EspySans_10.h`).
*   **Reason**: 
    *   Reduces external dependencies (no need for `freetype` or `SDL2_ttf` binaries).
    *   Simplifies cross-platform distribution (especially on macOS/Linux where managing shared library dependencies can be complex).
    *   Improves startup time (no font file loading).

## 3. User Interface & Controls

### Key Bindings
*   **Plan**: 
    *   `M` / `N` for Next/Previous Colormap.
    *   `F` for Fullscreen.
*   **Implementation**: 
    *   Added alternative keys: `.` (Next) and `,` (Previous) to match standard media player conventions (frame stepping).
    *   Added `F11` as an alternative for Fullscreen (standard on Windows/Linux).
    *   Added **Right Click** to toggle the temperature probe (feature not originally specified).

### Probe Behavior
*   **Plan**: Simple hover to show temperature.
*   **Implementation**: Added a toggle state (Right Click) to enable/disable the probe, preventing visual clutter when not needed.

## 4. Architecture & Design

### Threading Model
*   **Plan**: Generic 3-thread producer/consumer model.
*   **Implementation**: 
    *   Utilized `ctpl_stl_tls.h` for a managed thread pool instead of raw `std::thread`.
    *   Main Thread handles Rendering (required by SDL2 on macOS).
    *   Two worker threads explicitly assigned for Capture and Processing via the pool.
*   **Reason**: Simplifies thread lifecycle management and cleanup on shutdown.

### Shared State Management
*   **Plan**: Abstract "Pipeline" class.
*   **Implementation**: `SharedState` struct defined in `main.cpp` holding the `FrameBuffer`, `Config`, and synchronization primitives.
*   **Reason**: Reduced boilerplate. Since `main` coordinates the high-level lifecycle, a local struct passed via `shared_ptr` was cleaner than a heavy singleton or complex manager class.

### Camera Abstraction
*   **Plan**: Platform-specific `#ifdef` blocks potentially scattered.
*   **Implementation**: Clean `ICamera` interface pattern with factory-style creation.
    *   `src/camera/MacCameraImpl` (Objective-C++) for macOS.
    *   `src/camera/UvcCameraImpl` (C++) for Linux.
*   **Reason**: strictly separates platform-specific headers (like `<AVFoundation/AVFoundation.h>` or `<libuvc/libuvc.h>`) preventing compilation issues on opposing platforms.

## 5. Thermal Processing

### Raw Data Handling
*   **Plan**: Parse YUYV data.
*   **Implementation**: Explicitly ignores the top half of the frame (Rows 0-191) as it contains "Visible Light" garbage data on the TC001, focusing strictly on the bottom half (Rows 192-383) for thermal data.
*   **Optimization**: Implemented loop unrolling (stride of 8) in `CalculateTemperatureStats` for Min/Max/Avg calculations to maximize CPU throughput.
