# BDF to C++ Header Converter

This tool converts BDF (Bitmap Distribution Format) font files into C++ header files containing a static lookup table for ISO-8859-1 (Latin-1) characters.

## Features

*   **Zero Dependencies:** Uses only the Python standard library.
*   **AGL Mapping:** Supports both `glyphlist.txt` (name;hex) and `aglfn.txt` (hex;name) formats from the Adobe Glyph List to map glyph names to Unicode codepoints.
*   **Direct Lookup:** Generates a `static const Glyph glyphs[256]` array where the index corresponds directly to the character code (O(1) access).
*   **Robustness:** Handles non-ASCII metadata in BDF files gracefully by replacing invalid characters.
*   **Portable Output:** Generates self-contained C++ headers with `#pragma once` and namespaced structs.
*   **Font Metrics:** Extracts global font bounding box (`FONTBOUNDINGBOX`) information for precise layout.

## Usage

The script reads the BDF content from **standard input (stdin)** and writes the generated C++ code to **standard output (stdout)**.

### Arguments

```bash
python3 fonts/tools/bdf_to_cpp_header.py <path_to_glyphlist.txt>
```

*   `<path_to_glyphlist.txt>`: Absolute or relative path to the Adobe Glyph List file (e.g., `fonts/font_specs/agl-aglfn/glyphlist.txt` or `fonts/font_specs/agl-aglfn/aglfn.txt`). The script automatically detects the format.

### Examples

#### Convert a Single Font

```bash
# Assuming you are in the project root
mkdir -p src/fonts

python3 fonts/tools/bdf_to_cpp_header.py fonts/font_specs/agl-aglfn/glyphlist.txt \
< fonts/espy_dos/espysans.10 \
> src/fonts/EspySans_10.h
```

#### Batch Convert All Fonts

You can use a simple shell loop to convert all numeric-extension BDF files in a directory:

```bash
mkdir -p src/fonts

for f in fonts/espy_dos/*.[0-9]*; do
    # Extract filename and create a C-friendly output name (e.g., espysans.10 -> espysans_10.h)
    base=$(basename "$f")
    name="${base%.*}"
    ext="${base##*.}"
    out="src/fonts/${name}_${ext}.h"
    
    echo "Converting $f -> $out"
    python3 fonts/tools/bdf_to_cpp_header.py fonts/font_specs/agl-aglfn/glyphlist.txt < "$f" > "$out"
done
```

## Generated C++ Structure

The output header wraps the font data in a namespace derived from the font name and size.

```cpp
// Example output structure
namespace Fonts {
namespace EspySans_10 {

    struct Glyph {
        const uint8_t* bitmap; // Pointer to byte-aligned bitmap data
        int width;             // Bitmap width
        int height;            // Bitmap height
        int x_offset;          // Horizontal offset
        int y_offset;          // Vertical offset
        int advance;           // Horizontal advance
    };

    // ... bitmap data arrays ...

    // Latin-1 Glyph Table (0-255)
    // Index corresponds to ISO-8859-1 codepoint
    static const Glyph glyphs[256] = {
        { nullptr, 0, 0, 0, 0, 0 }, // 0x00
        // ...
        { bmp_41, 7, 8, 0, 0, 8 }, // 0x41 'A'
        // ...
    };

    static const int FONT_SIZE = 10;
    static const char* FONT_NAME = "EspySans";
    static const char* FONT_WEIGHT = "Regular";
    static const int FONT_BBOX_WIDTH = 15;
    static const int FONT_BBOX_HEIGHT = 16;
    static const int FONT_BBOX_X_OFFSET = -1;
    static const int FONT_BBOX_Y_OFFSET = -3;

} // namespace EspySans_10
} // namespace Fonts
```

## Metric Interpretation & Rendering Logic

This section explains how to use the generated metrics to render text correctly, assuming a standard screen coordinate system (X increases right, Y increases down).

### Glyph Metrics

*   **`width` / `height`**: The dimensions of the pixel bitmap array.
*   **`x_offset`**: The horizontal distance from the **pen position** (current cursor) to the **left edge** of the bitmap.
*   **`y_offset`**: The vertical distance from the **baseline** to the **bottom edge** of the bitmap. 
    *   *Note:* BDF uses a Cartesian system where Y increases *upwards*. A negative `y_offset` (e.g., -3) indicates the bitmap extends below the baseline (a descender).
*   **`advance`**: The horizontal distance to move the pen position *after* drawing this character to prepare for the next one.

### Global Font Metrics (`FONT_BBOX_*`)

These constants define the bounding box that encloses *all* glyphs in the font when superimposed at the origin.

*   **`FONT_BBOX_WIDTH`**: The maximum width of the font bounding box.
*   **`FONT_BBOX_HEIGHT`**: The total height of the font (max ascent + max descent). This is typically used as the **line height** or vertical step size.
*   **`FONT_BBOX_X_OFFSET`**: The leftmost X coordinate reached by any glyph (relative to origin).
*   **`FONT_BBOX_Y_OFFSET`**: The bottommost Y coordinate reached by any glyph (relative to baseline). This represents the **maximum descent**.
    *   Example: If `HEIGHT` is 16 and `Y_OFFSET` is -4, the font extends 12 pixels above the baseline and 4 pixels below.

### Rendering Formula

To render a character at a specific `(pen_x, pen_y)` where `pen_y` represents the **baseline**:

```cpp
// Calculate the top-left screen coordinate for the bitmap
int draw_x = pen_x + glyph.x_offset;

// BDF Y is 'up', Screen Y is 'down'.
// The bitmap sits 'glyph.height' pixels "tall" starting from 'glyph.y_offset'.
// Therefore, the top of the bitmap relative to baseline is: (glyph.y_offset + glyph.height)
// In screen coords (Y-down), we subtract this from the baseline.
int draw_y = pen_y - (glyph.y_offset + glyph.height);

// Draw the bitmap at (draw_x, draw_y) with size (glyph.width, glyph.height)
renderer.blit(glyph.bitmap, draw_x, draw_y, glyph.width, glyph.height);

// Advance the pen for the next character
pen_x += glyph.advance;
```

To calculate where to place the baseline given a top-left text bounding box coordinate `(box_x, box_y)`:

```cpp
// If you want to draw text starting at box_y (top of the line):
// The baseline is usually (box_y + Ascent).
// Ascent ~= FONT_BBOX_HEIGHT + FONT_BBOX_Y_OFFSET
int baseline_y = box_y + (Fonts::EspySans_10::FONT_BBOX_HEIGHT + Fonts::EspySans_10::FONT_BBOX_Y_OFFSET);
```

To use a glyph:

```cpp
#include "EspySans_10.h"

void drawChar(char c) {
    // Direct lookup (cast to unsigned to handle 128-255 range correctly)
    const auto& glyph = Fonts::EspySans_10::glyphs[static_cast<uint8_t>(c)];
    
    if (glyph.bitmap) {
        // Draw using glyph.width, glyph.height, glyph.bitmap...
    }
}
```
