#include "../thermal/ThermalProcessor.hpp"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <iomanip>

using namespace Thermal;

void TestConversions()
{
    std::cout << "Testing conversions..." << std::endl;
    
    ThermalProcessor processor;
    ThermalFrame frame;
    frame._width = 1;
    frame._height = 1;
    frame._data.resize(1);
    
    // Test 0C
    // Formula: Kelvin = (Celsius + 273.15) * 64
    uint16_t kelvin0C = static_cast<uint16_t>(round((0.0f + 273.15f) * 64.0f));
    frame._data[0] = kelvin0C;
    Temperature t = processor.GetTemperatureAt(frame, 0, 0);
    
    if (std::abs(t._celsius - 0.0f) >= 0.1f) {
        std::cerr << "0C Conversion failed: " << t._celsius << std::endl;
        assert(false);
    }
    if (std::abs(t._fahrenheit - 32.0f) >= 0.1f) {
        std::cerr << "32F Conversion failed: " << t._fahrenheit << std::endl;
        assert(false);
    }
    
    // Test 100C
    uint16_t kelvin100C = static_cast<uint16_t>(round((100.0f + 273.15f) * 64.0f));
    frame._data[0] = kelvin100C;
    t = processor.GetTemperatureAt(frame, 0, 0);
    
    if (std::abs(t._celsius - 100.0f) >= 0.1f) {
        std::cerr << "100C Conversion failed: " << t._celsius << std::endl;
        assert(false);
    }
    if (std::abs(t._fahrenheit - 212.0f) >= 0.1f) {
        std::cerr << "212F Conversion failed: " << t._fahrenheit << std::endl;
        assert(false);
    }
    
    std::cout << "Conversions passed." << std::endl;
}

void TestStatistics()
{
    std::cout << "Testing statistics..." << std::endl;
    ThermalProcessor processor;
    ThermalFrame output;
    
    // Create a 384x256 raw frame
    // Bottom half (192x256) is thermal
    // 384 * 256 * 2 bytes
    std::vector<uint8_t> rawFrame(384 * 256 * 2, 0);
    
    // Use uint8_t pointer to write bytes directly to control endianness
    // The processor expects Big Endian data (MSB first)
    uint8_t* thermalData = rawFrame.data() + 192 * 256 * 2;
    
    // Fill with 20C (approx 18762)
    uint16_t val20C = static_cast<uint16_t>((20.0f + 273.15f) * 64.0f);
    for (int i = 0; i < 192 * 256; ++i) {
        thermalData[i*2] = (val20C >> 8) & 0xFF;
        thermalData[i*2+1] = val20C & 0xFF;
    }
    
    // Set min at (0,0) to 0C
    uint16_t val0C = static_cast<uint16_t>((0.0f + 273.15f) * 64.0f);
    thermalData[0] = (val0C >> 8) & 0xFF;
    thermalData[1] = val0C & 0xFF;
    
    // Set max at (10,10) to 100C
    uint16_t val100C = static_cast<uint16_t>((100.0f + 273.15f) * 64.0f);
    int idx100C = 10 * 256 + 10;
    thermalData[idx100C*2] = (val100C >> 8) & 0xFF;
    thermalData[idx100C*2+1] = val100C & 0xFF;
    
    processor.ProcessFrame(rawFrame.data(), output);
    
    // Check dimensions
    assert(output._width == 256);
    assert(output._height == 192);
    
    // Check min
    // Note: The processor finds the FIRST occurrence of min/max if there are duplicates, 
    // or the last? The logic uses `val < minK`, so strictly less. So first occurrence.
    // 0C is strictly less than 20C.
    assert(output._min._row == 0);
    assert(output._min._col == 0);
    assert(std::abs(output._min._celsius - 0.0f) < 0.1f);
    
    // Check max
    assert(output._max._row == 10);
    assert(output._max._col == 10);
    assert(std::abs(output._max._celsius - 100.0f) < 0.1f);
    
    // Check avg
    // Most pixels are 20C.
    assert(std::abs(output._avg._celsius - 20.0f) < 1.0f);
    
    std::cout << "Statistics passed." << std::endl;
}

int main()
{
    try {
        TestConversions();
        TestStatistics();
        std::cout << "All tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
