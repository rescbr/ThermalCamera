# Phase 8: Release

## Overview

Prepare and publish release packages for Linux and macOS.

## Objectives

1. Create version information
2. Build release binaries for all platforms
3. Create distribution packages
4. Tag release in version control
5. Publish release notes

## Prerequisites

- Phase 6a: Common Polish - Complete
- Phase 6b: macOS-Specific Polish - Complete
- Phase 6c: Linux-Specific Polish - Complete
- Phase 7: End-to-End Testing - All tests pass

## Tasks

### Task 8.1: Version Information

**Status**: Completed

**Description**: Create version constants and update all version references.

**Steps**:
- [x] Create version header file:
  - [x] Create `src/version.hpp`
  - [x] Define version constants (MAJOR, MINOR, PATCH)
  - [x] Define version string
- [x] Update version in README.md
- [ ] Update version in documentation
- [x] Update version in build scripts (if any)

**Version Header Template**:
```cpp
#ifndef VERSION_HPP
#define VERSION_HPP

#define THERMALCAMERA_VERSION_MAJOR 1
#define THERMALCAMERA_VERSION_MINOR 0
#define THERMALCAMERA_VERSION_PATCH 0
#define THERMALCAMERA_VERSION_STRING "1.0.0"

#endif // VERSION_HPP
```

**Validation**: Version information consistent across all files

---

### Task 8.2: Linux Release Build

**Status**: Pending

**Description**: Build and package Linux release.

**Steps**:
- [ ] Build release binary:
  - [ ] Release optimization level (-O2 or -O3)
  - [ ] Stripped symbols (or keep for debugging)
  - [ ] No debug output
  - [ ] Verify performance targets
- [ ] Create Linux distribution:
  - [ ] Binary executable
  - [ ] README and LICENSE files
  - [ ] Build instructions
  - [ ] Installation instructions
  - [ ] udev rules for USB permissions (if needed)
- [ ] Create packages:
  - [ ] .deb package for Debian/Ubuntu
    - [ ] Package metadata (control file)
    - [ ] Post-install script (udev rules setup)
    - [ ] Pre-remove script (udev rules cleanup)
  - [ ] .rpm package for Fedora/RedHat
    - [ ] Package spec file
    - [ ] Post-install script (udev rules setup)
    - [ ] Pre-remove script (udev rules cleanup)
  - [ ] Source tarball
- [ ] Test packages:
  - [ ] Install .deb package on clean system
  - [ ] Install .rpm package on clean system
  - [ ] Verify application runs correctly
  - [ ] Verify uninstall works correctly

**Linux Release Checklist**:
- [ ] Release build compiled with optimizations
- [ ] Binary runs correctly
- [ ] .deb package created and tested
- [ ] .rpm package created and tested
- [ ] udev rules included (if needed)
- [ ] README and LICENSE included
- [ ] Performance targets met
- [ ] No crashes or major issues
- [ ] USB permissions documented

**Validation**: Linux release packages are complete and tested

---

### Task 8.3: macOS Release Build

**Status**: Completed

**Description**: Build and package macOS release.

**Steps**:
- [x] Build release binary:
  - [x] Release optimization level (-O2 or -O3)
  - [x] Stripped symbols (or keep for debugging)
  - [x] No debug output
  - [x] Verify performance targets
- [x] Create macOS distribution:
  - [x] .app bundle with correct structure
  - [x] Info.plist with correct metadata
  - [x] README and LICENSE files (in bundle or separate)
- [x] Create disk image (.dmg):
  - [x] DMG with .app bundle
  - [x] Background image (optional for v1.0)
  - [x] Volume name: "ThermalCamera v1.0.0"
  - [x] Verify DMG opens and mounts correctly
- [ ] Code signing (optional for v1.0):
  - [ ] Sign .app bundle (for distribution outside App Store)
  - [ ] Verify signature with codesign -vvv
  - [ ] Notarization (optional for v1.0, needed for distribution)
- [x] Test .app bundle:
  - [x] Launch from Finder
  - [x] Verify Info.plist is correct
  - [x] Test camera authorization
  - [x] Verify clean quit

**macOS Release Checklist**:
- [x] Release build compiled with optimizations
- [x] .app bundle launches correctly
- [x] Info.plist is complete and correct
- [x] DMG created and tested
- [ ] App icon included (if available)
- [x] README and LICENSE included
- [x] Performance targets met
- [x] No crashes or major issues
- [x] Camera authorization instructions clear

**Validation**: macOS release package is complete and tested

---

### Task 8.4: Source Distribution

**Status**: Pending

**Description**: Create source tarball for distribution.

**Steps**:
- [ ] Create source archive:
  - [ ] Include all source files
  - [ ] Include all platform-specific files
  - [ ] Include subprojects directory (empty or with notes)
  - [ ] Include README.md
  - [ ] Include LICENSE file
  - [ ] Include build instructions
  - [ ] Include AGENTS.md and CODING_GUIDELINES.md
  - [ ] Include plans directory
  - [ ] Exclude build artifacts (builddir/, .DS_Store, etc.)
- [ ] Test source build:
  - [ ] Extract tarball on clean system
  - [ ] Build from source
  - [ ] Verify application runs correctly

**Validation**: Source distribution is complete and buildable

---

### Task 8.5: Version Tagging

**Status**: Pending

**Description**: Tag release in version control.

**Steps**:
- [ ] Update changelog:
  - [ ] Document all new features
  - [ ] Document all bug fixes
  - [ ] Document known limitations
  - [ ] Document platform support
- [ ] Create git tag:
  - [ ] Tag name: v1.0.0
  - [ ] Tag message: Release v1.0.0
  - [ ] Push tag to remote
- [ ] Generate release notes:
  - [ ] Summary of changes
  - [ ] Platform support details
  - [ ] Camera compatibility list
  - [ ] Known issues
  - [ ] Future plans

**Validation**: Release tagged and documented

---

### Task 8.6: GitHub Release

**Status**: Pending

**Description**: Create GitHub release with all assets.

**Steps**:
- [ ] Create GitHub release:
  - [ ] Release title: ThermalCamera v1.0.0
  - [ ] Release description (from release notes)
  - [ ] Tag: v1.0.0
- [ ] Upload release assets:
  - [ ] Source tarball (thermalcamera-1.0.0.tar.gz)
  - [ ] macOS .dmg (ThermalCamera-1.0.0.dmg)
  - [ ] Linux .deb (thermalcamera_1.0.0_amd64.deb)
  - [ ] Linux .rpm (thermalcamera-1.0.0-1.x86_64.rpm)
- [ ] Verify release:
  - [ ] All assets download correctly
  - [ ] Checksums match (optional for v1.0)
  - [ ] Release notes display correctly

**Validation**: GitHub release published successfully

---

### Task 8.7: Release Notes

**Status**: Pending

**Description**: Write comprehensive release notes.

**Release Notes Template**:
```markdown
# ThermalCamera v1.0.0

## Overview

ThermalCamera v1.0.0 is a cross-platform thermal camera viewer for Topdon TC001 and compatible USB thermal cameras. It provides real-time thermal imaging with accurate temperature measurement, multiple visualization options, and responsive user interaction.

## Features

- Real-time USB thermal camera capture (25 FPS)
- Accurate temperature measurement (Kelvin, Celsius, Fahrenheit)
- Multiple colormaps for thermal visualization
- Scaling (1x to fullscreen) and rotation (0°, 90°, 180°, 270°)
- Mouse temperature probe
- Freeze frame functionality
- Embedded font support for on-screen display
- Responsive keyboard shortcuts

## Platform Support

### Linux
- Ubuntu 20.04 LTS or later
- Debian 11 or later
- Other distributions with libuvc and SDL2 support
- USB thermal camera support via libuvc
- Includes udev rules for USB permissions

### macOS
- macOS 12 Monterey or later
- Intel and Apple Silicon support
- USB thermal camera support via AVFoundation/CoreMedia
- Distributed as .app bundle

## Installation

### Linux
```bash
# Ubuntu/Debian
sudo dpkg -i thermalcamera_1.0.0_amd64.deb

# Fedora/RedHat
sudo rpm -i thermalcamera-1.0.0-1.x86_64.rpm

# From source
meson setup builddir
meson compile -C builddir
sudo meson install -C builddir
```

### macOS
```bash
# Download and open .dmg
# Drag ThermalCamera.app to Applications folder
# Launch from Applications
```

## Camera Compatibility

Tested with:
- Topdon TC001
- Topdon TC001 clones (may vary in behavior)

## Key Bindings

- Q: Quit
- F: Fullscreen toggle
- +/ -: Scale up/down
- M: Next colormap
- R: Rotate 90°
- Space: Freeze/unfreeze frame

## Known Limitations

- Windows support is planned for future releases
- Camera must be connected before starting application
- Only one thermal camera supported at a time
- Camera permissions required on first launch (macOS)

## Technical Details

- C++17 with Meson build system
- Cross-platform camera abstraction (ICamera)
- Linux: libuvc for USB camera access
- macOS: AVFoundation/CoreMedia for camera access
- SDL2 for rendering
- 3-threaded architecture (capture/process/render)
- Embedded fonts for HUD display

## Acknowledgments

- Based on Thermal-Camera-Redux reference implementation
- Uses libuvc for Linux USB camera support
- Uses SDL2 for cross-platform rendering
- Uses EspySans font for on-screen display

## Future Plans

- Windows support
- Additional camera models
- Recording and playback
- Advanced analysis tools
- More customization options
```

**Validation**: Release notes are comprehensive and accurate

---

## Dependencies

- Phase 6a: Common Polish - Complete
- Phase 6b: macOS-Specific Polish - Complete
- Phase 6c: Linux-Specific Polish - Complete
- Phase 7: End-to-End Testing - All tests pass

## Success Criteria

Phase 8 is complete when:
- [ ] Version information created and consistent
- [ ] Linux release packages built and tested
- [ ] macOS release package built and tested
- [ ] Source distribution created and tested
- [ ] Release tagged in version control
- [ ] GitHub release published with all assets
- [ ] Release notes written and published

## Notes

- Only proceed with release if Phase 7 (End-to-End Testing) passed all tests
- For v1.0, code signing and notarization are optional but recommended for macOS
- Consider App Store distribution for future versions
- Monitor for bug reports after release
- Plan for v1.0.1 if critical bugs are found

## Release Checklist

**Before Release**:
- [ ] All Phase 7 tests pass
- [ ] All performance targets met
- [ ] No known critical bugs
- [ ] Documentation complete
- [ ] Code quality verified

**Release Day**:
- [ ] Version tagged (v1.0.0)
- [ ] Release notes written
- [ ] Linux packages built and tested
- [ ] macOS package built and tested
- [ ] Source tarball created and tested
- [ ] GitHub release created
- [ ] All assets uploaded
- [ ] Release published

**After Release**:
- [ ] Announcement posted
- [ ] Monitor for issues
- [ ] Respond to user feedback
- [ ] Plan next release if needed
