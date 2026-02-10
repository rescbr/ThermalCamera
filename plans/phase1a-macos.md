# Phase 1a: macOS Support

## Overview

Implement native macOS camera support using AVFoundation since libuvc has limited support/stability on macOS.

## Objectives

1. Create abstract `ICamera` interface
2. Implement `MacCamera` using AVFoundation (Objective-C++)
3. Refactor `UvcCamera` to implement `ICamera`
4. Update build system to support both platforms
5. Verify frame capture on macOS

## Tasks

### Task 1a.1: Abstract Camera Interface

**Status**: Completed

**Description**: Define a common interface for camera implementations.

**File**: `src/camera/ICamera.hpp`

**Steps**:
- [x] Define abstract base class `ICamera`
- [x] Pure virtual methods:
  - [x] `Initialize(const std::string& devicePath)`
  - [x] `StartStreaming()`
  - [x] `StopStreaming()`
  - [x] `GetFrame(uint8_t*& frameData, size_t& frameSize)`
  - [x] `IsRunning() const`
- [x] Virtual destructor

**Validation**: Interface compiles

---

### Task 1a.2: AVFoundation Implementation

**Status**: Completed

**Description**: Implement macOS camera capture using AVFoundation.

**File**: `src/camera/MacCamera.mm` (Objective-C++)

**Steps**:
- [x] Create `MacCamera` class inheriting `ICamera`
- [x] Use `AVCaptureSession` for streaming
- [x] Implement `AVCaptureVideoDataOutputSampleBufferDelegate`
- [x] Configure session:
  - [x] Device discovery (AVCaptureDevice)
  - [x] Preset: 384x256 (if possible) or closest match
  - [x] Pixel format: kCVPixelFormatType_422YpCbCr8 (2vuy/YUY2)
- [x] Implement callback:
  - [x] Extract `CVImageBuffer`
  - [x] Copy data to internal buffer
  - [x] Handle thread safety
- [x] Manage memory (ARC or manual retain/release)

**Implementation Notes**:
- Needs `.mm` extension for Objective-C++
- Link against `AVFoundation`, `CoreMedia`, `CoreVideo` frameworks

**Validation**: Successfully captures frames on macOS

---

### Task 1a.3: Platform Factory

**Status**: Completed

**Description**: specific camera implementation based on OS.

**File**: `src/main.cpp`

**Steps**:
- [x] Add preprocessor directives:
  - [x] `#ifdef __APPLE__` -> Use `MacCamera`
  - [x] `#else` -> Use `UvcCamera`
- [x] Update `main.cpp` to use `std::unique_ptr<ICamera>`
- [x] Instantiate correct class

**Validation**: Builds on both macOS and Linux

---

### Task 1a.4: Build System Update

**Status**: Completed

**Description**: Configure Meson to handle platform differences.

**File**: `meson.build`

**Steps**:
- [x] Detect host system (`host_machine.system()`)
- [x] Add macOS-specific sources (`MacCamera.mm`)
- [x] Add macOS frameworks dependency (`dependency('appleframeworks', modules: ['AVFoundation', 'CoreMedia', 'CoreVideo'])`)
- [x] Conditional compilation for source files

**Validation**: `meson setup` configures correctly for macOS

---

## Dependencies

- Xcode command line tools (clang)
- AVFoundation framework

## Success Criteria

Phase 1a is complete when:
- [x] `ICamera` interface is defined
- [x] `MacCamera` captures frames on macOS
- [x] Build system supports both Linux and macOS
- [x] Application runs on macOS with thermal camera

## Next Phase

Proceed to **Phase 2: Thermal Processing** (common to both platforms).
