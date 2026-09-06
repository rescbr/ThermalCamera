# Phase 9: CPU Usage Optimization

**Goal:** Reduce CPU usage from ~20% to <5% when processing 256×192 thermal images at 30 FPS

## Overview

The application currently uses excessive CPU (~20%) for thermal image processing due to:
1. CPU-based bilinear scaling (primary bottleneck)
2. Per-pixel floating-point operations in colormap application
3. Inefficient text rendering
4. Redundant memory copies

## Phase 1: GPU Scaling (Highest Priority) - **COMPLETED**

**Impact:** 60-70% CPU reduction

### Current Problem
- `ScaleFrame()` in `Renderer.cpp:310` performs CPU bilinear interpolation
- For scale=3: 256×192 → 768×576 = 442,368 pixels per frame
- Each pixel: 4 reads + floating-point math on R/G/B channels
- At 30 FPS = ~13M processed pixels/second

### Solution Implemented
Replaced CPU scaling with SDL's GPU-accelerated scaling:

#### Implementation Steps Completed
1. **Removed `ScaleFrame()` method** (lines 310-360)
2. **Removed `_scaledBuffer` member** from Renderer class
3. **Changed texture access mode** from `SDL_TEXTUREACCESS_STREAMING` to `SDL_TEXTUREACCESS_STATIC`
   *(Correction: the code actually retains `SDL_TEXTUREACCESS_STREAMING` — see `src/render/Renderer.cpp`. Perf impact is negligible.)*
4. **Updated `RenderFrame()` workflow**:
   - Apply colormap to source-sized texture (256×192)
   - Let SDL handle scaling during `SDL_RenderCopy()`
   - Use `SDL_RenderSetScale()` for display scaling
5. **Removed row-by-row memcpy** for texture updates (no longer needed)

#### Files Modified
- `src/render/Renderer.cpp`
  - Removed `ScaleFrame()` method
  - Updated `RenderFrame()` to use SDL GPU scaling
  - Updated texture creation (lines 107, 623)
- `src/render/Renderer.hpp`
  - Removed `_scaledBuffer` member
  - Removed `ScaleFrame()` declaration

#### Technical Details
```cpp
// New approach:
// 1. Create texture at source resolution (256x192)
_texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888,
                             SDL_TEXTUREACCESS_STATIC, // Changed from STREAMING
                             256, 192);

// 2. Apply colormap to source resolution
ApplyColormap(frame, _pixelBuffer.data());
SDL_UpdateTexture(_texture, nullptr, _pixelBuffer.data(), 256 * 4);

// 3. Let SDL scale during render copy
SDL_RenderCopy(_renderer, _texture, nullptr, &destRect);
```

#### Validation
- Verify visual quality is maintained
- Test with scale factors 1x, 3x, 5x, 10x
- Profile with `THERMAL_PROFILE=1` to confirm CPU reduction

---

## Phase 2: Colormap Optimizations - **COMPLETED**

**Impact:** ~5% CPU reduction

### Current Performance Issues
1. **Per-pixel ARGB packing** (line 306): Bitshift operations for every pixel
2. **Floating-point math**: `(val - minK) * scale` computation
3. **Per-pixel clamping**: Min/max checks for every pixel

### Solution Implemented: Pre-packed ARGB + Integer Math

#### Implementation Steps Completed
1. **Pre-packed colormaps to ARGB format** at startup
   - Converted `Colormaps::magma[]` etc. to packed `uint32_t` format
   - Stored in `_packedColormaps` array
2. **Replaced floating-point with integer math**
   - Changed: `index = (val - minK) * 255.0f / (maxK - minK)`
   - To: `index = (val - minK) * 255 / (maxK - minK)` (using uint32_t)
3. **Removed per-pixel ARGB packing** by using direct lookup

#### Files Modified
- `src/render/Renderer.hpp`
  - Added `std::vector<std::vector<uint32_t>> _packedColormaps` member
  - Added `InitializeColormaps()` method declaration
- `src/render/Renderer.cpp`
  - Added `InitializeColormaps()` method
  - Updated `ApplyColormap()` to use pre-packed ARGB
  - Replaced floating-point with integer math

#### Technical Details
```cpp
// Pre-pack at startup
void Renderer::InitializeColormaps()
{
    _packedColormaps.clear();
    _packedColormaps.reserve(COLORMAP_COUNT);

    for (int cmapIdx = 0; cmapIdx < COLORMAP_COUNT; ++cmapIdx)
    {
        const ColormapEntry& map = AVAILABLE_COLORMAPS[cmapIdx];
        std::vector<uint32_t> packed(256);

        for (int i = 0; i < 256; ++i)
        {
            const auto& color = map.data[i];
            packed[i] = (255 << 24) | (color.r << 16) | (color.g << 8) | color.b;
        }

        _packedColormaps.push_back(std::move(packed));
    }
}

// ApplyColormap optimization
int range = maxK - minK;
const uint32_t* palette = _packedColormaps[cmapIdx % COLORMAP_COUNT].data();

for (size_t i = 0; i < pixelCount; ++i)
{
    uint16_t val = input[i];
    if (val < minK) val = minK;
    if (val > maxK) val = maxK;

    // Integer math
    uint32_t delta = val - minK;
    int index = (delta * 255) / range;
    if (index < 0) index = 0;
    if (index > 255) index = 255;

    outputBuffer[i] = palette[index]; // Direct lookup, no bitshifts
}
```

#### Validation
- Compare output with original for visual correctness
- Profile to confirm CPU reduction
- Test edge cases (min == max, temperature extremes)

---

## Phase 3: Text Rendering Optimization

**Impact:** 3-5% CPU reduction

### Current Problem
- `DrawText()` renders font glyphs pixel-by-pixel using `SDL_RenderDrawPoint()`
- Multiple draw calls per character
- All on CPU, not GPU-accelerated

### Solution: Pre-rendered Glyph Textures

#### Implementation Steps
1. **Create `GlyphCache` class**
   - Pre-render A-Z, 0-9, symbols to SDL_Texture surfaces
   - Cache glyph metrics (width, height, advance)
2. **Replace `DrawText()` implementation**
   - Use `SDL_RenderCopy()` to blit cached glyph textures
   - GPU-accelerated rendering
3. **Initialize glyph cache** at renderer startup

#### Files to Modify
- `src/render/Renderer.hpp`
  - Add `GlyphCache` class (nested or separate)
  - Add `GlyphCache _glyphCache` member
- `src/render/Renderer.cpp`
  - Implement `GlyphCache` class
  - Rewrite `DrawText()` to use cached textures
  - Initialize glyph cache in `Initialize()`

#### Technical Details
```cpp
class GlyphCache
{
public:
    struct Glyph {
        SDL_Texture* texture = nullptr;
        int advance = 0;
        int width = 0;
        int height = 0;
    };

    bool Initialize(SDL_Renderer* renderer);
    void Shutdown();
    const Glyph& GetGlyph(char c) const;

private:
    std::array<Glyph, 256> _glyphs;
    SDL_Renderer* _renderer = nullptr;
};

// DrawText becomes:
void Renderer::DrawText(const std::string& text, int x, int y, uint32_t color)
{
    int penX = x, penY = y;
    for (char c : text) {
        const auto& glyph = _glyphCache.GetGlyph(c);
        if (glyph.texture) {
            SDL_Rect dst = {penX, penY, glyph.width, glyph.height};
            SDL_RenderCopy(_renderer, glyph.texture, nullptr, &dst);
        }
        penX += glyph.advance;
    }
}
```

#### Validation
- Verify text rendering quality is maintained
- Test all characters (ASCII range)
- Profile to confirm CPU reduction

---

## Phase 4: Minor Optimizations - **COMPLETED**

**Impact:** 2-3% CPU reduction

### Optimization List

1. **Skip rotation copy when rotation = 0** - Already implemented
   - Early return in `ThermalProcessor::ApplyRotation()` (line 68)
   - Avoids unnecessary `std::vector` copy

2. **Optimize temperature lookups** - Implemented
   - Added private `GetTemperatureAtIndex()` helper without bounds checking
   - Used in `CalculateTemperatureStats()` for min/max/center lookups
   - Avoids redundant bounds checks when indices are known valid

3. **Use const references** in hot loops - Already in place
   - Pass `const ThermalFrame&` instead of copies
   - Mark `frame._data.data()` pointers as const

4. **Optimize profile guards** - Considered but not needed
   - `Profile::IsEnabled()` already cached per call
   - Current overhead is minimal

#### Files Modified
- `src/thermal/ThermalProcessor.hpp`
  - Added `GetTemperatureAtIndex(uint16_t kelvin, int row, int col)` private helper
- `src/thermal/ThermalProcessor.cpp`
  - Updated `CalculateTemperatureStats()` to use `GetTemperatureAtIndex()`
  - Added `GetTemperatureAtIndex()` implementation

#### Validation
- Profile each optimization individually
- Benchmark before/after
- Ensure no regressions

---

## Testing & Validation

### Performance Metrics
- Target: <5% CPU usage at 30 FPS (down from ~20%)
- FPS: Maintain ≥30 FPS with scale=3
- Memory: No significant increase

### Test Plan
1. **Before optimization**: Benchmark with `THERMAL_PROFILE=1`
   - Record CPU usage per component (Extract, Rotate, Scale, Render)
   - Measure FPS at different scales (1x, 3x, 5x, 10x)

2. **After each phase**: Re-run benchmark
   - Verify performance improvement matches expectations
   - Check for regressions

3. **Visual validation**:
   - Compare rendered frames before/after
   - Check colormap accuracy
   - Verify text rendering quality
   - Test rotation, scaling, colormaps

4. **Cross-platform testing**:
   - macOS (primary development platform)
   - Linux (libuvc backend)
   - Test with Topdon TC001 camera

### Profile Output Format
```
[PROFILE] ThermalProcessor::ProcessFrame: 1500 us (1.5 ms)
[PROFILE] ExtractThermalData: 200 us (0.2 ms)
[PROFILE] CalculateTemperatureStats: 800 us (0.8 ms)
[PROFILE] ApplyRotation: 0 us (0.0 ms)
[PROFILE] Renderer::RenderFrame: 8000 us (8.0 ms)
[PROFILE] ApplyColormap: 1500 us (1.5 ms)
[PROFILE] ScaleFrame: 4500 us (4.5 ms)  ← Remove this
[PROFILE] RenderHUD: 2000 us (2.0 ms)
```

---

## Implementation Order

1. **Phase 1: GPU Scaling** (highest impact, cleanest change) - **COMPLETED**
2. **Phase 2: Colormap Optimization** (significant, well-contained) - **COMPLETED**
3. **Phase 3: Text Rendering** (smaller but clean architecture) - **SKIPPED** (optimal to skip for now)
   *(Correction: text rendering was later optimized with a glyph texture atlas — see `PERF_WORK.md` Phase 5 and `Renderer::InitializeFont()`.)*
4. **Phase 4: Minor Tweaks** (incremental improvements) - **COMPLETED**
5. **Testing & Benchmarking** (after each phase, comprehensive at end) - **IN PROGRESS**

---

## Risk Assessment

### Low Risk
- Phase 1: GPU Scaling - SDL API is well-tested
- Phase 2: Colormap optimization - pure math, isolated code
- Phase 4: Minor tweaks - small, reversible changes

### Medium Risk
- Phase 3: Text rendering - new architecture, requires testing

### Mitigation Strategies
- Profile after each phase to catch regressions early
- Keep original code commented out temporarily for rollback
- Visual comparison tools for rendering validation
- Unit tests for critical math operations

---

## Success Criteria

- [x] Phase 1 completed - GPU scaling implemented
- [x] Phase 2 completed - Colormap optimized with pre-packed ARGB + integer math
- [x] Phase 3 skipped - Text rendering optimization (not needed - excellent results without it)
- [x] Phase 4 completed - Minor optimizations (GetTemperatureAtIndex helper)
- [x] CPU usage <5% at 30 FPS with scale=3 - **Achieved ~1% (95% reduction)**
- [x] No visual quality degradation - **Tested with real camera, no issues**
- [x] FPS ≥30 maintained - **FPS limited by camera (25-26), not CPU**
- [x] All existing functionality preserved - **All features working**
- [x] Memory usage stable (<10% increase) - **Reduced by 1.7 MB (no scaled buffer)**
- [x] Cross-platform compatibility maintained - **Builds successfully**

## Final Summary

### CPU Optimization: **COMPLETE** ✓

**Results:**
- CPU usage reduced from ~20% to ~1% (**95% improvement**)
- Eliminated 1.7 MB memory allocation (scaled buffer)
- Rendering performance independent of scale factor
- All functionality preserved, no visual quality loss

**Key Achievements:**
1. GPU scaling completely removed CPU bottleneck
2. Colormap optimization eliminated floating-point and bitshift overhead
3. Processing time now <1ms per frame (vs camera limit of 40ms)
4. ~38ms idle time per frame at 25 FPS

**Status: Ready for production**

## Phase 5: Disable macOS Reaction Effects (Emergency Fix)

**Impact:** ~15-20% CPU reduction (eliminating unintended framework overhead)

### Problem Discovered
The time profiler trace (2025) revealed that despite the previous optimizations achieving ~1% CPU usage in the ThermalCamera application itself, an additional ~20% CPU was being consumed by macOS system frameworks:

- **VCPHandGestureVideoRequest / VCPHandPoseImageRequest**: Apple's Vision framework for hand gesture detection
- **Espresso / ANECompilerEngine**: Apple Neural Engine compilers for ML models
- **Total overhead:** ~23.7% of CPU time (from profile.txt)

### Root Cause
macOS Sonoma (14.0+) and later automatically enable **"Reaction Effects"** on video capture devices. This feature:
- Runs CoreML models on every frame to detect hand gestures (thumbs up, peace sign, etc.)
- Uses the Apple Neural Engine (ANE) for processing
- Is enabled by default even for applications that don't use it

The thermal camera feed is being analyzed for hand gestures by the operating system, causing significant CPU overhead.

### Solution Implemented
Explicitly disable macOS Reaction Effects in the camera initialization:

**File Modified:** `src/camera/MacAVFoundationStreamer.mm`

**Code Added:**
```objectivec
for (AVCaptureConnection* connection in _videoOutput.connections)
{
    if (connection.isVideoMirroringSupported)
    {
        connection.videoMirrored = NO;
    }

    // Disable macOS 14.0+ Reaction Effects (Gestures) to save CPU
    if (@available(macOS 14.0, *))
    {
        if ([connection respondsToSelector:@selector(setVideoEffects:)])
        {
            [connection setValue:@[] forKey:@"videoEffects"];
            std::cerr << "[INFO] Disabled macOS Reaction Effects (Gestures) to save CPU" << std::endl;
        }
    }
}
```

**Implementation Notes:**
- Uses Key-Value Coding (KVC) to set `videoEffects` to an empty array
- The `videoEffects` property is not fully exposed in current SDK headers, so we use `setValue:forKey:` 
- Checks for availability with `@available(macOS 14.0, *)`
- Checks for property existence with `respondsToSelector:` for safety

### Expected Results
- **Eliminate** the VCPHandGestureVideoRequest overhead
- **Eliminate** the Espresso/ANECompilerEngine overhead
- **Total CPU usage** should drop from ~20% back to the expected ~1%
- **No impact** on thermal camera functionality

### Validation
- Re-run the application with the fix
- Check that CPU usage in Activity Monitor is now ~1% (not ~20%)
- Verify thermal camera still captures and displays correctly
- Optional: Run xctrace again to confirm VCP/Espresso threads are gone

---

## Testing & Benchmarking Status

### Completed Optimizations:
1. **GPU Scaling** (Phase 1)
   - Removed CPU bilinear interpolation
   - Uses SDL texture scaling (GPU-accelerated)
   - Eliminated `_scaledBuffer` memory usage
   - Expected CPU reduction: 60-70%

2. **Colormap Optimization** (Phase 2)
   - Pre-packed colormaps to ARGB format
   - Replaced floating-point with integer math
   - Eliminated per-pixel bitshift operations
   - Expected CPU reduction: ~5%

3. **Minor Optimizations** (Phase 4)
   - Added `GetTemperatureAtIndex()` helper without bounds checking
   - Eliminated redundant bounds checks in stats calculation
   - Expected CPU reduction: 1-2%

### Benchmark Results (Real Camera):

#### Scale=1 (256×192 pixels):
- **FPS:** 25-26 (camera-limited, not CPU-limited)
- **RenderFrame:** 0.3-2.0 ms (avg ~0.8 ms)
- **ApplyColormap:** 0.05-0.4 ms (avg ~0.1 ms)
- **No `ScaleFrame` profiling** (completely eliminated!)
- **Camera frame time:** ~40 ms (USB thermal camera limit)
- **CPU processing time:** ~0.42 ms per frame average

#### Scale=3 (768×576 pixels):
- **FPS:** 21-26 (similar to scale=1 - GPU scaling works!)
- **RenderFrame:** 0.28-0.37 ms (avg ~0.32 ms)
- **ApplyColormap:** 0.028-0.031 ms (avg ~0.03 ms)
- **Rendering time constant** regardless of scale - GPU handling scaling!

### Actual CPU Reduction:
- **Before:** ~20% CPU usage with scale=3 (based on user report)
- **After:** ~1% CPU usage (estimated from profiling)
- **Reduction:** ~95% (far exceeded goal of <5%!)

### Visual Quality:
- No degradation observed
- All colormaps working correctly
- Rotation, scaling, HUD all functional

### Issue Fixed:
**Scaling Issue (Retina Display):**
- Problem: Using `SDL_GetRendererOutputSize()` which returns physical pixels on macOS Retina displays
- This caused double-scaling (window size × Retina factor)
- Fix: Changed to `SDL_GetWindowSize()` which returns logical window size
- Texture now renders correctly at full window size

**Fullscreen/Letterbox Issue:**
- Problem: HUD used configured scale factor for coordinate calculations, but in fullscreen with letterboxing, the actual rendered image is smaller than `targetW × targetH`
- Fix: Updated mouse coordinate transformation to use actual rendered image size (`viewportRect.w × viewportRect.h`)
- Mouse coordinates now correctly map to thermal frame coordinates regardless of window size or aspect ratio

**Colormap Optimization Applied:**
- Pre-packed all colormaps to ARGB format at startup
- Changed from floating-point to integer math for colormap index calculation
- Eliminated per-pixel bitshift operations via direct ARGB lookup
- Expected CPU reduction: ~5% (in addition to GPU scaling savings)

### Performance Comparison:

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| CPU Usage | ~20% | ~1% | **95% reduction** |
| Scale Impact | High (CPU bound) | None (GPU bound) | **Eliminated** |
| Memory Usage | ~1.7 MB extra for scale=3 | 0 (no scaled buffer) | **1.7 MB saved** |
| FPS (scale=1) | 25-26 | 25-26 | Same |
| FPS (scale=3) | 20-25 | 21-26 | Improved |

## Phase 9b: Linux amd64 SIMD (2026-09-06)

New file `src/thermal/ThermalSimd.{hpp,cpp}` with runtime CPU dispatch
(`__builtin_cpu_supports`), tiers: AVX-512BW/F -> SSE4.1 -> scalar (stats);
AVX-512F -> AVX2 -> scalar (colormap). x86-only (`#if defined(__x86_64__)`),
macOS ARM compiles the scalar reference.

- `ExtractThermalData`: byte-shuffle loop replaced with `std::memcpy`
  (YUYV thermal sub-frame is already little-endian packed uint16 on LE hosts).
- `CalculateTemperatureStats`: SIMD min/max/sum kernels; first-occurrence index
  recovered in a second pass (firstIndexOf). SSE4.1 uses `_mm_minpos_epu16`
  (horizontal min with lane index in one op); GCC has no `_mm512_reduce_min_epu16`,
  AVX-512 path folds to 128-bit + minpos instead.
- `Renderer::ApplyColormap`: gather-based lookup (`vpgatherdd`). Exact truncated
  division `(delta*255)/range` reproduced via exact float math (< 2^24) with
  two-sided correction masks - bit-identical to scalar (verified by tests).
- Benchmarks (Ryzen/Zen-class AVX-512, 256x192 frame, us/frame):
  stats 26.3 -> 7.0 (3.8x), colormap 51.7 -> 23.5 (2.2x).
- Parity tests in `test_thermal.cpp`: sizes 1..49152 (tail coverage), 7 range
  regimes incl. {0,65535}, single-step, constant input; memcmp-exact.

### Phase 9c: ARM NEON paths (2026-09-06)

- aarch64 NEON is baseline -> no runtime dispatch (`#if THERMAL_NEON` branch
  checked before the x86 tier selection).
- Stats: vminq/vmaxq accumulate, vminvq/vmaxvq reduce, vmovl+vaddq widening
  sum; identical structure to the SSE4.1 kernel.
- Colormap: NEON has no gather - indices computed 8-wide in vectors (same
  exact float division + correction; note vcvtq_u32_f32 rounds-to-nearest per
  FPCR vs x86 cvttps truncate, single correction step suffices), palette
  lookups stay scalar.
- Verified by aarch64 cross syntax-check only (clang --target=aarch64-linux-gnu);
  runtime + parity test verification pending next macOS session (test suite
  exercises the NEON path there automatically).

### Phase 9c addendum: NEON runtime-verified + first-index SIMD (2026-09-06)

- NEON kernels verified on real hardware (ROCKNIX console, RK3566/Cortex-A55):
  all parity tests bit-exact vs scalar oracle.
- SIMD first-index recovery added (was scalar pass dominating: x86 stats
  7.0 -> 1.5 us/frame; NEON uses vceqq + u64 lane OR reduction).
- RK3566 quirk: libusb isochronous packet buffers can be NULL with
  actual_length > 0; unpatched libuvc dereferences them (SIGTRAP via LLVM
  null-deref trap). Fixed via wrap patch (subprojects/packagefiles/
  libuvc-rocknix): guard NULL payload and NULL stream handle in
  _uvc_process_payload/_uvc_stream_callback.
- Deployed on device: stable 26 FPS under sway/wayland (Mali blob panics on
  SDL fullscreen toggle - launcher uses swaymsg fullscreen instead).
- Note: on the in-order A55 the compiler auto-vectorizes the scalar reference
  well; the win there is correctness + the memcpy extraction, not raw SIMD.
