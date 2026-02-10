# Phase 2: Thermal Processing

## Overview

Extract thermal data from YUYV format and implement temperature conversions and calculations.

## Objectives

1. Parse YUYV 4:2:2 format manually
2. Extract thermal sub-frame from raw frames
3. Implement temperature unit conversions (Kelvin ↔ Celsius ↔ Fahrenheit)
4. Calculate Min/Avg/Max temperatures
5. Implement thermal data structure

## Tasks

### Task 2.1: Thermal Data Structures

**Status**: Completed

**Description**: Define structures for thermal data and temperature representation.

**File**: `src/thermal/ThermalProcessor.hpp`

**Steps**:
- [x] Define `Temperature` struct:
  - [x] `uint16_t kelvin` - Raw 16-bit Kelvin value
  - [x] `float celsius` - Converted temperature in Celsius
  - [x] `float fahrenheit` - Converted temperature in Fahrenheit
  - [x] `int row` - Pixel row coordinate
  - [x] `int col` - Pixel column coordinate
- [x] Define `ThermalFrame` struct:
  - [x] `std::vector<uint8_t> _data` - Raw YUYV data (192 × 256 × 2 bytes)
  - [x] `int _width` - Width in pixels (256)
  - [x] `int _height` - Height in pixels (192)
  - [x] `Temperature _min` - Minimum temperature in frame
  - [x] `Temperature _avg` - Average temperature in frame
  - [x] `Temperature _max` - Maximum temperature in frame
  - [x] `Temperature _center` - Temperature at frame center
- [x] Use Allman brace style
- [x] Follow naming conventions: `_camelCase` for members

**Reference**: Temperature struct should match reference code patterns.

**Validation**: Structures compile and can be instantiated

---

### Task 2.2: YUYV Parsing

**Status**: Completed

**Description**: Implement manual YUYV 4:2:2 format parsing to extract thermal data.

**File**: `src/thermal/ThermalProcessor.cpp`

**Steps**:
- [x] Create `ExtractThermalFrame()` function:
  - [x] Take raw frame (384 × 256 × 2 bytes)
  - [x] Extract bottom half (rows 192-383)
  - [x] Parse YUYV format to extract Y component
  - [x] YUYV format: Y0 U0 Y1 V0 Y2 U2 Y3 V2...
  - [x] Each thermal pixel is stored as 16-bit Kelvin in Y component
  - [x] Interleave U/V to form 16-bit values
- [x] Handle thermal sub-frame extraction:
  - [x] Source: Raw frame bottom half
  - [x] Destination: ThermalFrame structure
  - [x] Width: 256 pixels
  - [x] Height: 192 pixels
- [x] Implement efficient memory access:
  - [x] Use pointer arithmetic for speed
  - [x] Minimize allocations
- [x] Add debug logging for verification

**YUYV Parsing Logic**:
```cpp
// Each thermal pixel is 2 bytes (Y high byte, Y low byte)
// Combined to form 16-bit Kelvin value
uint16_t* thermalPtr = (uint16_t*)(rawFrame + (192 * 256 * 2));
for (int i = 0; i < 192 * 256; ++i) {
    thermalFrame._data[i] = thermalPtr[i];
}
```

**Validation**: Extracted thermal data matches expected format (16-bit Kelvin values)

---

### Task 2.3: Temperature Conversions

**Status**: Completed

**Description**: Implement conversion functions between Kelvin, Celsius, and Fahrenheit.

**File**: `src/thermal/ThermalProcessor.cpp`

**Steps**:
- [x] Implement `KelvinToCelsius()`:
  - [x] Formula: `kelvin / 64.0f - 273.15f`
  - [x] Return `float` value
- [x] Implement `CelsiusToFahrenheit()`:
  - [x] Formula: `celsius * 1.8f + 32.0f`
  - [x] Return `float` value
- [x] Implement `CelsiusToKelvin()`:
  - [x] Formula: `(celsius + 273.15f) * 64.0f`
  - [x] Return `uint16_t rounded value`
- [x] Follow reference code formulas exactly
- [x] Add inline hints for performance
- [x] Mark functions as `constexpr` where possible

**Reference Formulas**:
- Kelvin to Celsius: `(kelvin / 64.0f) - 273.15f`
- Celsius to Fahrenheit: `celsius * 1.8f + 32.0f`
- Celsius to Kelvin: `round((celsius + 273.15f) * 64.0f)`

**Validation**: Conversion tests pass with known values

---

### Task 2.4: Temperature Statistics

**Status**: Completed

**Description**: Calculate Min/Avg/Max temperatures from thermal frame.

**File**: `src/thermal/ThermalProcessor.cpp`

**Steps**:
- [x] Create `CalculateTemperatureStats()` function:
  - [x] Take ThermalFrame reference
  - [x] Iterate through all pixels
  - [x] Calculate minimum temperature
  - [x] Calculate maximum temperature
  - [x] Calculate average temperature
  - [x] Find row/col of min/max
  - [x] Calculate center temperature (row 96, col 128)
  - [x] Convert all to Celsius and Fahrenheit
  - [x] Store in ThermalFrame structure
- [x] Implement optimizations:
  - [x] Loop unrolling (process 8 pixels at once)
  - [x] Minimize branching
  - [x] Use 64-bit accumulator for sum
- [x] Handle edge cases:
  - [x] Empty frame
  - [x] Invalid temperature values
- [x] Return success/failure status

**Optimized Loop Pattern**:
```cpp
// Loop unrolling for min/max/avg
for (int i = 0; i < pixelCount; i += 8) {
    // Process 8 pixels per iteration
    // Update min, max, sum
}
```

**Validation**: Statistics calculated correctly for test frames

---

### Task 2.5: Temperature Lookup

**Status**: Completed

**Description**: Implement function to get temperature at specific pixel location.

**File**: `src/thermal/ThermalProcessor.cpp`

**Steps**:
- [x] Create `GetTemperatureAt()` function:
  - [x] Take ThermalFrame reference
  - [x] Take row and col coordinates
  - [x] Validate coordinates (0-191, 0-255)
  - [x] Extract Kelvin value from frame data
  - [x] Return Temperature struct with all units
  - [x] Set row/col in returned struct
- [x] Handle out-of-bounds:
  - [x] Return temperature with NaN values
  - [x] Log error to `std::cerr`
- [x] Use const reference for frame parameter

**Validation**: Temperature lookup matches expected values at known positions

---

### Task 2.6: ThermalProcessor Class

**Status**: Completed

**Description**: Create ThermalProcessor class to encapsulate thermal data operations.

**File**: `src/thermal/ThermalProcessor.hpp` and `.cpp`

**Steps**:
- [x] Define `ThermalProcessor` class:
  - [x] Public methods:
    - [x] `bool ProcessFrame(const uint8_t* rawFrame, ThermalFrame& output)`
    - [x] `Temperature GetTemperatureAt(const ThermalFrame& frame, int row, int col)`
  - [x] Private helper methods:
    - [x] `void ExtractThermalData(...)`
    - [x] `void CalculateTemperatureStats(...)`
    - [x] `Temperature ConvertKelvin(...)`
  - [x] Private members:
    - [x] `std::unique_ptr<uint8_t[]> _scratchBuffer`
    - [x] `bool _useCelsius` - Temperature display unit
- [x] Implement `ProcessFrame()`:
  - [x] Call ExtractThermalData
  - [x] Call CalculateTemperatureStats
  - [x] Convert all temperatures to display units
  - [x] Return success/failure
- [x] Follow Allman brace style
- [x] Use smart pointers for memory management

**Validation**: ThermalProcessor correctly processes raw frames

---

### Task 2.7: Testing

**Status**: Completed

**Description**: Create tests for thermal processing functionality.

**Steps**:
- [x] Create test frame with known temperatures
- [x] Test YUYV parsing:
  - [x] Verify thermal sub-frame extraction
  - [x] Verify 16-bit Kelvin values
- [x] Test temperature conversions:
  - [x] Kelvin → Celsius (known values)
  - [x] Celsius → Fahrenheit (known values)
  - [x] Round-trip conversions
- [x] Test statistics calculations:
  - [x] Known frame, verify min/avg/max
  - [x] Verify row/col positions of extrema
- [x] Test temperature lookup:
  - [x] Various pixel locations
  - [x] Out-of-bounds handling
- [x] Run with valgrind for memory leaks

**Test Cases**:
- Conversion: 0°C = 32°F = 273.15K
- Conversion: 100°C = 212°F = 373.15K
- Statistics: Uniform frame (avg = min = max)

**Validation**: All tests pass without memory leaks

---

## Dependencies

- C++14 standard library
- Phase 1 foundation (UvcCamera)
- Existing colormaps.hpp (for later phase)

## Success Criteria

Phase 2 is complete when:
- [x] Thermal data structures are defined
- [x] YUYV format is parsed correctly
- [x] Temperature conversions are accurate
- [x] Min/Avg/Max calculations are correct
- [x] Temperature lookup works at any pixel location
- [x] Code follows CODING_GUIDELINES.md
- [x] Tests verify functionality

## Next Phase

After Phase 2 completion, proceed to **Phase 3: Colormaps & Rendering**.

