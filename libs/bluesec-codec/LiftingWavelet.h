// LiftingWavelet.h
// Surfels -- Copyright (c) 2026 Dave Wilkinson / Blueshell LLC
// SPDX-License-Identifier: Apache-2.0
//
// PUBLIC BUILD NOTE: the private version of this project builds its LOD pyramid with a
// second-generation lifting wavelet (predict/update steps with deadband sparsification).
// This open build substitutes a much simpler decimation scheme with the same external
// shape (same struct layout, same function signature) so the rest of the codebase needs
// no changes: every coarser level is just every other point, with normal/colour averaged
// and radius scaled up to keep surface coverage. See LiftingWavelet.cpp for the details.

#pragma once
#include <vector>
#include <cstdint>
#include "../../src/SurfelsCore/WaveletTypes.h"

namespace Surfels
{
    struct WaveletLODLevel
    {
        uint32_t level = 0;
        float geometricError = 0.0f;
        std::vector<SurfelVertex> surfels;
    };

    struct WaveletDecompositionResult
    {
        std::vector<WaveletLODLevel> lodLevels;
    };

    class LiftingWavelet
    {
    public:
        // Builds a multi-resolution LOD pyramid for one spatial chunk. See the file header
        // above: this open build uses simple decimation, not a true lifting wavelet.
        static WaveletDecompositionResult DecomposeChunk(
            const std::vector<SurfelVertex>& inputSurfels,
            uint32_t maxLODLevels = 4,
            float deadbandThreshold = 0.003f); // Unused in this build; kept for signature compatibility
    };
}
