#pragma once

#include <cstddef>
#include <cstdint>

namespace Thermal
{
    namespace Simd
    {
        // Min/max/sum statistics over a uint16_t buffer.
        // Returns the first-occurrence indices of min and max.
        void StatsMinMaxSum(
            const uint16_t* data,
            size_t pixelCount,
            uint16_t& outMin,
            size_t& outMinIndex,
            uint16_t& outMax,
            size_t& outMaxIndex,
            uint64_t& outSum);

        // Colormap mapping: out[i] = palette[clamp((clamp(in[i],minK,maxK) - minK) * 255 / range)]
        // Produces bit-identical results to the scalar reference
        // ((delta * 255) / range truncated toward zero, indices clamped to [0,255]).
        void ColormapLookup(
            const uint16_t* input,
            uint32_t* output,
            size_t pixelCount,
            uint16_t minK,
            uint16_t maxK,
            const uint32_t* palette);
    }
}
