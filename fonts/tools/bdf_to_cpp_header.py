#!/usr/bin/env python3
import sys
import os
import re

def load_agl(agl_path):
    """Loads the Adobe Glyph List into a dictionary mapping name -> unicode int."""
    agl_map = {}
    if not os.path.exists(agl_path):
        sys.stderr.write(f"Error: AGL file not found at {agl_path}\n")
        sys.exit(1)
        
    try:
        with open(agl_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                
                parts = line.split(';')
                if len(parts) >= 2:
                    p0 = parts[0].strip()
                    p1 = parts[1].strip()
                    
                    # Heuristic: if p0 looks like a 4-digit hex, assume aglfn.txt (0041;A;...)
                    # Otherwise assume glyphlist.txt (A;0041)
                    if re.match(r'^[0-9A-Fa-f]{4}$', p0):
                        name = p1
                        hex_val = p0
                    else:
                        name = p0
                        # Take first hex value if sequence (e.g. "name;hex1 hex2")
                        hex_val = p1.split(' ')[0]
                        
                    try:
                        agl_map[name] = int(hex_val, 16)
                    except ValueError:
                        pass
    except Exception as e:
        sys.stderr.write(f"Error reading AGL file: {e}\n")
        sys.exit(1)
        
    return agl_map

def parse_bdf_bitmap(lines_iter, height):
    """Parses the bitmap hex data from BDF lines iterator."""
    bitmap = []
    # If height is 0, just return empty
    if height == 0:
        return bitmap

    try:
        lines_read = 0
        while lines_read < height:
            try:
                line = next(lines_iter).strip()
            except StopIteration:
                break
                
            if line == "ENDCHAR":
                break
            
            # Ensure even length for hex parsing
            if len(line) % 2 != 0:
                line += "0"
                
            for i in range(0, len(line), 2):
                byte_hex = line[i:i+2]
                bitmap.append(int(byte_hex, 16))
            lines_read += 1
            
    except Exception as e:
        sys.stderr.write(f"Error parsing bitmap: {e}\n")
        
    return bitmap

def parse_bdf_stream(stream, agl_map):
    """Parses BDF content from a stream (stdin)."""
    font_data = {
        'name': 'UnknownFont',
        'size': 0,
        'weight': 'Regular',
        'bbox': [0, 0, 0, 0], # FONTBOUNDINGBOX w h x y
        'glyphs': {} # Map codepoint -> glyph_data
    }
    
    iterator = iter(stream)
    
    current_char = None
    
    for line in iterator:
        line = line.strip()
        parts = line.split()
        
        if not parts:
            continue
            
        if line.startswith("FONT "):
            # FONT Name-Weight or just Name
            font_name_full = line[5:].strip()
            if "Bold" in font_name_full:
                 font_data['weight'] = 'Bold'
            # Clean name
            font_data['name'] = re.sub(r'[^a-zA-Z0-9_]', '', font_name_full.replace('-', '_').replace(' ', '_'))
            
        elif line.startswith("SIZE "):
            if len(parts) >= 2:
                try: font_data['size'] = int(parts[1])
                except: pass
        
        elif line.startswith("FONTBOUNDINGBOX"):
            if len(parts) >= 5:
                try:
                    font_data['bbox'] = [int(p) for p in parts[1:5]]
                except: pass
                
        elif line.startswith("STARTCHAR"):
            if len(parts) > 1:
                char_name = parts[1]
                codepoint = -1
                
                # Resolve codepoint
                if char_name in agl_map:
                    codepoint = agl_map[char_name]
                elif char_name.startswith('uni'):
                    try: codepoint = int(char_name[3:], 16)
                    except: pass
                elif char_name.startswith('u') and len(char_name) == 5:
                    try: codepoint = int(char_name[1:], 16)
                    except: pass
                
                # Filter for ISO-8859-1 (0-255)
                if codepoint != -1:
                    if codepoint <= 0xFF:
                        current_char = {
                            'name': char_name,
                            'code': codepoint,
                            'bbx': [0,0,0,0], # w, h, x, y
                            'dwidth': 0,
                            'bitmap': []
                        }
                    else:
                        # Skip chars outside Latin-1
                        current_char = None
                else:
                    # Not found in AGL
                    current_char = None
            
        elif line.startswith("BBX") and current_char:
            if len(parts) >= 5:
                current_char['bbx'] = [int(p) for p in parts[1:5]]
            
        elif line.startswith("DWIDTH") and current_char:
            if len(parts) >= 2:
                current_char['dwidth'] = int(parts[1])
            
        elif line.startswith("BITMAP") and current_char:
            h = current_char['bbx'][1]
            current_char['bitmap'] = parse_bdf_bitmap(iterator, h)
            
        elif line.startswith("ENDCHAR"):
            if current_char:
                font_data['glyphs'][current_char['code']] = current_char
            current_char = None
            
    return font_data

def generate_header(font_data):
    """Generates the C++ header to stdout."""
    name = font_data['name']
    size = font_data['size']
    weight = font_data['weight']
    bbox = font_data['bbox']
    
    # Construct namespace name
    safe_name = f"{name}"
    # Avoid appending size if already in name (e.g. EspySans10)
    if str(size) not in safe_name and size > 0:
        safe_name += f"_{size}"
    
    # Header guard
    guard = f"FONTS_{safe_name.upper()}_H"
    
    print(f"// Auto-generated from BDF. Font: {name}, Size: {size}")
    print(f"#ifndef {guard}")
    print(f"#define {guard}\n")
    print(f"#include <stdint.h>")
    print(f"#include <stddef.h>\n")
    
    print(f"namespace Fonts {{")
    print(f"namespace {safe_name} {{")
    
    print(f"\n    struct Glyph {{")
    print(f"        const uint8_t* bitmap;")
    print(f"        int width;")
    print(f"        int height;")
    print(f"        int x_offset;")
    print(f"        int y_offset;")
    print(f"        int advance;")
    print(f"    }};\n")
    
    # Generate bitmap arrays
    sorted_codes = sorted(font_data['glyphs'].keys())
    
    for code in sorted_codes:
        g = font_data['glyphs'][code]
        if not g['bitmap']:
            # For space or empty chars, don't generate empty array unless necessary
            continue
            
        hex_data = ", ".join(f"0x{b:02X}" for b in g['bitmap'])
        print(f"    static const uint8_t bmp_{code:02X}[] = {{ {hex_data} }};")
        
    print(f"\n    // Latin-1 Glyph Table (0-255)")
    print(f"    // Index corresponds to ISO-8859-1 codepoint")
    print(f"    static const Glyph glyphs[256] = {{")
    
    for i in range(256):
        if i in font_data['glyphs']:
            g = font_data['glyphs'][i]
            bmp_ptr = f"bmp_{i:02X}" if g['bitmap'] else "nullptr"
            
            # Simple char representation for comment
            # Avoid control chars
            char_display = "."
            # Allow printable Latin-1 (32-126, 160-255). Exclude controls (0-31, 127-159).
            if (32 <= i <= 126) or (i >= 160):
                char_display = chr(i)
                if char_display == '\\': char_display = "\\\\"
                elif char_display == '"': char_display = "\\\""
            
            print(f"        {{ {bmp_ptr}, {g['bbx'][0]}, {g['bbx'][1]}, {g['bbx'][2]}, {g['bbx'][3]}, {g['dwidth']} }}, // 0x{i:02X} '{char_display}'")
        else:
            print(f"        {{ nullptr, 0, 0, 0, 0, 0 }}, // 0x{i:02X}")
            
    print(f"    }};\n")
    
    print(f"    static const int FONT_SIZE = {size};")
    print(f"    static const char* FONT_NAME = \"{name}\";")
    print(f"    static const char* FONT_WEIGHT = \"{weight}\";")
    print(f"    static const int FONT_BBOX_WIDTH = {bbox[0]};")
    print(f"    static const int FONT_BBOX_HEIGHT = {bbox[1]};")
    print(f"    static const int FONT_BBOX_X_OFFSET = {bbox[2]};")
    print(f"    static const int FONT_BBOX_Y_OFFSET = {bbox[3]};\n")
    
    print(f"}} // namespace {safe_name}")
    print(f"}} // namespace Fonts\n")
    
    print(f"#endif // {guard}")

def main():
    if len(sys.argv) < 2:
        sys.stderr.write("Usage: python3 bdf_to_cpp_header.py <path_to_agl.txt> < font.bdf > output.h\n")
        sys.exit(1)
        
    agl_path = sys.argv[1]
    agl_map = load_agl(agl_path)
    
    # Force ASCII decoding with replacement for invalid bytes (like non-ASCII copyright symbols in comments)
    import io
    stdin_wrapper = io.TextIOWrapper(sys.stdin.buffer, encoding='ascii', errors='replace')

    try:
        font_data = parse_bdf_stream(stdin_wrapper, agl_map)
    except Exception as e:
        sys.stderr.write(f"Error parsing BDF stream: {e}\n")
        sys.exit(1)
    
    if not font_data['glyphs']:
        sys.stderr.write("Error: No valid glyphs parsed from input stream.\n")
        sys.exit(1)
        
    generate_header(font_data)

if __name__ == "__main__":
    main()
