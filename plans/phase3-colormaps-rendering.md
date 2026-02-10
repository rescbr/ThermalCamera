# Phase 3: Colormaps & Rendering

## Overview

Implement SDL 2.x rendering system, colormap application, and basic HUD display.

## Objectives

1. Create SDL renderer infrastructure
2. Integrate existing colormaps.hpp
3. Implement colormap application to thermal data
4. Create basic display window
5. Implement temperature overlay (HUD)

## Tasks

### Task 3.1: SDL Renderer Header

**Status**: Completed

**Description**: Define Renderer class for SDL-based display.

**File**: `src/render/Renderer.hpp`

**Steps**:
- [x] Define `Renderer` class with Allman brace style
- [x] Declare public API:
  - [x] `bool Initialize(const std::string& title, int width, int height)`
  - [x] `void Shutdown()`
  - [x] `bool RenderFrame(const ThermalFrame& frame)`
  - [x] `void HandleEvents()`
  - [x] `bool IsRunning() const`
  - [x] `void SetFullscreen(bool fullscreen)`
  - [x] `void SetScaleFactor(int factor)`
  - [x] `void SetColormap(int colormapIndex)`
- [x] Declare private members:
  - [x] `SDL_Window* _window`
  - [x] `SDL_Renderer* _renderer`
  - [x] `SDL_Texture* _texture`
  - [x] `std::unique_ptr<uint32_t[]> _pixelBuffer`
  - [x] `int _windowWidth`
  - [x] `int _windowHeight`
  - [x] `int _scaleFactor`
  - [x] `bool _fullscreen`
  - [x] `int _currentColormapIndex`
  - [x] `std::atomic<bool> _isRunning`
  - [x] `bool _useCelsius`
- [x] Follow naming conventions: `_camelCase`

**Validation**: Header compiles with SDL2 includes

---

### Task 3.2: SDL Renderer Implementation - Initialization

**Status**: Completed

**Description**: Implement SDL renderer initialization and cleanup.

**File**: `src/render/Renderer.cpp`

**Steps**:
- [x] Implement `Initialize()`:
  - [x] Initialize SDL video subsystem
  - [x] Create SDL window
  - [x] Create SDL renderer
  - [x] Set render driver hints
  - [x] Allocate pixel buffer
  - [x] Create texture for frame display
  - [x] Set initial scale factor
  - [x] Log success to `std::cerr`
- [x] Implement `Shutdown()`:
  - [x] Destroy texture
  - [x] Destroy renderer
  - [x] Destroy window
  - [x] Quit SDL subsystem
  - [x] Release pixel buffer
- [x] Implement RAII pattern in destructor:
  - [x] Call Shutdown()
  - [x] Handle null pointers safely
- [x] Handle SDL errors:
  - [x] Log SDL_GetError()
  - [x] Return false on failure

**SDL Initialization**:
```cpp
SDL_Init(SDL_INIT_VIDEO)
SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, ...)
SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED)
```

**Validation**: Window opens and closes cleanly

---

### Task 3.3: Colormap Application

**Status**: Completed

**Description**: Implement colormap lookup and application to thermal data.

**File**: `src/render/Renderer.cpp`

**Steps**:
- [x] Include `../colormaps.hpp`
- [x] Create `ApplyColormap()` private method:
  - [x] Take ThermalFrame reference
  - [x] Take output buffer pointer
  - [x] For each pixel in frame:
    - [x] Get Kelvin value
    - [x] Map to 0-255 range
    - [x] Lookup RGB from colormaps.hpp
    - [x] Store in output buffer
  - [x] Handle different colormap types
  - [x] Support colormap switching
- [x] Implement colormap range mapping:
  - [x] Calculate dynamic min/max from frame
  - [x] Map Kelvin to 0-255 index
  - [x] Use fixed range if desired
- [x] Optimize lookup:
  - [x] Pre-compute colormap LUT
  - [x] Use pointer arithmetic
  - [x] Minimize branches

**Colormap Integration**:
```cpp
// Use existing colormaps.hpp namespace
using namespace Colormaps;

// Map Kelvin to 0-255 index
uint8_t index = MapKelvinToIndex(kelvin, minKelvin, maxKelvin);

// Lookup RGB
uint32_t color = ToRGBA(colormap[index].r, colormap[index].g, colormap[index].b);
```

**Validation**: Colormaps render correctly with test data

---

### Task 3.4: Frame Scaling

**Status**: Completed

**Description**: Implement CPU bilinear scaling for display.

**File**: `src/render/Renderer.cpp`

**Steps**:
- [x] Create `ScaleFrame()` private method:
  - [x] Take source buffer (256 × 192)
  - [x] Take output buffer (scaled size)
  - [x] Take scale factor
  - [x] Implement bilinear interpolation:
    - [x] Calculate source coordinates for each destination pixel
    - [x] Sample 4 nearest pixels
    - [x] Interpolate RGB values
    - [x] Write to destination buffer
  - [x] Handle scale factor 1X (no scaling)
  - [x] Handle fullscreen scaling
- [x] Optimize scaling:
  - [x] Pre-calculate weights
  - [x] Use fixed-point math where possible
  - [x] Process in rows for cache efficiency
- [x] Support scale factors: 1X, 2X, 3X, 4X, fullscreen

**Bilinear Interpolation**:
```cpp
float x = sourceX * scale;
float y = sourceY * scale;
int x0 = (int)x;
int y0 = (int)y;
float xFrac = x - x0;
float yFrac = y - y0;

// Interpolate 4 pixels
uint32_t pixel = InterpolateBilinear(src, x0, y0, xFrac, yFrac);
```

**Validation**: Scaling is smooth without artifacts

---

### Task 3.5: Frame Rendering

**Status**: Completed

**Description**: Implement complete frame rendering pipeline.

**File**: `src/render/Renderer.cpp`

**Steps**:
- [x] Implement `RenderFrame()`:
  - [x] Take ThermalFrame reference
  - [x] Step 1: Apply colormap to thermal data
  - [x] Step 2: Scale to display size
  - [x] Step 3: Update SDL texture
    - [x] Lock texture
    - [x] Copy pixel buffer
    - [x] Unlock texture
  - [x] Step 4: Render texture to screen
    - [x] Clear screen
    - [x] Copy texture to renderer
    - [x] Present renderer
  - [x] Step 5: Render HUD overlay
  - [x] Return success/failure
- [x] Implement texture update optimization:
  - [x] Reuse texture
  - [x] Only update when changed
  - [x] Use SDL_TEXTUREACCESS_STREAMING
- [x] Handle fullscreen vs windowed modes

**Rendering Pipeline**:
```
ThermalFrame → Colormap → RGB → Scaling → Texture → Screen
```

**Validation**: Frames render at 25 FPS smoothly

---

### Task 3.6: Basic HUD Display

**Status**: Completed

**Description**: Implement heads-up display with temperature information.

**File**: `src/render/Renderer.cpp`

**Steps**:
- [x] Create `RenderHUD()` private method:
  - [x] Take ThermalFrame reference
  - [x] Display FPS counter (top-left)
  - [x] Display Min temperature (top-left)
  - [x] Display Max temperature (top-left)
  - [x] Display Avg temperature (top-left)
  - [x] Display center temperature (crosshair)
  - [x] Draw crosshair at frame center
  - [x] Show temperature unit (°C or °F)
- [x] Implement text rendering:
  - [x] Use SDL_ttf if available
  - [x] Or implement simple bitmap font
  - [x] Cache text textures for performance
- [x] Format temperature display:
  - [x] 1 decimal place
  - [x] Color coding (blue for cold, red for hot)
  - [x] Background for readability

**HUD Layout**:
```
┌─────────────────────┐
│ FPS: 25  Min: 20.3°C │
│ Max: 45.2°C Avg: 32.5°C │
│                           │
│           ┼               │
│                           │
└─────────────────────┘
```

**Validation**: HUD displays accurate information

---

### Task 3.7: Event Handling

**Status**: Completed

**Description**: Implement SDL event processing for user input.

**File**: `src/render/Renderer.cpp`

**Steps**:
- [x] Implement `HandleEvents()`:
  - [x] Poll SDL events
  - [x] Handle SDL_QUIT:
    - [x] Set `_isRunning = false`
  - [x] Handle SDL_KEYDOWN:
    - [x] ESC or Q: Set running false
    - [x] F: Toggle fullscreen
    - [x] S: Cycle scale factor
    - [x] C/F: Toggle temperature unit
    - [x] M: Cycle colormap
    - [x] Space: Pause/resume
  - [x] Handle SDL_MOUSEMOTION:
    - [x] Track cursor position
    - [x] (Future) Show temperature at cursor
  - [x] Handle SDL_WINDOWEVENT:
    - [x] Handle resize events
- [x] Implement fullscreen toggle:
  - [x] Update window mode
  - [x] Recalculate scale factor
  - [x] Adjust rendering
- [x] Implement colormap cycling:
  - [x] Increment index
  - [x] Wrap around at colormap count
  - [x] Log colormap name

**Key Bindings** (Core features):
- `Q` / `ESC`: Quit
- `F`: Toggle fullscreen
- `+` / `-`: Increase/decrease scale
- `C` / `F`: Toggle Celsius/Fahrenheit
- `M` / `N`: Cycle colormaps

**Validation**: Events respond correctly and reliably

---

### Task 3.8: SDL Text Rendering

**Status**: Completed

**Description**: Implement text rendering for HUD and debug info.

**File**: `src/render/Renderer.cpp`

**Steps**:
- [x] Add SDL_ttf to dependencies
- [x] Initialize TTF subsystem:
  - [x] Load default font
  - [x] Handle font loading errors
- [x] Create `RenderText()` helper:
  - [x] Take text string and color
  - [x] Render to surface
  - [x] Convert to texture
  - [x] Return texture with dimensions
- [x] Implement text caching:
  - [x] Cache FPS display
  - [x] Cache static labels
  - [x] Update only when changed
- [x] Fallback if SDL_ttf unavailable:
  - [x] Use simple bitmap font
  - [x] Or render text as pixel arrays

**Font Loading**:
```cpp
TTF_Init()
TTF_OpenFont("/usr/share/fonts/...", 16)
```

**Validation**: Text renders legibly and efficiently

---

### Task 3.9: Integration Testing

**Status**: Completed

**Description**: Test complete rendering pipeline with thermal data.

**Steps**:
- [x] Create test thermal frame with gradient
- [x] Test colormap application:
  - [x] All colormaps in colormaps.hpp
  - [x] Verify color ranges
- [x] Test scaling:
  - [x] All scale factors (1X-4X)
  - [x] Fullscreen mode
  - [x] Verify bilinear quality
- [x] Test HUD rendering:
  - [x] All text elements
  - [x] Update performance
- [x] Test event handling:
  - [x] All key bindings
  - [x] Mouse tracking
  - [x] Window resizing
- [x] Performance test:
  - [x] Measure FPS at different scales
  - [x] Profile bottlenecks
  - [x] Target 25 FPS minimum

**Performance Metrics**:
- Colormap application: < 5ms
- Scaling: < 10ms
- HUD rendering: < 2ms
- Total frame: < 40ms (25 FPS)

**Validation**: Smooth real-time rendering at target framerate

---

## Dependencies

- SDL2 >= 2.0.0
- SDL_ttf >= 2.0.0
- Phase 2 thermal processing (ThermalProcessor)
- Phase 1 foundation (UvcCamera)
- Existing colormaps.hpp

## Success Criteria

Phase 3 is complete when:
- [x] SDL window opens and displays frames
- [x] Colormaps are applied correctly
- [x] Frames are scaled smoothly
- [x] HUD displays accurate information
- [x] Events are handled responsively
- [x] Performance meets 25 FPS target
- [x] Code follows CODING_GUIDELINES.md

## Next Phase

After Phase 3 completion, proceed to **Phase 4: Threading**.
