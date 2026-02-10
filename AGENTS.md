# ThermalCamera - Agent Instructions

This document provides instructions for agents working on the ThermalCamera project. All agents must follow the [CODING_GUIDELINES.md](CODING_GUIDELINES.md) file when making changes.

## Project Overview

ThermalCamera is a cross-platform C++14 application that:
- Captures thermal camera video via platform-specific backends (libuvc on Linux, AVFoundation on macOS)
- Processes thermal data (Kelvin → Celsius/Fahrenheit)
- Renders frames with SDL 2.x
- Displays temperature data with colormaps and HUD
- Uses a 3-threaded architecture (capture/process/render)
- Supports macOS and Linux (Windows planned)

## Code Style Guidelines

All agents MUST follow [CODING_GUIDELINES.md](CODING_GUIDELINES.md) which specifies:

- **Build System**: Meson
- **Indentation**: 4 spaces (no tabs)
- **Brace Style**: Allman style for classes/functions, K&R for lambdas
- **Naming Conventions**:
  - Namespaces: `PascalCase`
  - Classes/Functions: `PascalCase`
  - Member variables: `_camelCase`
  - Local variables: `camelCase` (snake_case for math variables)
- **C++ Standard**: C++17
- **Memory Management**: Use smart pointers (unique_ptr/shared_ptr), no raw pointers for ownership
- **Concurrency**: Use `ctpl_stl_tls.h` for thread pools with TLS
- **CLI Parsing**: Use `cmdline` library
- **Logging**: Use `std::cerr` for logging/debug, `std::cout` for data output

## Project Structure

```
ThermalCamera/
├── meson.build                    # Main build configuration
├── CODING_GUIDELINES.md          # Coding standards
├── AGENTS.md                     # This file
├── plans/                        # Implementation plans
│   ├── phase1-foundation.md
│   ├── phase1a-macos.md          # macOS-specific implementation
│   ├── phase2-thermal-processing.md
│   ├── phase3-colormaps-rendering.md
│   ├── phase4-threading.md
│   ├── phase5-interaction.md
│   ├── phase6-polish.md          # Phase 6 overview
│   ├── phase6a-common.md        # Common polish tasks
│   ├── phase6b-macos.md         # macOS-specific polish
│   ├── phase6c-linux.md         # Linux-specific polish
│   ├── phase7-testing.md         # End-to-end testing
│   └── phase8-release.md         # Release preparation
├── src/
│   ├── main.cpp
│   ├── FrameBuffer.hpp           # Double buffering implementation
│   ├── camera/                   # Platform-specific camera implementations
│   │   ├── ICamera.hpp           # Camera interface
│   │   ├── Camera.hpp            # Platform typedef
│   │   ├── UvcCameraImpl.hpp     # Linux implementation
│   │   ├── UvcCameraImpl.cpp
│   │   ├── UvcCameraProvider.hpp
│   │   ├── UvcCameraProvider.cpp
│   │   ├── MacCameraImpl.hpp     # macOS implementation
│   │   ├── MacCameraImpl.mm
│   │   ├── MacCameraProvider.hpp
│   │   ├── MacCameraProvider.mm
│   │   ├── MacAVFoundationStreamer.hpp
│   │   ├── MacAVFoundationStreamer.mm
│   │   ├── MacIOKitUvcController.hpp
│   │   ├── MacIOKitUvcController.cpp
│   │   └── ObjCPtr.hpp          # CFObject RAII wrapper
│   ├── thermal/
│   │   ├── ThermalProcessor.hpp
│   │   └── ThermalProcessor.cpp
│   ├── render/
│   │   ├── Renderer.hpp
│   │   └── Renderer.cpp
│   ├── config/
│   │   └── Config.hpp
│   ├── fonts/                    # Embedded fonts
│   │   ├── EspySans_10.h
│   │   └── EspySansBold_10.h
│   ├── vendor/
│   │   ├── cmdline/
│   │   └── ctpl/
│   ├── tests/
│   │   ├── test_thermal.cpp
│   │   └── test_renderer.cpp
│   └── colormaps.hpp
└── subprojects/
    ├── sdl2
    └── libuvc
```

## Agent Workflow

When working on ThermalCamera project, agents should:

1. **Milestone-Driven Workflow**:
   - **Start**: Create implementation plans for the current phase (e.g., `plans/phase2.md`).
   - **Execute**: Implement code, verifying against the plan.
   - **Milestone**: When the **USER declares** a major milestone is reached:
     1. Update `spec/architecture.md` and `spec/specification.md` with new system state.
     2. Commit changes (Ensure the completed `plans/phaseX.md` is included in this commit to preserve history).
     *Note: Do NOT declare a milestone in commit messages or delete plan files until explicitly instructed by the user.*
   - **Cleanup**: When starting the **next** phase/task (after user instruction), **DELETE** the previously completed plan files from `plans/`. This keeps the active workspace clean.

2. **Plan Maintenance**: 
   - Update the active `plans/phaseX.md` file as tasks are completed to track progress.
   - Do not let the plan file become stale.

3. **Use Todo Lists**: Always create and maintain a todo list for tracking progress within a session.
4. **Test Incrementally**: Run build and tests after each significant change.
5. **Test on Target Platforms**: Test on Linux and macOS for cross-platform compatibility.
6. **Follow Coding Standards**: Adhere to CODING_GUIDELINES.md conventions.
7. **Document Changes**: Update relevant documentation when adding features.
8. **Architectural Reasoning**: When updating `spec/architecture.md`, explicitly explain the *why* (reasoning) behind decisions, especially if they arose from user discussions.
9. **Handle Dependencies**: Use Meson subprojects for external libraries.
10. **Platform-Specific Code**: Keep platform-specific code in separate files (.mm for macOS, .cpp for Linux).

## Key Technology Stack

- **Camera Access**:
  - Linux: libuvc (USB thermal camera, YUYV 4:2:2 format)
  - macOS: AVFoundation/CoreMedia (CMBuffer, CVImageBuffer)
  - Cross-platform: ICamera interface
- **Rendering**: SDL 2.x (cross-platform 2D graphics)
- **Threading**: ctpl_stl_tls.h (thread pool with thread-local storage)
- **CLI Parsing**: cmdline (single-header library)
- **Colormaps**: Custom pre-generated data in colormaps.hpp
- **Fonts**: Embedded fonts (EspySans 10pt)
- **Build System**: Meson + Ninja
- **macOS Build**: Objective-C++ (.mm), ARC, Apple frameworks

## Temperature Conversions

Reference code formulas (from Thermal-Camera-Redux):

```cpp
// 16-bit Kelvin to Celsius
float KelvinToCelsius(uint16_t kelvin) {
    return (kelvin / 64.0f) - 273.15f;
}

// Celsius to Fahrenheit
float CelsiusToFahrenheit(float celsius) {
    return (celsius * 1.8f) + 32.0f;
}

// Celsius to Kelvin (for threshold calculations)
uint16_t CelsiusToKelvin(float celsius) {
    return static_cast<uint16_t>(round((celsius + 273.15f) * 64.0f));
}
```

## Frame Format

**Raw Frame**: 384 rows × 256 cols × 2 bytes (YUYV 4:2:2)
- Rows 0-191: Visible camera image (RGB-like)
- Rows 192-383: Thermal data (16-bit Kelvin in Y component)

**Thermal Sub-frame**: 192 rows × 256 cols × 2 bytes
- Each thermal pixel is stored as 16-bit Kelvin value
- Manual YUYV parsing required to extract thermal data

## Build Commands

```bash
# Setup build directory
meson setup builddir

# Compile
meson compile -C builddir

# Run tests (when available)
meson test -C builddir

# Install
meson install -C builddir
```

## Agent-Specific Instructions

### Implementation Agent
- Follow phase plans in `plans/` directory sequentially
- Create todo lists for each phase
- Test after completing each phase
- Update documentation as features are added

### Review Agent
- Verify adherence to CODING_GUIDELINES.md
- Check for memory leaks and proper RAII usage
- Ensure thread safety where applicable
- Verify cross-platform compatibility

### Testing Agent
- Create unit tests for core functionality
- Test on multiple platforms (Linux, macOS)
- Verify camera compatibility (Topdon TC001 and clones)
- Performance benchmarking (Linux: perf/valgrind, macOS: Instruments)
- Memory leak detection (Linux: Valgrind, macOS: Instruments)

## Common Tasks

### Adding New Features
1. Create feature branch from main
2. Create todo list tracking implementation steps
3. Implement following CODING_GUIDELINES.md
4. Add tests if applicable
5. Update documentation
6. Submit for review

### Fixing Bugs
1. Reproduce the bug
2. Create todo list for debugging steps
3. Implement fix following coding standards
4. Add regression test if applicable
5. Document the fix

### Performance Optimization
1. Profile using tools (gprof, perf, valgrind)
2. Identify bottlenecks
3. Optimize hot paths following reference code patterns
4. Benchmark before/after changes
5. Document optimizations

## Reference Code

The original Thermal-Camera-Redux reference code is in `./FOR_REFERENCE_ONLY/Thermal-Camera-Redux/`.
Key insights:
- Multi-threaded architecture with careful synchronization
- Manual YUYV parsing for thermal data extraction
- Optimized loop unrolling for min/max calculations
- Complex UI features (rulers, crosshairs, histograms)
- Temperature conversion formulas above

## Contact & Reporting

Report issues, bugs, or questions via project issue tracker.
