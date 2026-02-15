# Performance Optimization Work Log

## Initial State
- **Symptoms**: High CPU usage (~100% of a core), low frame rate (~6 FPS when scaled), significant system overhead.
- **Goal**: Reduce CPU usage to <5% and achieve stable 25+ FPS (camera limit).

## Investigation & Analysis

### 1. Profiling (Time Profiler)
Using `xcrun xctrace` and Instruments, we identified two massive bottlenecks:

1.  **CPU Scaling (`Render::Renderer::ScaleFrame`) - 1.16s per frame**
    - The application was performing software-based bilinear interpolation to scale the 256x192 thermal image to the window size (e.g., 768x576).
    - This single function consumed the vast majority of the application's main thread time.

2.  **macOS Reaction Effects (System Overhead) - ~20% CPU**
    - The trace showed heavy activity in `VCPHandGestureVideoRequest`, `Espresso`, and `ANECompilerEngine`.
    - **Cause**: macOS Sonoma (14.0+) automatically enables "Reaction Effects" (hand gesture detection) for video cameras. This runs Neural Engine models on every frame, even for a thermal camera which is inappropriate and resource-heavy.

### 2. Secondary Issues
- **Colormap Application**: Relied on per-pixel floating-point math and manual bit-shifting for ARGB packing.
- **Memory Safety**: A crash (Segmentation Fault) occurred during the transition to GPU scaling due to a buffer size mismatch.

## Corrective Actions

### Phase 1: GPU Scaling (The Big Win)
**Problem**: Software scaling is O(N*Scale^2) and runs on the CPU.
**Solution**: Offload scaling to the GPU via SDL2.
- **Changes**:
    - Removed `Renderer::ScaleFrame` function.
    - Updated `SDL_CreateTexture` to use the **source resolution** (256x192) instead of the target window resolution.
    - Updated `SDL_RenderCopy` to draw the small source texture into the full window viewport. SDL (and the underlying Metal/OpenGL driver) handles the scaling almost instantly.
- **Result**: `ScaleFrame` time went from 1.16s to 0s.

### Phase 2: Disabling macOS Reaction Effects
**Problem**: Unwanted system-level ML processing.
**Solution**: Programmatically disable the feature.
- **Changes**:
    - **Info.plist**: Added `<key>NSCameraReactionEffectGesturesEnabledDefault</key><false/>`.
    - **Code (`MacAVFoundationStreamer.mm`)**: 
        - Added runtime checks using `@available(macOS 14.0, *)`.
        - Uses Key-Value Coding (KVC) to set `reactionEffectGesturesEnabled` to `NO` on the `AVCaptureDevice`.
        - Implemented a double-check mechanism in `startStreaming` to re-disable it if the system re-enables it upon session start.
        - Attempted to clear `videoEffects` on the `AVCaptureConnection` as a fallback.

### Phase 3: Colormap Optimization
**Problem**: Expensive inner loop for pixel coloring.
**Solution**: Pre-calculation and Integer Math.
- **Changes**:
    - Pre-generated ARGB `uint32_t` lookup tables for all colormaps at startup.
    - Replaced floating-point scaling logic `(val - min) * 255.0f / range` with integer math `(val - min) * 255 / range`.
    - Replaced manual bit-shifting `(r << 16 | g << 8 ...)` with direct array lookups.

### Phase 4: Bug Fixes
**Problem**: Segmentation Fault during rendering.
**Cause**: The `_pixelBuffer` vector could grow to match a previous frame size, but the `memcpy` to the SDL texture (which is now fixed at source resolution) was using `_pixelBuffer.size()`. This caused a buffer overflow when the texture was smaller than the allocated vector capacity.
**Fix**: Updated `memcpy` to use the exact frame dimensions (`frame._width * frame._height`) for the copy size.

### Phase 5: HUD Optimization (Texture Atlas)
**Problem**: The trace showed significant CPU time in `SDL_RenderDrawPointsF_REAL` (and `snprintf/stringstream`) within `RenderHUD`.
- Drawing text pixel-by-pixel using `SDL_RenderDrawPoint` is extremely inefficient (thousands of draw calls per frame).
- `std::stringstream` caused unnecessary allocation overhead every frame.

**Solution**:
- **Texture Atlas**: Implemented `InitializeFont` to generate a single texture containing all glyphs from `EspySans_10` at startup.
- **Batch Rendering**: Updated `DrawText` to use `SDL_RenderCopy` to blit glyphs from the atlas, replacing thousands of point draws with a few dozen texture copies.
- **String Optimization**: Replaced `std::stringstream` with `snprintf` and a reusable stack buffer in `RenderHUD`.

## Final Status
- **CPU Usage**: Expected to be minimal (<5%).
- **FPS**: Should be limited only by the camera hardware (25 FPS or 9Hz depending on model).
- **Stability**: Crash resolved, threading issues addressed.
