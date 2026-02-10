# Phase 5: Interaction

## Overview

Implement user interaction features including keyboard controls, mouse input, and CLI argument parsing.

## Objectives

1. Complete CLI argument parsing with cmdline library
2. Implement keyboard bindings for all core features
3. Add mouse interaction for temperature probing
4. Implement configuration management
5. Add freeze frame mode

## Tasks

### Task 5.1: Enhanced CLI Parsing

**Status**: Completed

**Description**: Complete command-line argument parsing for all features.

**File**: `src/main.cpp`

**Steps**:
- [ ] Extend cmdline parser with all options:
  - [ ] `-d` / `-device`: Camera device path (default /dev/video0)
  - [ ] `-f` / `-file`: Offline raw file input
  - [ ] `-scale`: Initial scale factor (default 1X)
  - [ ] `-fullscreen`: Start in fullscreen mode
  - [ ] `-colormap`: Initial colormap index (default 0)
  - [ ] `-celsius`: Use Celsius units (default Fahrenheit)
  - [ ] `-help`: Show usage and exit
  - [ ] `-quiet`: Suppress verbose output
  - [ ] `-threads`: Override thread count (default 3)
- [ ] Implement argument validation:
  - [ ] Check for valid device paths
  - [ ] Validate scale factor range (1-10)
  - [ ] Validate colormap index (0-36)
  - [ ] Show error for invalid values
- [ ] Implement help text:
  - [ ] Usage examples
  - [ ] All options with descriptions
  - [ ] Default values
- [ ] Implement quiet mode:
  - [ ] Only log errors
  - [ ] Suppress startup messages

**CLI Examples**:
```bash
# Live camera with defaults
./thermalcamera

# Specific device with 2X scale
./thermalcamera -d /dev/video1 -scale 2

# Offline mode with custom colormap
./thermalcamera -f recording.raw -colormap 4 -celsius
```

**Validation**: All CLI options parse correctly

---

### Task 5.2: Configuration Structure

**Status**: Completed

**Description**: Create centralized configuration structure and management.

**File**: `src/config/Config.hpp`

**Steps**:
- [ ] Define `Config` class:
  - [ ] Public getter/setter methods
  - [ ] Private member variables
  - [ ] Default values
- [ ] Define configuration members:
  - [ ] `std::string _devicePath` - Camera device
  - [ ] `std::string _inputFile` - Offline file
  - [ ] `int _scaleFactor` - Display scale (1-10)
  - [ ] `bool _fullscreen` - Fullscreen mode
  - [ ] `int _colormapIndex` - Current colormap (0-36)
  - [ ] `bool _useCelsius` - Temperature unit
  - [ ] `int _rotation` - Display rotation (0-3)
  - [ ] `bool _freezeFrame` - Freeze frame mode
  - [ ] `int _threadCount` - Worker threads
  - [ ] `bool _verbose` - Logging level
- [ ] Implement methods:
  - [ ] `void ApplyCLIArguments(const cmdline::parser&)`
  - [ ] `void SetDefaults()`
  - [ ] `void LogCurrentConfig() const`
- [ ] Add Allman brace style
- [ ] Follow naming conventions

**Config Usage**:
```cpp
Config config;
config.SetDefaults();
config.ApplyCLIArguments(cmd);

if (config._freezeFrame) {
    // Enter freeze mode
}
```

**Validation**: Config manages all application state correctly

---

### Task 5.3: Keyboard Input Processing

**Status**: Completed

**Description**: Implement comprehensive keyboard input handling.

**File**: `src/render/Renderer.cpp` and update `Renderer.hpp`

**Steps**:
- [ ] Extend `HandleEvents()` method:
  - [ ] Process all SDL_KEYDOWN events
  - [ ] Map keys to actions
  - [ ] Handle key repeats
- [ ] Implement key bindings:
  - [ ] `Q` / `ESC`: Quit application
  - [ ] `F` / `F11`: Toggle fullscreen
  - [ ] `+` / `-`: Increase/decrease scale
  - [ ] `[` / `]`: Rotate display (90°)
  - [ ] `C` / `F`: Toggle Celsius/Fahrenheit
  - [ ] `M` / `,`: Cycle colormaps forward
  - [ ] `N` / `.`: Cycle colormaps backward
  - [ ] `SPACE`: Toggle freeze frame
  - [ ] `R`: Reset to defaults
  - [ ] `H`: Toggle HUD visibility
- [ ] Add debouncing:
  - [ ] Prevent rapid key repeats
  - [ ] Track key press times
- [ ] Log key actions:
  - [ ] Debug logging for key presses
  - [ ] State changes

**Key Binding Table**:
| Key | Action |
|-----|--------|
| ESC / Q | Quit |
| F / F11 | Fullscreen |
| + / - | Scale + / - |
| [ / ] | Rotate 90° |
| C | Toggle C/F |
| M / N | Prev/Next colormap |
| SPACE | Freeze/Resume |
| R | Reset defaults |

**Validation**: All key bindings respond correctly

---

### Task 5.4: Mouse Input Processing

**Status**: Completed

**Description**: Implement mouse interaction for temperature probing.

**File**: `src/render/Renderer.cpp`

**Steps**:
- [ ] Extend `HandleEvents()` for mouse:
  - [ ] Process SDL_MOUSEMOTION:
    - [ ] Track cursor position
    - [ ] Map to frame coordinates
    - [ ] Query temperature at position
  - [ ] Process SDL_MOUSEBUTTONDOWN:
    - [ ] Left click: Show temperature tooltip
    - [ ] Right click: Toggle probe mode
  - [ ] Process SDL_MOUSEBUTTONUP:
    - [ ] Release tooltips
- [ ] Implement temperature probe:
  - [ ] Get mouse position
  - [ ] Calculate frame coordinates
  - [ ] Call ThermalProcessor.GetTemperatureAt()
  - [ ] Display tooltip with temperature
  - [ ] Show coordinates
- [ ] Implement tooltip rendering:
  - [ ] Draw box near cursor
  - [ ] Render temperature text
  - [ ] Fade in/out animation
  - [ ] Handle screen edges
- [ ] Scale coordinates:
  - [ ] Account for scale factor
  - [ ] Account for window position
  - [ ] Handle rotation

**Mouse Interaction Logic**:
```cpp
void OnMouseMove(int x, int y) {
    int frameX = x / scaleFactor;
    int frameY = y / scaleFactor;
    Temperature temp = thermalProcessor.GetTemperatureAt(
        thermalFrame, frameY, frameX);
    ShowTooltip(x, y, temp);
}
```

**Validation**: Mouse probes show accurate temperatures

---

### Task 5.5: Freeze Frame Mode

**Status**: Completed

**Description**: Implement freeze frame functionality for capturing moments.

**File**: `src/main.cpp` and `src/config/Config.hpp`

**Steps**:
- [ ] Add freeze frame state to Config:
  - [ ] `bool _freezeFrame` - Is frame frozen
  - [ ] `std::unique_ptr<uint8_t[]> _frozenFrame` - Saved frame data
- [ ] Implement freeze logic:
  - [ ] On SPACE key: toggle freeze
  - [ ] Save current frame when freezing
  - [ ] Stop capture thread processing
  - [ ] Keep rendering frozen frame
  - [ ] Update HUD with "FROZEN" indicator
- [ ] Implement resume logic:
  - [ ] On SPACE key: resume capture
  - [ ] Restart capture thread processing
  - [ ] Clear frozen frame buffer
  - [ ] Remove HUD indicator
- [ ] Handle frame updates:
  - [ ] Continue event processing
  - [ ] Continue rendering (static frame)
  - [ ] Continue mouse probing

**Freeze Frame HUD**:
```
┌─────────────────────┐
│ FPS: --  [FROZEN] │
│ Min: 20.3°C         │
│ Max: 45.2°C         │
│      Press SPACE        │
│      to resume        │
└─────────────────────┘
```

**Validation**: Freeze frame captures and resumes correctly

---

### Task 5.6: Colormap Cycling

**Status**: Completed

**Description**: Implement smooth colormap switching with user feedback.

**File**: `src/render/Renderer.cpp` and `src/config/Config.hpp`

**Steps**:
- [ ] Implement colormap cycling:
  - [ ] Increment/Decrement index
  - [ ] Wrap around at array bounds
  - [ ] Update Config._colormapIndex
  - [ ] Log new colormap name
- [ ] Update colormap list:
  - [ ] Get count from colormaps.hpp
  - [ ] Implement forward/backward cycle
  - [ ] Skip "None" colormap if desired
- [ ] Add user feedback:
  - [ ] Show colormap name in HUD
  - [ ] Fade effect on transition
  - [ ] Cache new colormap
- [ ] Support rapid cycling:
  - [ ] Pre-load colormaps
  - [ ] Handle key repeats
  - [ ] Smooth transitions

**Colormap Names** (from colormaps.hpp):
- Magma, Inferno, Plasma, Viridis, etc.
- Jet, Hot, Cool, Rainbow
- Autumn, Spring, Winter, Ocean

**Validation**: Colormap switching works smoothly

---

### Task 5.7: Scale Factor Control

**Status**: Completed

**Description**: Implement dynamic scaling with bounds checking.

**File**: `src/render/Renderer.cpp` and `src/config/Config.hpp`

**Steps**:
- [ ] Implement scale adjustment:
  - [ ] Increment/Decrement scale factor
  - [ ] Clamp to valid range (1-10 or fullscreen)
  - [ ] Update Config._scaleFactor
  - [ ] Recalculate window size
- [ ] Handle fullscreen mode:
  - [ ] Toggle fullscreen independently
  - [ ] Remember scale factor
  - [ ] Restore on exit
  - [ ] Recalculate display size
- [ ] Update rendering:
  - [ ] Resize SDL window/texture
  - [ ] Recalculate scaling factors
  - [ ] Update font sizes
  - [ ] Adjust HUD layout
- [ ] Add scale indicators:
  - [ ] Show current scale in HUD
  - [ ] Update on change
  - [ ] Show "MAX" at full screen

**Scale Factors**:
- 1X: Native (256 × 192)
- 2X: (512 × 384)
- 3X: (768 × 576)
- 4X: (1024 × 768)
- Fullscreen: Fit to display

**Validation**: Scaling adjusts smoothly and stays in bounds

---

### Task 5.8: Rotation Support

**Status**: Completed

**Description**: Implement display rotation in 90° increments.

**File**: `src/render/Renderer.cpp` and `src/thermal/ThermalProcessor.cpp`

**Steps**:
- [ ] Add rotation state to Config:
  - [ ] `int _rotation` - 0, 90, 180, 270 degrees
- [ ] Implement rotation logic:
  - [ ] Map 0-3 to rotation values
  - [ ] Increment on `[` key
  - [ ] Decrement on `]` key
  - [ ] Wrap around (modulo 4)
- [ ] Implement frame rotation:
  - [ ] Rotate thermal data before processing
  - [ ] Swap width/height for 90/270
  - [ ] Adjust coordinate systems
  - [ ] Update mouse mapping
- [ ] Adjust dimensions:
  - [ ] 0°, 180°: 256 × 192
  - [ ] 90°, 270°: 192 × 256
  - [ ] Update texture sizes
  - [ ] Recalculate scale factors

**Rotation Implementation**:
```cpp
void ApplyRotation(ThermalFrame& frame) {
    switch (config._rotation) {
        case 0: break;  // No rotation
        case 1: Rotate90(frame); break;
        case 2: Rotate180(frame); break;
        case 3: Rotate270(frame); break;
    }
}
```

**Validation**: Rotation works correctly in all 4 orientations

---

### Task 5.9: HUD Enhancements

**Status**: Completed

**Description**: Enhance HUD with complete configuration information.

**File**: `src/render/Renderer.cpp`

**Steps**:
- [ ] Expand HUD layout:
  - [ ] Top-left: FPS, Min/Max/Avg temps
  - [ ] Top-right: Colormap name, Scale factor
  - [ ] Bottom-left: Temperature unit, Rotation
  - [ ] Bottom-right: Controls hint
  - [ ] Center: Crosshair with center temp
- [ ] Add status indicators:
  - [ ] Freeze frame indicator
  - [ ] Camera connection status
  - [ ] Offline mode indicator
- [ ] Implement dynamic layout:
  - [ ] Adjust for different scale factors
  - [ ] Handle rotation
  - [ ] Prevent text overlap
  - [ ] Background for readability
- [ ] Add controls hints:
  - [ ] Show key bindings
  - [ ] Toggle with `H` key
  - [ ] Fade in/out
- [ ] Optimize rendering:
  - [ ] Cache static text
  - [ ] Only update changed values
  - [ ] Use efficient font rendering

**HUD Layout Example**:
```
┌─────────────────────────────────────┐
│ FPS: 25  Min: 18.2°C   Jet 2X   │
│ Max: 42.5°C Avg: 30.1°C  (0°)   │
│                                     [C] FROZEN│
│                           ┼                    │
│     [H] for controls                       │
└─────────────────────────────────────┘
```

**Validation**: HUD displays all necessary information

---

### Task 5.10: Integration Testing

**Status**: Completed

**Description**: Test all interaction features end-to-end.

**File**: Test script or manual testing

**Steps**:
- [ ] Test CLI parsing:
  - [ ] All valid combinations
  - [ ] Invalid arguments
  - [ ] Help output
- [ ] Test keyboard controls:
  - [ ] All key bindings
  - [ ] Key repeat handling
  - [ ] Combinations (fullscreen + quit)
- [ ] Test mouse interaction:
  - [ ] Temperature probing
  - [ ] Different scale factors
  - [ ] Screen edges
  - [ ] Rotation
- [ ] Test freeze frame:
  - [ ] Freeze and resume
  - [ ] Mouse probing while frozen
  - [ ] Rotation while frozen
- [ ] Test offline mode:
  - [ ] Load raw file
  - [ ] All interactions
  - [ ] File loop
- [ ] Usability testing:
  - [ ] Responsiveness
  - [ ] Intuitive controls
  - [ ] Clear feedback

**Test Scenarios**:
1. Normal operation with camera
2. Offline mode with raw file
3. Freeze frame usage
4. Rapid key presses
5. All scale factors
6. All rotations
7. All colormaps

**Validation**: All interaction features work reliably

---

## Dependencies

- Phase 4 threading (ctpl)
- Phase 3 renderer (SDL)
- Phase 2 thermal processor
- Phase 1 camera (UvcCamera)
- cmdline library (vendor/cmdline)
- Phase 2 Config structure

## Success Criteria

Phase 5 is complete when:
- [x] All CLI options parse correctly
- [x] All keyboard bindings work reliably
- [x] Mouse probes show accurate temperatures
- [x] Freeze frame captures and resumes
- [x] Colormap switching works smoothly
- [x] Scale factors adjust correctly
- [x] Rotation works in all 4 orientations
- [x] HUD displays complete information
- [x] Offline mode works with raw files
- [x] Code follows CODING_GUIDELINES.md

## Next Phase

After Phase 5 completion, proceed to **Phase 6: Polish**.
