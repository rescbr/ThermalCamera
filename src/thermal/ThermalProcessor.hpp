#pragma once

#include <cstdint>
#include <vector>
#include <memory>

namespace Thermal
{
    struct Temperature
    {
        uint16_t _kelvin;
        float _celsius;
        float _fahrenheit;
        int _row;
        int _col;
    };

    struct ThermalFrame
    {
        std::vector<uint16_t> _data; // Stores 16-bit Kelvin values
        int _width;
        int _height;
        Temperature _min;
        Temperature _avg;
        Temperature _max;
        Temperature _center;
    };

    class ThermalProcessor
    {
    public:
        ThermalProcessor();
        ~ThermalProcessor() = default;

        // Process a raw frame from the camera
        // rawFrame: Pointer to the raw frame buffer (expected 384 x 256 x 2 bytes)
        // output: Reference to ThermalFrame to populate
        // Returns true if successful
        bool ProcessFrame(const uint8_t* rawFrame, ThermalFrame& output);

        // Get temperature at specific coordinates
        static Temperature GetTemperatureAt(const ThermalFrame& frame, int row, int col);

        // Set rotation (0, 1=90, 2=180, 3=270)
        void SetRotation(int rotation);

    private:
        // Helper methods
        void ExtractThermalData(const uint8_t* rawFrame, ThermalFrame& output);
        void CalculateTemperatureStats(ThermalFrame& frame);
        void ApplyRotation(ThermalFrame& frame);

        // Helper for temperature lookup without bounds checking (internal use only)
        static Temperature GetTemperatureAtIndex(uint16_t kelvin, int row, int col);

        // Conversion helpers
        static float KelvinToCelsius(uint16_t kelvin);
        static float CelsiusToFahrenheit(float celsius);
        static uint16_t CelsiusToKelvin(float celsius);

        // Private members
        // std::unique_ptr<uint8_t[]> _scratchBuffer; // Removed as unused
        // bool _useCelsius; // Removed for now to avoid unused warning in Phase 2
        int _rotation;
    };
}
