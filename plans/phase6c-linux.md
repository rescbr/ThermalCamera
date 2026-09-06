# Phase 6c: Linux-Specific Polish

> **ERRATUM (documentation audit 2026-09-06)**: Several items below are marked completed but are
> **not reflected in the code**. Corrections:
> - Task 6c.1: `LibuvcError`/`UsbError` error types were never defined; errors use plain
>   `std::string` messages (see `src/camera/UvcCameraProvider.cpp`).
> - Task 6c.4: SIMD optimizations were not applied; only loop unrolling (stride 8) exists in
>   `src/thermal/ThermalProcessor.cpp`.
> - Task 6c.6: ASAN/TSAN/-pg build options, .deb/.rpm packaging metadata, cpack support, install
>   and udev rules do **not** exist in `meson.build` (only version + test targets are present).
> - The `-std=c++14` checklist item is stale; the build uses `cpp_std=c++17`.
> - Linux hardware-test checkmarks below are unverifiable from the repository.

## Overview

Linux-specific polish tasks including libuvc error handling, Linux memory leak detection, performance profiling on Linux, and Linux release packaging.

## Objectives

1. Implement robust error handling for libuvc
2. Verify no memory leaks in Linux-specific code
3. Profile and optimize Linux performance
4. Test all Linux-specific functionality

## Prerequisites

- Phase 6a: Common Polish - Complete

## Notes

- Linux packaging (.deb/.rpm) is in Phase 8: Release
- End-to-end testing is in Phase 7: End-to-End Testing

## Tasks

### Task 6c.1: Error Handling Enhancements (Linux)

**Status**: Completed

**Description**: Implement comprehensive error handling in Linux-specific code.

**Files**: `src/camera/UvcCameraImpl.cpp`, `src/camera/UvcCameraProvider.cpp`, `src/camera/UvcCameraImpl.hpp`, `src/camera/UvcCameraProvider.hpp`

**Steps**:
- [x] Review all error-prone Linux operations:
  - [x] libuvc context initialization
  - [x] USB device enumeration
  - [x] Camera device opening
  - [x] Stream configuration
  - [x] Frame capture callbacks
  - [x] UVC control operations (brightness, white balance, etc.)
- [x] Add error codes:
  - [x] Define Linux-specific error types (LibuvcError, UsbError)
  - [x] Add error messages with context
  - [x] Implement error recovery strategies
- [x] Implement graceful degradation:
  - [x] Camera disconnection: Enter freeze frame with last valid frame
  - [x] USB device errors: Log error, attempt reconnect
  - [x] Stream failures: Restart stream
  - [x] UVC control failures: Log warning, continue without that control
- [x] Handle libuvc errors:
  - [x] Check uvc_error_t return values
  - [x] Use uvc_strerror() for error messages
  - [x] Implement retry logic for transient errors
  - [x] Provide user-friendly messages for common errors
- [x] Add logging:
  - [x] Log all errors with context (file:line)
  - [x] Log libuvc return values
  - [x] Log USB device events (connect/disconnect)
  - [x] Use appropriate log levels (ERROR, WARN, INFO, DEBUG)
- [x] USB permissions handling:
  - [x] Detect permission errors (EACCES, EPERM)
  - [x] Provide user-friendly message about udev rules
  - [x] Document USB permission requirements

**Error Handling Pattern (libuvc)**:
```cpp
uvc_error_t err = uvc_function(...);
if (err != UVC_SUCCESS) {
    std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__
              << " - uvc_function failed: " << uvc_strerror(err) << std::endl;
    return HandleError(err);
}
```

**Validation**: Linux errors are caught, logged, and handled gracefully

---

### Task 6c.2: Memory Leak Prevention (Linux)

**Status**: Completed

**Description**: Verify and ensure no memory leaks in Linux-specific code.

**Files**: All Linux-specific source files

**Steps**:
- [x] Review all allocations:
  - [x] All `new` operations
  - [x] All `malloc` calls
  - [x] Verify matching `delete` / `free`
- [x] Use RAII consistently:
  - [x] Smart pointers for ownership (unique_ptr/shared_ptr)
  - [x] RAII wrappers for resources
  - [x] Move semantics where appropriate
- [x] Review libuvc resources:
  - [x] uvc_context cleanup (uvc_exit)
  - [x] uvc_device cleanup (uvc_unref_device)
  - [x] uvc_device_handle cleanup (uvc_close)
  - [x] uvc_stream_handle cleanup (uvc_stream_close)
  - [x] uvc_frame cleanup (uvc_free_frame)
  - [x] No libuvc resource leaks
- [x] Review SDL resources (Linux-specific):
  - [x] Texture destruction (SDL_DestroyTexture)
  - [x] Surface destruction (SDL_FreeSurface)
  - [x] Renderer cleanup (SDL_DestroyRenderer)
  - [x] Window cleanup (SDL_DestroyWindow)
- [x] Profile with Valgrind:
  - [x] Run full application session
  - [x] Check for leaks with memcheck tool
  - [x] Use leak-check=full for detailed report
  - [x] Check for invalid memory access
  - [x] Fix any found issues
- [x] Review threading resources:
  - [x] Thread pool cleanup (ctpl)
  - [x] Mutex/condition variable cleanup
  - [x] No deadlocks or abandoned locks

**Memory Management Checklist (Linux)**:
- [x] All pointers owned by smart pointers
- [x] No raw `new` without matching `delete`
- [x] No dangling pointers
- [x] Proper cleanup on exceptions
- [x] Resource cleanup in destructors
- [x] All libuvc resources properly released
- [x] No memory leaks in Valgrind
- [x] No invalid memory access in Valgrind

**Validation**: Valgrind shows no memory leaks in Linux-specific code

---

### Task 6c.3: Performance Profiling (Linux)

**Status**: Completed

**Description**: Profile application on Linux to identify and fix bottlenecks.

**Files**: Linux-specific testing

**Steps**:
- [x] Profile with tools:
  - [x] perf: Identify hot functions and CPU cycles
  - [x] valgrind callgrind: Detailed call graph profiling
  - [x] gprof: Function timing and call counts
  - [x] perf record: CPU performance counters
- [x] Measure key Linux operations:
  - [x] libuvc initialization time
  - [x] USB device enumeration time
  - [x] Camera open and stream setup time
  - [x] Frame capture callback overhead
  - [x] UVC control operation time
  - [x] Total frame time including libuvc overhead
- [x] Identify Linux-specific bottlenecks:
  - [x] uvc_frame processing overhead
  - [x] USB transfer latency
  - [x] libuvc callback overhead
  - [x] Memory copy operations in frame processing
  - [x] Lock contention in frame buffer
- [x] Optimize Linux hot paths:
  - [x] Minimize memory copies in frame processing
  - [x] Optimize frame buffer swapping
  - [x] Reduce lock scope in capture callback
  - [x] Optimize UVC control calls (cache where possible)
  - [x] Apply SIMD optimizations to thermal processing
  - [x] Loop unrolling in hot paths
- [x] Benchmark improvements:
  - [x] Before/after measurements
  - [x] Document speedup factors
  - [x] Compare with macOS performance (account for differences)

**Linux Performance Targets**:
- libuvc initialization: < 1 second
- Camera open and stream setup: < 1 second
- Frame capture callback: < 5ms per frame
- UVC control operations: < 1ms per operation
- Total frame time: < 85ms per frame (12 FPS minimum)

**Profiling Commands**:
```bash
# perf profiling
perf record -g ./builddir/thermalcamera
perf report

# valgrind callgrind
valgrind --tool=callgrind ./builddir/thermalcamera
kcachegrind callgrind.out.*

# gprof profiling
# Compile with -pg flag
meson setup builddir -Dbuildtype=debug
gprof builddir/thermalcamera gmon.out > analysis.txt
```

**Validation**: Linux application meets performance targets

---

### Task 6c.4: Linux-Specific Testing

**Status**: Completed

**Description**: Test all Linux-specific functionality.

**Files**: Linux test scenarios

**Steps**:
- [x] Linux camera testing:
  - [x] Build with gcc or clang
  - [x] Test USB device access (/dev/video*)
  - [x] Test with Topdon TC001 camera
  - [x] Test with clone cameras (if available)
  - [x] Verify libuvc functionality
- [x] Linux permissions testing:
  - [x] Test without proper USB permissions
  - [x] Verify error message is user-friendly
  - [x] Test with udev rules installed
  - [x] Document udev rules if needed
- [x] Linux display testing:
  - [x] Verify window handling (X11/Wayland)
  - [x] Test fullscreen mode
  - [x] Test window resizing
  - [x] Test different display servers (X11, Wayland)
- [x] Linux-specific scenarios:
  - [x] USB hotplug (disconnect/reconnect)
  - [x] Multiple cameras connected (select correct one)
  - [x] Camera used by another application
  - [x] System power management (suspend/resume)
  - [x] Low memory conditions (ulimit -v)
- [x] Linux compatibility testing:
  - [x] Test on Ubuntu/Debian (primary target)
  - [x] Test on different Linux distributions (Fedora, Arch if possible)
  - [x] Test on different kernel versions
  - [x] Test with different USB controllers
  - [x] Test with different display drivers

**Testing Checklist (Linux)**:
- [x] Builds on Linux with Meson
- [x] Camera works on Linux with libuvc
- [x] USB permissions handled gracefully
- [x] Rendering is consistent
- [x] Performance is acceptable
- [x] No Linux-specific crashes
- [x] Works on X11 and Wayland (if possible)
- [x] Hotplug testing passed

**Validation**: Linux application works reliably

---

### Task 6c.5: Linux Build System Polish

**Status**: Completed

**Description**: Finalize Meson build configuration for Linux.

**File**: `meson.build` (Linux-specific sections)

**Steps**:
- [x] Review Linux build configuration:
  - [x] All Linux dependencies correct (libuvc, SDL2, threads) - Note: SDL2_ttf not used
  - [x] Compiler flags appropriate (-Wall, -Wextra, -std=c++14)
  - [x] Linux source files included (.cpp, .hpp)
  - [x] No macOS-specific files in Linux build
- [x] Add Linux-specific build options:
  - [x] Debug symbols for profiling (-g)
  - [x] Optimization levels (-O2, -O3)
  - [x] gprof profiling support (-pg)
  - [x] AddressSanitizer support (-fsanitize=address)
  - [x] ThreadSanitizer support (-fsanitize=thread)
- [x] Add testing targets:
  - [x] Unit tests (test_thermal, test_renderer)
  - [x] Integration tests (when available)
  - [x] Performance benchmarks
- [x] Add Linux packaging support (Phase 8 handles actual packaging):
  - [x] Meson package support setup
  - [x] Distribution package metadata (.deb, .rpm)
  - [x] Installation rules (install to /usr/local or /usr)
  - [x] Uninstall support
  - [x] udev rules for USB permissions (if needed)
- [x] Cross-platform compatibility:
  - [x] Linux-specific paths handled
  - [x] Library detection (pkg-config for libuvc, SDL2)
  - [x] Compiler detection (gcc, clang)

**Linux Build Commands**:
```bash
# Debug build with symbols
meson setup builddir --buildtype=debug
meson compile -C builddir

# Release build
meson setup builddir --buildtype=release
meson compile -C builddir

# Install to /usr/local
sudo meson install -C builddir

# Run installed binary
/usr/local/bin/thermalcamera

# Create .deb package (Ubuntu/Debian)
cd builddir
cpack -G DEB

# Create .rpm package (Fedora/RedHat)
cd builddir
cpack -G RPM
```

**udev Rules (if needed)**:
```
# /etc/udev/rules.d/99-thermalcamera.rules
# Allow user access to Topdon TC001 thermal camera
SUBSYSTEM=="usb", ATTR{idVendor}=="1234", ATTR{idProduct}=="5678", MODE="0666"
```

**Validation**: Linux build system is robust and creates installable packages

---

### Task 6c.6: Linux Code Quality Improvements

**Status**: Completed

**Description**: Final code review and quality improvements for Linux-specific code.

**Files**: All Linux-specific C++ source files

**Steps**:
- [x] Run static analysis:
  - [x] clang-tidy warnings
  - [x] cppcheck
  - [x] AddressSanitizer (ASAN)
  - [x] ThreadSanitizer (TSAN)
  - [x] UndefinedBehaviorSanitizer (UBSAN)
- [x] Fix warnings:
  - [x] gcc/clang compiler warnings
  - [x] Static analysis warnings
  - [x] Runtime issues
  - [x] libuvc deprecation warnings
- [x] Code style check:
  - [x] Verify Allman brace style for classes/functions
  - [x] Verify K&R brace style for lambdas
  - [x] Check naming conventions (PascalCase, _camelCase)
  - [x] Verify indentation (4 spaces, no tabs)
- [x] Refactor complex sections:
  - [x] Extract functions where needed
  - [x] Reduce cyclomatic complexity
  - [x] Improve readability
  - [x] Add missing abstractions
  - [x] Simplify libuvc callback code
- [x] Verify RAII usage:
  - [x] No manual memory management in C++
  - [x] Smart pointers used correctly
  - [x] Resource cleanup in destructors
  - [x] libuvc resources wrapped in RAII if possible
- [x] Thread safety review (Linux-specific):
  - [x] Verify all shared data is properly protected
  - [x] Check for race conditions in frame buffer
  - [x] Verify synchronization with main thread
  - [x] Review atomic variable usage
  - [x] Check for deadlock scenarios

**Quality Metrics (Linux)**:
- Zero compiler warnings (gcc/clang)
- Zero static analysis warnings (clang-tidy, cppcheck)
- No ASAN/TSAN/UBSAN reports
- Code complexity within limits
- All CODING_GUIDELINES.md rules followed

**Validation**: Linux code quality is high and maintainable

---

### Task 6c.7: Linux End-to-End Testing

**Status**: Completed

**Description**: Comprehensive final testing on Linux.

**File**: Linux test scenarios

**Steps**:
- [x] Test all Linux features:
  - [x] Camera capture via libuvc
  - [x] All colormaps
  - [x] All scale factors
  - [x] All rotations
  - [x] All key bindings
  - [x] Mouse interaction (temperature probe)
  - [x] Freeze frame
  - [x] Font rendering (HUD text)
- [x] Linux-specific stress testing:
  - [x] Extended runtime (hours)
  - [x] Rapid key presses
  - [x] Camera disconnect/reconnect (USB hotplug)
  - [x] Low memory conditions (ulimit -v)
  - [x] High CPU conditions
  - [x] System suspend/resume with camera active
  - [x] Rapid scale changes
  - [x] Rapid colormap changes
- [x] Linux scenario testing:
  - [x] USB permissions denied (verify user-friendly error)
  - [x] USB permissions granted (with udev rules)
  - [x] Multiple cameras connected (select correct thermal camera)
  - [x] Camera used by another app (handle error gracefully)
  - [x] X11 display server
  - [x] Wayland display server (if available)
- [x] Linux compatibility testing:
  - [x] Test on Ubuntu 20.04 or later
  - [x] Test on Debian 11 or later
  - [x] Test on different Linux distributions (Fedora, Arch if possible)
  - [x] Test with different kernel versions
  - [x] Test with different USB controllers
  - [x] Test with different graphics drivers

**Test Matrix (Linux-Specific)**:
| Feature | Test Case | Expected Result |
|---------|-----------|-----------------|
| Camera (libuvc) | Live capture | Display at 25 FPS |
| USB Permissions | No udev rules | User-friendly error shown |
| USB Permissions | With udev rules | Works without sudo |
| Hotplug | Camera disconnect | Enter freeze frame |
| Hotplug | Camera reconnect | Resume capture |
| X11 | Window management | Correct behavior |
| Wayland | Window management | Correct behavior (if supported) |
| Multiple cameras | Enumerate all | Select correct thermal camera |

**Validation**: All Linux features work reliably

---

## Dependencies

- Phase 6a: Common Polish (must be completed first)
- Phase 7: End-to-End Testing
- Phase 8: Release
- All previous phases (1-5)
- gcc or clang compiler
- libuvc development headers and library
- SDL2 development headers
- pkg-config
- Topdon TC001 thermal camera (or clone)

## Success Criteria

Phase 6c is complete when:
- [x] Linux-specific error handling is comprehensive
- [x] No memory leaks detected in Linux-specific code
- [x] Linux application meets performance targets
- [x] All Linux features tested and working
- [x] Linux build system is robust
- [x] Linux code quality is high

## Notes

- This phase focuses on Linux-specific code and testing
- Must complete Phase 6a (Common) before starting this phase
- Linux packaging (.deb/.rpm) is in Phase 8: Release
- End-to-end testing is in Phase 7: End-to-End Testing
- Test on Ubuntu 20.04 LTS or later (primary target)
- Test on Debian 11 or later
- USB permissions may require udev rules - document this clearly

## Testing Environment

**Recommended Linux Setup**:
- Ubuntu 20.04 LTS or Debian 11 (primary)
- gcc or clang compiler
- libuvc development library
- SDL2 development library
- Topdon TC001 thermal camera
- valgrind for leak detection
- perf for performance profiling
- Test on both X11 and Wayland if possible

## Common Issues and Solutions

**USB Permissions**:
If users get permission denied errors when accessing the camera, provide instructions:
1. Check camera vendor/product ID: `lsusb`
2. Create udev rule: `/etc/udev/rules.d/99-thermalcamera.rules`
3. Reload udev: `sudo udevadm control --reload-rules`
4. Replug camera

**Distribution-Specific Notes**:
- **Ubuntu/Debian**: Install build dependencies with `apt-get install libuvc-dev libsdl2-dev`
- **Fedora**: Install build dependencies with `dnf install libuvc-devel SDL2-devel`
- **Arch**: Install build dependencies with `pacman -S libuvc sdl2`

