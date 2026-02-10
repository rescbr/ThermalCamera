# Phase 1: Foundation

## Overview

Establish the core project structure, build system, and basic camera connectivity.

## Objectives

1. Set up Meson build configuration
2. Create project directory structure
3. Implement UvcCamera C++ wrapper for libuvc
4. Implement basic frame capture and display test
5. Verify cross-platform build capability

## Tasks

### Task 1.1: Project Structure Setup

**Status**: Completed

**Description**: Create complete directory structure for the project.

**Steps**:
- [x] Create `src/` subdirectories:
  - [x] `camera/` - Camera capture code
  - [x] `thermal/` - Thermal data processing
  - [x] `render/` - SDL rendering
  - [x] `config/` - Configuration management
- [x] Verify `src/vendor/` contains:
  - [x] `cmdline/cmdline.h`
  - [x] `ctpl/ctpl_stl_tls.h`
- [x] Create `subprojects/` directory for Meson dependencies
- [x] Create `plans/` directory (completed)

**Validation**: Directory structure matches AGENTS.md specification

---

### Task 1.2: Meson Build Configuration

**Status**: Completed

**Description**: Create `meson.build` with all dependencies and source files.

**Steps**:
- [x] Create root `meson.build` file
- [x] Configure project with C++14 standard
- [x] Add SDL2 dependency (>= 2.0.0)
- [x] Add libuvc dependency (>= 0.0.6)
- [x] Add threads dependency
- [x] Define source file lists
- [x] Create executable target with proper dependencies
- [x] Configure installation target
- [x] Add subproject directories for SDL2 and libuvc

**Implementation Notes**:
```python
project('thermalcamera', 'cpp',
    default_options: ['warning_level=3'],
    cpp_std: 'c++14')

# Dependencies
sdl2_dep = dependency('sdl2', version: '>=2.0.0', required: false)
uvc_dep = dependency('libuvc', version: '>=0.0.6', required: false)
thread_dep = dependency('threads')

# Use subprojects if system dependencies unavailable
if not sdl2_dep.found()
    sdl2_sub = subproject('sdl2')
    sdl2_dep = sdl2_sub.get_variable('sdl2_dep')
endif

if not uvc_dep.found()
    uvc_sub = subproject('libuvc')
    uvc_dep = uvc_sub.get_variable('uvc_dep')
endif
```

**Validation**: `meson setup builddir` succeeds without errors

---

### Task 1.3: UvcCamera Header

**Status**: Completed

**Description**: Create C++ RAII wrapper for libuvc camera management.

**File**: `src/camera/UvcCamera.hpp`

**Steps**:
- [x] Define `UvcCamera` class with Allman brace style
- [x] Implement RAII constructor/destructor
- [x] Declare public API:
  - [x] `bool Initialize(const std::string& devicePath)`
  - [x] `bool StartStreaming()`
  - [x] `void StopStreaming()`
  - [x] `bool GetFrame(uint8_t*& frameData, size_t& frameSize)`
  - [x] `bool IsRunning() const`
- [x] Declare private members:
  - [x] `uvc_context_t* _context`
  - [x] `uvc_device_t* _device`
  - [x] `uvc_device_handle_t* _deviceHandle`
  - [x] `uvc_stream_handle_t* _streamHandle`
  - [x] `std::unique_ptr<uint8_t[]> _frameBuffer`
  - [x] `std::mutex _frameMutex`
  - [x] `std::atomic<bool> _isRunning`
- [x] Follow naming conventions: `_camelCase` for member variables
- [x] Use `std::unique_ptr` for buffer ownership

**Implementation Notes**:
- libuvc is C library, need C++ wrapper
- Handle both frame buffers (double buffering)
- Use atomic bool for thread-safe state

**Validation**: Header compiles with proper includes

---

### Task 1.4: UvcCamera Implementation

**Status**: Completed

**Description**: Implement UvcCamera wrapper with libuvc operations.

**File**: `src/camera/UvcCamera.cpp`

**Steps**:
- [x] Implement `Initialize()`:
  - [x] Initialize uvc_context
  - [x] Find device by path
  - [x] Open device
  - [x] Set video format (YUYV 4:2:2, 256x384, 25 FPS)
- [x] Implement `StartStreaming()`:
  - [x] Start video stream
  - [x] Set frame callback
  - [x] Allocate frame buffers
- [x] Implement `StopStreaming()`:
  - [x] Stop video stream
  - [x] Cleanup resources
- [x] Implement `GetFrame()`:
  - [x] Lock mutex for thread safety
  - [x] Return pointer to current frame
  - [x] Return frame size
- [x] Implement RAII destructor:
  - [x] Stop streaming if running
  - [x] Close device
  - [x] Cleanup context
- [x] Handle error cases gracefully
- [x] Log errors to `std::cerr`

**Reference Format**:
- Width: 256
- Height: 384
- Format: YUYV 4:2:2 (UVC pixel format)
- FPS: 25

**Validation**: Camera can be opened and started successfully

---

### Task 1.5: Main Entry Point

**Status**: Completed

**Description**: Create minimal main.cpp to test camera capture.

**File**: `src/main.cpp`

**Steps**:
- [x] Include necessary headers:
  - [x] `camera/UvcCamera.hpp`
  - [x] `cmdline/cmdline.h`
  - [x] iostream for logging
- [x] Parse command line arguments:
  - [x] `-d` / `-device`: Camera device path (default /dev/video0)
  - [x] `-help`: Show usage and exit
- [x] Validate arguments with cmdline parser
- [x] Create UvcCamera instance
- [x] Initialize camera with device path
- [x] Start streaming
- [x] Capture a few test frames
- [x] Stop streaming
- [x] Log success/failure to `std::cerr`

**Validation**: Program runs, opens camera, and exits cleanly

---

### Task 1.6: Build and Test

**Status**: Completed

**Description**: Verify the build system works and basic functionality.

**Steps**:
- [x] Run `meson setup builddir`
- [x] Run `meson compile -C builddir`
- [x] Fix any compilation errors
- [x] Test with connected thermal camera
- [x] Verify frame capture (log frame size)
- [x] Test without camera (error handling)
- [x] Test on Linux (primary target)
- [x] Verify macOS compatibility if available

**Validation**:
- Build succeeds on Linux
- Can open thermal camera device
- Frame size is correct (196608 bytes = 384 × 256 × 2)
- No memory leaks detected (valgrind)

---

## Dependencies

- libuvc >= 0.0.6
- libusb-1.0
- C++14 compiler
- Meson >= 0.56.0
- Ninja build tool

## Success Criteria

Phase 1 is complete when:
- [x] Project structure is created per AGENTS.md
- [x] Meson builds executable successfully
- [x] UvcCamera can open and start thermal camera
- [x] Frames are captured at correct size and format
- [x] Clean shutdown on exit
- [x] Code follows CODING_GUIDELINES.md

## Next Phase

After Phase 1 completion, proceed to **Phase 2: Thermal Processing**.
