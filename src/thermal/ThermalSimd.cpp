#include "ThermalSimd.hpp"

#include <cstring>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#define THERMAL_X86 1
#endif

#if defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#define THERMAL_NEON 1
#endif

namespace Thermal
{
    namespace Simd
    {
        // ------------------------------------------------------------------
        // Scalar reference implementations (non-x86 fallback + test oracle)
        // ------------------------------------------------------------------

        // First index i in [0, n) where data[i] == value (scalar fallback)
        static size_t FirstIndexOfScalar(const uint16_t* data, size_t n, uint16_t value)
        {
            for (size_t i = 0; i < n; ++i)
            {
                if (data[i] == value) return i;
            }
            return 0;
        }

#if THERMAL_NEON
        __attribute__((target("neon")))
        static size_t FirstIndexOfNeon(const uint16_t* data, size_t n, uint16_t value)
        {
            const uint16x8_t target = vdupq_n_u16(value);
            size_t i = 0;
            for (; i + 8 <= n; i += 8)
            {
                const uint16x8_t v = vld1q_u16(data + i);
                const uint16x8_t eq = vceqq_u16(v, target);
                const uint64x2_t m = vreinterpretq_u64_u16(eq);
                if ((vgetq_lane_u64(m, 0) | vgetq_lane_u64(m, 1)) != 0)
                {
                    for (int j = 0; j < 8; ++j)
                    {
                        if (data[i + j] == value) return i + j;
                    }
                }
            }
            for (; i < n; ++i)
            {
                if (data[i] == value) return i;
            }
            return 0;
        }
#elif THERMAL_X86
        __attribute__((target("sse2")))
        static size_t FirstIndexOfSse2(const uint16_t* data, size_t n, uint16_t value)
        {
            const __m128i target = _mm_set1_epi16(static_cast<short>(value));
            size_t i = 0;
            for (; i + 8 <= n; i += 8)
            {
                const __m128i eq = _mm_cmpeq_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i)), target);
                const int mask = _mm_movemask_epi8(eq);
                if (mask != 0)
                {
                    return i + (__builtin_ctz(mask) / 2); // first bit pair = first word
                }
            }
            for (; i < n; ++i)
            {
                if (data[i] == value) return i;
            }
            return 0;
        }
#endif

        static size_t FirstIndexOf(const uint16_t* data, size_t n, uint16_t value)
        {
#if THERMAL_NEON
            return FirstIndexOfNeon(data, n, value);
#elif THERMAL_X86
            return FirstIndexOfSse2(data, n, value);
#else
            return FirstIndexOfScalar(data, n, value);
#endif
        }

        static void StatsMinMaxSumScalar(
            const uint16_t* data,
            size_t pixelCount,
            uint16_t& outMin,
            size_t& outMinIndex,
            uint16_t& outMax,
            size_t& outMaxIndex,
            uint64_t& outSum)
        {
            uint16_t minV = 0xFFFF;
            uint16_t maxV = 0;
            uint64_t sum = 0;
            size_t minIdx = 0;
            size_t maxIdx = 0;

            for (size_t i = 0; i < pixelCount; ++i)
            {
                const uint16_t v = data[i];
                sum += v;
                if (v < minV) { minV = v; minIdx = i; }
                if (v > maxV) { maxV = v; maxIdx = i; }
            }

            outMin = minV;
            outMinIndex = minIdx;
            outMax = maxV;
            outMaxIndex = maxIdx;
            outSum = sum;
        }

        static void ColormapLookupScalar(
            const uint16_t* input,
            uint32_t* output,
            size_t pixelCount,
            uint16_t minK,
            uint16_t maxK,
            const uint32_t* palette)
        {
            const uint32_t range = static_cast<uint32_t>(maxK) - minK;

            for (size_t i = 0; i < pixelCount; ++i)
            {
                uint16_t val = input[i];
                if (val < minK) val = minK;
                if (val > maxK) val = maxK;

                const uint32_t delta = static_cast<uint32_t>(val) - minK;
                uint32_t index = (delta * 255) / range;
                if (index > 255) index = 255;

                output[i] = palette[index];
            }
        }

#if THERMAL_X86

        // ------------------------------------------------------------------
        // Shared helpers
        // ------------------------------------------------------------------

        // ------------------------------------------------------------------
        // SSE4.1: unsigned min/max accumulate + madd sums
        // ------------------------------------------------------------------

        __attribute__((target("sse4.1")))
        static void StatsMinMaxSumSse41(
            const uint16_t* data,
            size_t pixelCount,
            uint16_t& outMin,
            size_t& outMinIndex,
            uint16_t& outMax,
            size_t& outMaxIndex,
            uint64_t& outSum)
        {
            const __m128i kFFFF = _mm_set1_epi16(-1); // 0xFFFF
            const __m128i kZero = _mm_setzero_si128();
            const __m128i kOnes = _mm_set1_epi16(1);

            __m128i vmin = kFFFF;
            __m128i vmax = kZero;
            __m128i vsum = kZero; // 32-bit lanes via madd; no overflow below 2^32/pairSum iterations

            size_t i = 0;
            for (; i + 8 <= pixelCount; i += 8)
            {
                const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
                vmin = _mm_min_epu16(vmin, v);
                vmax = _mm_max_epu16(vmax, v);
                vsum = _mm_add_epi32(vsum, _mm_madd_epi16(v, kOnes));
            }

            uint16_t minV = static_cast<uint16_t>(_mm_extract_epi16(_mm_minpos_epu16(vmin), 0));
            const __m128i maxPos = _mm_minpos_epu16(_mm_xor_si128(vmax, kFFFF));
            uint16_t maxV = static_cast<uint16_t>(~_mm_extract_epi16(maxPos, 0));

            uint32_t sum = 0;
            for (int lane = 0; lane < 4; ++lane)
            {
                sum += static_cast<uint32_t>(_mm_extract_epi32(vsum, lane));
            }

            for (; i < pixelCount; ++i)
            {
                const uint16_t v = data[i];
                sum += v;
                if (v < minV) { minV = v; }
                if (v > maxV) { maxV = v; }
            }

            outMin = minV;
            outMax = maxV;
            outSum = sum;
            outMinIndex = FirstIndexOf(data, pixelCount, minV);
            outMaxIndex = FirstIndexOf(data, pixelCount, maxV);
        }

        // ------------------------------------------------------------------
        // AVX-512BW: 32 pixels/iteration
        // ------------------------------------------------------------------

        __attribute__((target("avx512bw,avx512f")))
        static void StatsMinMaxSumAvx512(
            const uint16_t* data,
            size_t pixelCount,
            uint16_t& outMin,
            size_t& outMinIndex,
            uint16_t& outMax,
            size_t& outMaxIndex,
            uint64_t& outSum)
        {
            const __m512i kFFFF = _mm512_set1_epi16(-1);
            const __m512i kZero = _mm512_setzero_si512();

            __m512i vmin = kFFFF;
            __m512i vmax = kZero;
            __m512i vsum = kZero; // 32-bit lanes

            size_t i = 0;
            for (; i + 32 <= pixelCount; i += 32)
            {
                const __m512i v = _mm512_loadu_si512(data + i);
                vmin = _mm512_min_epu16(vmin, v);
                vmax = _mm512_max_epu16(vmax, v);

                // 16 -> 32 bit widen (two 256 halves), accumulate
                const __m512i dwords = _mm512_add_epi32(
                    _mm512_cvtepu16_epi32(_mm512_castsi512_si256(v)),
                    _mm512_cvtepu16_epi32(_mm512_extracti64x4_epi64(v, 1)));
                vsum = _mm512_add_epi32(vsum, dwords);
            }

            // Horizontal min/max (GCC lacks reduce_min_epu16): fold with SSE minpos
            const __m128i k128FFFF = _mm_set1_epi16(-1);
            const __m128i m0 = _mm_min_epu16(_mm512_castsi512_si128(vmin), _mm512_extracti32x4_epi32(vmin, 1));
            const __m128i m1 = _mm_min_epu16(_mm512_extracti32x4_epi32(vmin, 2), _mm512_extracti32x4_epi32(vmin, 3));
            const __m128i minFold = _mm_min_epu16(m0, m1);
            const uint16_t minV = static_cast<uint16_t>(_mm_extract_epi16(_mm_minpos_epu16(minFold), 0));

            const __m128i x0 = _mm_max_epu16(_mm512_castsi512_si128(vmax), _mm512_extracti32x4_epi32(vmax, 1));
            const __m128i x1 = _mm_max_epu16(_mm512_extracti32x4_epi32(vmax, 2), _mm512_extracti32x4_epi32(vmax, 3));
            const __m128i maxFold = _mm_max_epu16(x0, x1);
            const uint16_t maxV = static_cast<uint16_t>(~_mm_extract_epi16(
                _mm_minpos_epu16(_mm_xor_si128(maxFold, k128FFFF)), 0));

            uint64_t sum = static_cast<uint64_t>(_mm512_reduce_add_epi32(vsum));
            uint16_t tailMin = minV;
            uint16_t tailMax = maxV;

            for (; i < pixelCount; ++i)
            {
                const uint16_t v = data[i];
                sum += v;
                if (v < tailMin) tailMin = v;
                if (v > tailMax) tailMax = v;
            }

            outMin = tailMin;
            outMax = tailMax;
            outSum = sum;

            // Single SIMD pass recovering BOTH first-occurrence indices
            outMinIndex = 0;
            outMaxIndex = 0;
            const uint16x8_t tMin = vdupq_n_u16(tailMin);
            const uint16x8_t tMax = vdupq_n_u16(tailMax);
            for (size_t j = 0; j < pixelCount && (outMinIndex == 0 || outMaxIndex == 0); j += 8)
            {
                const uint16x8_t v = vld1q_u16(data + j);
                const uint64x2_t eqMin = vreinterpretq_u64_u16(vceqq_u16(v, tMin));
                const uint64x2_t eqMax = vreinterpretq_u64_u16(vceqq_u16(v, tMax));

                if (outMinIndex == 0 && (vgetq_lane_u64(eqMin, 0) | vgetq_lane_u64(eqMin, 1)) != 0)
                {
                    const size_t lim = j + 8 <= pixelCount ? j + 8 : pixelCount;
                    for (size_t k = j; k < lim; ++k)
                        if (data[k] == tailMin) { outMinIndex = k; break; }
                }
                if (outMaxIndex == 0 && (vgetq_lane_u64(eqMax, 0) | vgetq_lane_u64(eqMax, 1)) != 0)
                {
                    const size_t lim = j + 8 <= pixelCount ? j + 8 : pixelCount;
                    for (size_t k = j; k < lim; ++k)
                        if (data[k] == tailMax) { outMaxIndex = k; break; }
                }
            }
        }

        // ------------------------------------------------------------------
        // Colormap: exact truncated division via float + correction.
        // All intermediates stay < 2^24 so float math is exact:
        //   prod = delta * 255          (<= 65535*255 < 2^24)
        //   q * range, (q+1) * range    (<= 256 * 65535 < 2^24)
        // ------------------------------------------------------------------

        __attribute__((target("avx2")))
        static void ColormapLookupAvx2(
            const uint16_t* input,
            uint32_t* output,
            size_t pixelCount,
            uint16_t minK,
            uint16_t maxK,
            const uint32_t* palette)
        {
            const uint32_t range = static_cast<uint32_t>(maxK) - minK;
            const __m128i vMin = _mm_set1_epi16(static_cast<short>(minK));
            const __m128i vMax = _mm_set1_epi16(static_cast<short>(maxK));
            const __m256 rangeF = _mm256_set1_ps(static_cast<float>(range));
            const __m256 invRangeF = _mm256_div_ps(_mm256_set1_ps(1.0f), rangeF);
            const __m256 k255F = _mm256_set1_ps(255.0f);
            const __m256i k255 = _mm256_set1_epi32(255);
            const __m256i kOne = _mm256_set1_epi32(1);

            size_t i = 0;
            for (; i + 8 <= pixelCount; i += 8)
            {
                __m128i w = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input + i));
                w = _mm_min_epu16(w, vMax);
                w = _mm_max_epu16(w, vMin);

                const __m256i delta = _mm256_sub_epi32(
                    _mm256_cvtepu16_epi32(w),
                    _mm256_set1_epi32(static_cast<int>(minK)));

                // prod = delta * 255 (exact)
                const __m256 prod = _mm256_mul_ps(_mm256_cvtepi32_ps(delta), k255F);

                __m256i q = _mm256_cvttps_epi32(_mm256_mul_ps(prod, invRangeF));

                // Correct rounding drift: prod / range truncated, both directions
                const __m256 qRange = _mm256_mul_ps(_mm256_cvtepi32_ps(q), rangeF);
                const __m256 gt = _mm256_cmp_ps(qRange, prod, _CMP_GT_OQ);
                q = _mm256_sub_epi32(q, _mm256_and_si256(_mm256_castps_si256(gt), kOne));
                q = _mm256_sub_epi32(q, _mm256_and_si256(_mm256_castps_si256(gt), kOne)); // covers error up to +2

                const __m256i q1 = _mm256_add_epi32(q, kOne);
                const __m256 q1Range = _mm256_mul_ps(_mm256_cvtepi32_ps(q1), rangeF);
                const __m256 le = _mm256_cmp_ps(q1Range, prod, _CMP_LE_OQ);
                q = _mm256_add_epi32(q, _mm256_and_si256(_mm256_castps_si256(le), kOne));

                q = _mm256_min_epi32(q, k255);

                const __m256i color = _mm256_i32gather_epi32(
                    reinterpret_cast<const int*>(palette), q, 4);
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(output + i), color);
            }

            for (; i < pixelCount; ++i)
            {
                uint16_t val = input[i];
                if (val < minK) val = minK;
                if (val > maxK) val = maxK;
                const uint32_t delta = static_cast<uint32_t>(val) - minK;
                uint32_t index = (delta * 255) / range;
                if (index > 255) index = 255;
                output[i] = palette[index];
            }
        }

        __attribute__((target("avx512f")))
        static void ColormapLookupAvx512(
            const uint16_t* input,
            uint32_t* output,
            size_t pixelCount,
            uint16_t minK,
            uint16_t maxK,
            const uint32_t* palette)
        {
            const uint32_t range = static_cast<uint32_t>(maxK) - minK;
            const __m512i vMin = _mm512_set1_epi16(static_cast<short>(minK));
            const __m512i vMax = _mm512_set1_epi16(static_cast<short>(maxK));
            const __m512 rangeF = _mm512_set1_ps(static_cast<float>(range));
            const __m512 invRangeF = _mm512_div_ps(_mm512_set1_ps(1.0f), rangeF);
            const __m512 k255F = _mm512_set1_ps(255.0f);

            size_t i = 0;
            for (; i + 16 <= pixelCount; i += 16)
            {
                __m256i w = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + i));
                w = _mm256_min_epu16(w, _mm256_set1_epi16(static_cast<short>(maxK)));
                w = _mm256_max_epu16(w, _mm256_set1_epi16(static_cast<short>(minK)));

                const __m512i delta = _mm512_sub_epi32(
                    _mm512_cvtepu16_epi32(w),
                    _mm512_set1_epi32(static_cast<int>(minK)));

                const __m512 prod = _mm512_mul_ps(_mm512_cvtepi32_ps(delta), k255F);

                __m512i q = _mm512_cvttps_epi32(_mm512_mul_ps(prod, invRangeF));

                // Correct rounding drift (both directions, error <= 2 ulp)
                const __mmask16 gtMask = _mm512_cmp_ps_mask(
                    _mm512_mul_ps(_mm512_cvtepi32_ps(q), rangeF), prod, _CMP_GT_OQ);
                q = _mm512_mask_sub_epi32(q, gtMask, q, _mm512_set1_epi32(1));
                q = _mm512_mask_sub_epi32(q, gtMask, q, _mm512_set1_epi32(1));

                const __m512i q1 = _mm512_add_epi32(q, _mm512_set1_epi32(1));
                const __mmask16 leMask = _mm512_cmp_ps_mask(
                    _mm512_mul_ps(_mm512_cvtepi32_ps(q1), rangeF), prod, _CMP_LE_OQ);
                q = _mm512_mask_add_epi32(q, leMask, q, _mm512_set1_epi32(1));

                q = _mm512_min_epi32(q, _mm512_set1_epi32(255));

                const __m512i color = _mm512_i32gather_epi32(q, palette, 4);
                _mm512_storeu_si512(output + i, color);
            }

            // Tail
            const size_t remaining = pixelCount - i;
            if (remaining > 0)
            {
                ColormapLookupScalar(input + i, output + i, remaining, minK, maxK, palette);
            }
        }

#endif // THERMAL_X86

#if THERMAL_NEON

        // ------------------------------------------------------------------
        // ARM NEON (baseline on aarch64 - no runtime dispatch needed).
        // No gather instruction on NEON: colormap indices are computed in
        // vectors, palette lookups stay scalar.
        // ------------------------------------------------------------------

        static void StatsMinMaxSumNeon(
            const uint16_t* data,
            size_t pixelCount,
            uint16_t& outMin,
            size_t& outMinIndex,
            uint16_t& outMax,
            size_t& outMaxIndex,
            uint64_t& outSum)
        {
            uint16x8_t vmin = vdupq_n_u16(0xFFFF);
            uint16x8_t vmax = vdupq_n_u16(0);
            uint32x4_t vsum = vdupq_n_u32(0);

            size_t i = 0;
            for (; i + 8 <= pixelCount; i += 8)
            {
                const uint16x8_t v = vld1q_u16(data + i);
                vmin = vminq_u16(vmin, v);
                vmax = vmaxq_u16(vmax, v);

                // Pairwise add-and-accumulate: sums adjacent u16 pairs into u32 lanes
                vsum = vpadalq_u16(vsum, v);
            }

            uint16_t minV = vminvq_u16(vmin);
            uint16_t maxV = vmaxvq_u16(vmax);
            uint64_t sum = static_cast<uint64_t>(vaddvq_u32(vsum));

            for (; i < pixelCount; ++i)
            {
                const uint16_t v = data[i];
                sum += v;
                if (v < minV) minV = v;
                if (v > maxV) maxV = v;
            }

            outMin = minV;
            outMax = maxV;
            outSum = sum;
            outMinIndex = FirstIndexOf(data, pixelCount, minV);
            outMaxIndex = FirstIndexOf(data, pixelCount, maxV);
        }

        static void ColormapLookupNeon(
            const uint16_t* input,
            uint32_t* output,
            size_t pixelCount,
            uint16_t minK,
            uint16_t maxK,
            const uint32_t* palette)
        {
            const uint32_t range = static_cast<uint32_t>(maxK) - minK;
            const uint16x8_t vMin = vdupq_n_u16(minK);
            const uint16x8_t vMax = vdupq_n_u16(maxK);
            const float32x4_t rangeF = vdupq_n_f32(static_cast<float>(range));
            const float32x4_t invRangeF = vdivq_f32(vdupq_n_f32(1.0f), rangeF);
            const float32x4_t k255F = vdupq_n_f32(255.0f);
            const uint32x4_t kOne = vdupq_n_u32(1);
            const uint32x4_t k255 = vdupq_n_u32(255);

            uint32_t indices[8];

            size_t i = 0;
            for (; i + 8 <= pixelCount; i += 8)
            {
                uint16x8_t w = vld1q_u16(input + i);
                w = vminq_u16(w, vMax);
                w = vmaxq_u16(w, vMin);

                const uint32x4_t dLo = vsubl_u16(vget_low_u16(w), vget_low_u16(vMin));
                const uint32x4_t dHi = vsubl_u16(vget_high_u16(w), vget_high_u16(vMin));

                // prod = delta * 255 (exact: < 2^24); vcvt rounds to nearest, corrected below
                const uint32x4_t deltas[2] = {dLo, dHi};
                for (int half = 0; half < 2; ++half)
                {
                    const uint32x4_t delta = deltas[half];

                    // prod = delta * 255 (exact: < 2^24)
                    const float32x4_t prod = vmulq_f32(vcvtq_f32_u32(delta), k255F);

                    // vcvtq_u32_f32 rounds to nearest (FPCR); correct both ways
                    uint32x4_t q = vcvtq_u32_f32(vmulq_f32(prod, invRangeF));

                    const uint32x4_t gt = vcltq_f32(prod, vmulq_f32(vcvtq_f32_u32(q), rangeF));
                    q = vsubq_u32(q, vandq_u32(gt, kOne));

                    const uint32x4_t q1 = vaddq_u32(q, kOne);
                    const uint32x4_t le = vcleq_f32(vmulq_f32(vcvtq_f32_u32(q1), rangeF), prod);
                    q = vaddq_u32(q, vandq_u32(le, kOne));

                    q = vminq_u32(q, k255);

                    vst1q_u32(indices + half * 4, q);
                }

                for (int j = 0; j < 8; ++j)
                {
                    output[i + j] = palette[indices[j]];
                }
            }

            for (; i < pixelCount; ++i)
            {
                uint16_t val = input[i];
                if (val < minK) val = minK;
                if (val > maxK) val = maxK;
                const uint32_t delta = static_cast<uint32_t>(val) - minK;
                uint32_t index = (delta * 255) / range;
                if (index > 255) index = 255;
                output[i] = palette[index];
            }
        }

#endif // THERMAL_NEON

        // ------------------------------------------------------------------
        // Public dispatch
        // ------------------------------------------------------------------

        void StatsMinMaxSum(
            const uint16_t* data,
            size_t pixelCount,
            uint16_t& outMin,
            size_t& outMinIndex,
            uint16_t& outMax,
            size_t& outMaxIndex,
            uint64_t& outSum)
        {
            if (data == nullptr || pixelCount == 0)
            {
                outMin = 0; outMinIndex = 0;
                outMax = 0; outMaxIndex = 0;
                outSum = 0;
                return;
            }

#if THERMAL_NEON
            StatsMinMaxSumNeon(data, pixelCount, outMin, outMinIndex, outMax, outMaxIndex, outSum);
            return;
#elif THERMAL_X86
            static const int tier = []() -> int {
                if (__builtin_cpu_supports("avx512bw")) return 2;
                if (__builtin_cpu_supports("sse4.1")) return 1;
                return 0;
            }();

            switch (tier)
            {
                case 2:
                    StatsMinMaxSumAvx512(data, pixelCount, outMin, outMinIndex, outMax, outMaxIndex, outSum);
                    return;
                case 1:
                    StatsMinMaxSumSse41(data, pixelCount, outMin, outMinIndex, outMax, outMaxIndex, outSum);
                    return;
                default:
                    break;
            }
#endif
            StatsMinMaxSumScalar(data, pixelCount, outMin, outMinIndex, outMax, outMaxIndex, outSum);
        }

        void ColormapLookup(
            const uint16_t* input,
            uint32_t* output,
            size_t pixelCount,
            uint16_t minK,
            uint16_t maxK,
            const uint32_t* palette)
        {
            if (minK >= maxK) maxK = minK + 1;

#if THERMAL_NEON
            ColormapLookupNeon(input, output, pixelCount, minK, maxK, palette);
            return;
#elif THERMAL_X86
            static const int tier = []() -> int {
                if (__builtin_cpu_supports("avx512f")) return 2;
                if (__builtin_cpu_supports("avx2")) return 1;
                return 0;
            }();

            switch (tier)
            {
                case 2:
                    ColormapLookupAvx512(input, output, pixelCount, minK, maxK, palette);
                    return;
                case 1:
                    ColormapLookupAvx2(input, output, pixelCount, minK, maxK, palette);
                    return;
                default:
                    break;
            }
#endif
            ColormapLookupScalar(input, output, pixelCount, minK, maxK, palette);
        }
    }
}
