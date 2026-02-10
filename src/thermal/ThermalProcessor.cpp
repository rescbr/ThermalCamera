#include "ThermalProcessor.hpp"
#include "../Error.hpp"
#include "../Profile.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace Thermal
{
    ThermalProcessor::ThermalProcessor() : _rotation(0)
    {
    }

    bool ThermalProcessor::ProcessFrame(const uint8_t* rawFrame, ThermalFrame& output)
    {
        PROFILE_SCOPE("ThermalProcessor::ProcessFrame");

        if (!rawFrame)
        {
            LOG_ERROR("Null raw frame provided to ThermalProcessor");
            return false;
        }

        ExtractThermalData(rawFrame, output);
        ApplyRotation(output);
        CalculateTemperatureStats(output);

        return true;
    }

    void ThermalProcessor::SetRotation(int rotation)
    {
        _rotation = rotation % 4;
        if (_rotation < 0) _rotation += 4;
    }

    void ThermalProcessor::ExtractThermalData(const uint8_t* rawFrame, ThermalFrame& output)
    {
        // Thermal data is in the bottom half of the 384x256 frame
        // Width: 256, Height: 192 (Thermal part)
        // Offset: 192 * 256 * 2 bytes = 98304 bytes
        
        const int width = 256;
        const int height = 192;
        const int offset = width * height * 2; 
        
        output._width = width;
        output._height = height;
        
        // Resize data vector if needed
        if (output._data.size() != static_cast<size_t>(width * height))
        {
            output._data.resize(width * height);
        }

        // Camera sends big-endian 16-bit values, need to swap bytes for little-endian systems
        for (int i = 0; i < width * height; ++i)
        {
            uint16_t rawValue = (rawFrame[offset + i * 2] << 8) | (rawFrame[offset + i * 2 + 1]);
            output._data[i] = rawValue;
        }
    }

    void ThermalProcessor::ApplyRotation(ThermalFrame& frame)
    {
        if (_rotation == 0) return;

        int srcW = frame._width;
        int srcH = frame._height;
        std::vector<uint16_t> temp = frame._data; // Copy original data
        
        // Swap dimensions for 90/270
        if (_rotation == 1 || _rotation == 3) {
             frame._width = srcH;
             frame._height = srcW;
        }
        // frame._data is already sized W*H, just contents change position
        
        uint16_t* dst = frame._data.data();
        const uint16_t* src = temp.data();
        
        if (_rotation == 1) // 90 degrees
        {
            for (int y = 0; y < srcH; ++y) {
                for (int x = 0; x < srcW; ++x) {
                    // New coords: x' = srcH - 1 - y, y' = x
                    // Index: y' * newW + x' = x * frame._width + (srcH - 1 - y)
                    // newW = srcH
                    dst[x * srcH + (srcH - 1 - y)] = src[y * srcW + x];
                }
            }
        }
        else if (_rotation == 2) // 180 degrees
        {
             for (int i = 0; i < srcW * srcH; ++i) {
                 dst[i] = src[srcW * srcH - 1 - i];
             }
        }
        else if (_rotation == 3) // 270 degrees
        {
            for (int y = 0; y < srcH; ++y) {
                for (int x = 0; x < srcW; ++x) {
                    // New coords: x' = y, y' = srcW - 1 - x
                    // Index: y' * newW + x' = (srcW - 1 - x) * frame._width + y
                    // newW = srcH
                    dst[(srcW - 1 - x) * srcH + y] = src[y * srcW + x];
                }
            }
        }
    }

    void ThermalProcessor::CalculateTemperatureStats(ThermalFrame& frame)
    {
        if (frame._data.empty())
        {
            return;
        }

        uint16_t minK = std::numeric_limits<uint16_t>::max();
        uint16_t maxK = std::numeric_limits<uint16_t>::min();
        uint64_t sumK = 0;
        
        int minIdx = 0;
        int maxIdx = 0;

        const size_t pixelCount = frame._data.size();
        const uint16_t* data = frame._data.data();

        // Optimized loop with unrolling (process 8 pixels at once)
        size_t i = 0;
        for (; i + 8 <= pixelCount; i += 8)
        {
            for (int j = 0; j < 8; ++j)
            {
                uint16_t val = data[i + j];
                sumK += val;
                
                if (val < minK)
                {
                    minK = val;
                    minIdx = i + j;
                }
                if (val > maxK)
                {
                    maxK = val;
                    maxIdx = i + j;
                }
            }
        }
        
        // Handle remaining pixels
        for (; i < pixelCount; ++i)
        {
            uint16_t val = data[i];
            sumK += val;
            
            if (val < minK)
            {
                minK = val;
                minIdx = i;
            }
            if (val > maxK)
            {
                maxK = val;
                maxIdx = i;
            }
        }

        uint16_t avgK = static_cast<uint16_t>(sumK / pixelCount);

        // Populate stats
        frame._min = GetTemperatureAt(frame, minIdx / frame._width, minIdx % frame._width);
        frame._max = GetTemperatureAt(frame, maxIdx / frame._width, maxIdx % frame._width);
        
        // For avg, set as a pseudo-temperature
        frame._avg._kelvin = avgK;
        frame._avg._celsius = KelvinToCelsius(avgK);
        frame._avg._fahrenheit = CelsiusToFahrenheit(frame._avg._celsius);
        frame._avg._row = -1;
        frame._avg._col = -1;

        // Center temperature
        frame._center = GetTemperatureAt(frame, frame._height / 2, frame._width / 2);
    }

    Temperature ThermalProcessor::GetTemperatureAt(const ThermalFrame& frame, int row, int col)
    {
        Temperature temp;
        temp._row = row;
        temp._col = col;

        if (row < 0 || row >= frame._height || col < 0 || col >= frame._width)
        {
            // Out of bounds
            temp._kelvin = 0;
            temp._celsius = std::numeric_limits<float>::quiet_NaN();
            temp._fahrenheit = std::numeric_limits<float>::quiet_NaN();
            // Suppress error log for common out-of-bounds (e.g. mouse move)
            return temp;
        }

        int index = row * frame._width + col;
        // Safety check for vector size
        if (index >= static_cast<int>(frame._data.size()))
        {
             temp._kelvin = 0;
             temp._celsius = std::numeric_limits<float>::quiet_NaN();
             temp._fahrenheit = std::numeric_limits<float>::quiet_NaN();
             return temp;
        }

        temp._kelvin = frame._data[index];
        temp._celsius = KelvinToCelsius(temp._kelvin);
        temp._fahrenheit = CelsiusToFahrenheit(temp._celsius);
        
        return temp;
    }

    float ThermalProcessor::KelvinToCelsius(uint16_t kelvin)
    {
        return (kelvin / 64.0f) - 273.15f;
    }

    float ThermalProcessor::CelsiusToFahrenheit(float celsius)
    {
        return (celsius * 1.8f) + 32.0f;
    }

    uint16_t ThermalProcessor::CelsiusToKelvin(float celsius)
    {
        return static_cast<uint16_t>(round((celsius + 273.15f) * 64.0f));
    }
}
