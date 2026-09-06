#include "../thermal/ThermalProcessor.hpp"
#include "../thermal/ThermalSimd.hpp"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <random>

using namespace Thermal;

void TestSimdParity()
{
    std::cout << "Testing SIMD parity..." << std::endl;

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, 65535);

    // Reference oracles mirroring the scalar implementations
    auto statsRef = [](const std::vector<uint16_t>& data,
                       uint16_t& minV, size_t& minIdx,
                       uint16_t& maxV, size_t& maxIdx, uint64_t& sum) {
        minV = 0xFFFF; maxV = 0; sum = 0; minIdx = maxIdx = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            sum += data[i];
            if (data[i] < minV) { minV = data[i]; minIdx = i; }
            if (data[i] > maxV) { maxV = data[i]; maxIdx = i; }
        }
    };

    auto cmapRef = [](const std::vector<uint16_t>& in, uint16_t minK, uint16_t maxK,
                      const uint32_t palette[256], std::vector<uint32_t>& out) {
        const uint32_t range = uint32_t(maxK) - minK;
        out.assign(in.size(), 0);
        for (size_t i = 0; i < in.size(); ++i) {
            uint16_t v = std::min(std::max(in[i], minK), maxK);
            uint32_t delta = uint32_t(v) - minK;
            uint32_t idx = std::min<uint32_t>((delta * 255) / range, 255);
            out[i] = palette[idx];
        }
    };

    uint32_t palette[256];
    for (int i = 0; i < 256; ++i) palette[i] = 0xFF000000u | (uint32_t(i) * 65793);

    // Various sizes to hit SIMD tails: 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 4096, 49152
    const size_t sizes[] = {1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 100, 4096, 49152};
    for (size_t n : sizes)
    {
        std::vector<uint16_t> data(n);
        for (auto& v : data) v = static_cast<uint16_t>(dist(rng));

        uint16_t rMin, rMax; size_t rMinI, rMaxI; uint64_t rSum;
        statsRef(data, rMin, rMinI, rMax, rMaxI, rSum);

        uint16_t sMin, sMax; size_t sMinI, sMaxI; uint64_t sSum;
        Simd::StatsMinMaxSum(data.data(), n, sMin, sMinI, sMax, sMaxI, sSum);

        assert(sMin == rMin && sMax == rMax && sSum == rSum);
        assert(sMinI == rMinI && sMaxI == rMaxI);

        // Colormap with several ranges, including extremes
        const uint16_t ranges[][2] = {
            {10000, 11000}, {0, 65535}, {40000, 40001}, {65000, 65535},
            {data.front(), static_cast<uint16_t>(data.front() + 3)}, {0, 256}, {65280, 65535}
        };
        for (auto& r : ranges)
        {
            std::vector<uint32_t> refOut, simdOut(n);
            cmapRef(data, r[0], r[1], palette, refOut);
            Simd::ColormapLookup(data.data(), simdOut.data(), n, r[0], r[1], palette);
            assert(memcmp(refOut.data(), simdOut.data(), n * 4) == 0);
        }
    }

    // Constant input (minK == maxK after normalization)
    {
        std::vector<uint16_t> data(1000, 12345);
        std::vector<uint32_t> refOut, simdOut(data.size());
        uint32_t palette2[256];
        for (int i = 0; i < 256; ++i) palette2[i] = i;
        const uint16_t m = 12345, M = 12345;
        const uint32_t range = 1; // public API normalizes maxK = minK + 1
        for (size_t i = 0; i < data.size(); ++i) {
            uint32_t delta = uint32_t(std::min(std::max(data[i], m), uint16_t(m + 1))) - m;
            refOut.push_back(palette2[std::min<uint32_t>((delta * 255) / range, 255)]);
        }
        Simd::ColormapLookup(data.data(), simdOut.data(), data.size(), m, M, palette2);
        assert(memcmp(refOut.data(), simdOut.data(), data.size() * 4) == 0);
    }

    std::cout << "SIMD parity passed." << std::endl;
}

void TestSimdBench()
{
    std::cout << "Benchmarking SIMD paths..." << std::endl;

    std::mt19937 rng(1);
    std::uniform_int_distribution<int> dist(0, 65535);
    const size_t n = 256 * 192;
    std::vector<uint16_t> data(n);
    for (auto& v : data) v = static_cast<uint16_t>(dist(rng));
    std::vector<uint32_t> out(n);

    uint32_t palette[256];
    for (int i = 0; i < 256; ++i) palette[i] = 0xFF000000u | (uint32_t(i) * 65793);

    using Clock = std::chrono::high_resolution_clock;

    // Stats (reference loop for comparison)
    {
        auto t0 = Clock::now();
        uint64_t sink = 0;
        for (int iter = 0; iter < 2000; ++iter) {
            uint16_t mn = 0xFFFF, mx = 0; uint64_t sum = 0;
            for (size_t i = 0; i < n; ++i) {
                sum += data[i];
                if (data[i] < mn) mn = data[i];
                if (data[i] > mx) mx = data[i];
            }
            sink += sum + mn + mx;
        }
        auto t1 = Clock::now();
        std::cout << "  stats scalar : "
                  << std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 2000.0
                  << " us/frame (sink " << (sink & 1) << ")" << std::endl;
    }
    {
        auto t0 = Clock::now();
        uint64_t sink = 0;
        for (int iter = 0; iter < 2000; ++iter) {
            uint16_t mn, mx; size_t mi, ma; uint64_t sum;
            Simd::StatsMinMaxSum(data.data(), n, mn, mi, mx, ma, sum);
            sink += sum + mn + mx;
        }
        auto t1 = Clock::now();
        std::cout << "  stats simd   : "
                  << std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 2000.0
                  << " us/frame (sink " << (sink & 1) << ")" << std::endl;
    }
    // Colormap (reference loop)
    {
        auto t0 = Clock::now();
        uint64_t sink = 0;
        const uint16_t minK = 12000, maxK = 16000;
        for (int iter = 0; iter < 2000; ++iter) {
            for (size_t i = 0; i < n; ++i) {
                uint16_t v = std::min(std::max(data[i], minK), maxK);
                uint32_t d = uint32_t(v) - minK;
                sink += palette[(d * 255) / (uint32_t(maxK) - minK)];
            }
        }
        auto t1 = Clock::now();
        std::cout << "  cmap  scalar : "
                  << std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 2000.0
                  << " us/frame (sink " << (sink & 1) << ")" << std::endl;
    }
    {
        auto t0 = Clock::now();
        uint64_t sink = 0;
        const uint16_t minK = 12000, maxK = 16000;
        for (int iter = 0; iter < 2000; ++iter) {
            Simd::ColormapLookup(data.data(), out.data(), n, minK, maxK, palette);
            sink += out[0];
        }
        auto t1 = Clock::now();
        std::cout << "  cmap  simd   : "
                  << std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 2000.0
                  << " us/frame (sink " << (sink & 1) << ")" << std::endl;
    }
}


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
    // The processor expects Little Endian data (standard UVC)
    uint8_t* thermalData = rawFrame.data() + 192 * 256 * 2;
    
    // Fill with 20C (approx 18762)
    uint16_t val20C = static_cast<uint16_t>((20.0f + 273.15f) * 64.0f);
    for (int i = 0; i < 192 * 256; ++i) {
        thermalData[i*2] = val20C & 0xFF;
        thermalData[i*2+1] = (val20C >> 8) & 0xFF;
    }
    
    // Set min at (0,0) to 0C
    uint16_t val0C = static_cast<uint16_t>((0.0f + 273.15f) * 64.0f);
    thermalData[0] = val0C & 0xFF;
    thermalData[1] = (val0C >> 8) & 0xFF;
    
    // Set max at (10,10) to 100C
    uint16_t val100C = static_cast<uint16_t>((100.0f + 273.15f) * 64.0f);
    int idx100C = 10 * 256 + 10;
    thermalData[idx100C*2] = val100C & 0xFF;
    thermalData[idx100C*2+1] = (val100C >> 8) & 0xFF;
    
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
        TestSimdParity();
        TestSimdBench();
        std::cout << "All tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
