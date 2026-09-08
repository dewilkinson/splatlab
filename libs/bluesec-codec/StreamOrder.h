// StreamOrder.h
// Surfels -- Copyright (c) 2026 Dave Wilkinson / Blueshell LLC
// SPDX-License-Identifier: Apache-2.0
//
// PUBLIC BUILD NOTE: the private version of this project schedules streaming with a proprietary
// ordering (which face of the bounding octahedron a block belongs to, the delivery order within a
// face, how a frame's bandwidth is dealt out across the visible faces, and how a view-driven request
// is ranked). This open build keeps the same API and a plain order in its place: blocks are grouped
// by octant, listed coarse-to-fine in chunk-index order, and served round-robin. See StreamOrder.cpp.
// Same header, same signatures as the private build, so nothing else in the codebase changes.

#pragma once
#include <vector>
#include <functional>
#include <cstdint>
#include <cstddef>
#include "../../src/SurfelsCore/WaveletTypes.h"

namespace Surfels
{
    // Faces of the bounding octahedron. Face index = (x >= 0) | (y >= 0) << 1 | (z >= 0) << 2 of a
    // block's position relative to the model centre.
    static constexpr int kOctahedronFaces = 8;
    // Deepest LOD hierarchy the per-face lists are split by.
    static constexpr int kMaxStreamLevels = 8;

    class StreamOrder
    {
    public:
        // One streaming block as the scheduler sees it. 'handle' is the caller's own id for the
        // block (typically its index in the caller's block array) and is what every result refers to.
        struct Block
        {
            uint32_t handle      = 0;
            int      lodLevel    = 0;
            uint32_t chunkIndex  = 0;
            XMFLOAT3 center      = { 0.0f, 0.0f, 0.0f };
            uint32_t byteSize    = 0;
            float    detailScore = 0.0f; // 0..1 from DetailGrid::SampleBox over the block's bounds
        };

        // Live residency of a block, asked of the caller while a load list is built.
        struct BlockState
        {
            bool resident        = false;
            bool evictionPending = false;
        };
        using StateFn = std::function<BlockState(uint32_t handle)>;

        // Builds the order for a model's blocks (any input order). Assigns every block a face and
        // prepares the per-face delivery lists. Handles must be dense: 0 .. blocks.size() - 1.
        void   Build(const std::vector<Block>& blocks, const XMFLOAT3& modelCenter);
        void   Clear();
        size_t BlockCount() const { return m_blocks.size(); }

        // Face (0..7) the block was assigned by Build().
        uint8_t FaceOf(uint32_t handle) const;

        // Recomputes which faces the camera can see and how directly each faces it.
        void         UpdateVisibility(const XMFLOAT3& eyePos, const XMFLOAT3& modelCenter);
        uint8_t      VisibleFaceMask() const     { return m_visibleMask; }     // Bit f = face f visible
        float        FaceFacing(int face) const  { return m_facing[face]; }    // -1..1, 1 = squarely facing the camera
        const float* CameraDirection() const     { return m_camDir; }          // Unit vector, model centre -> eye

        // Fills outHandles with up to maxEntries blocks to deliver this frame, in delivery order,
        // across the faces according to the current visibility. targetLOD is the level the view
        // renders at present. residencyEpoch must change whenever any block stops being resident
        // (evict, reset, decay); 'state' reports each block's live residency.
        void BuildLoadList(int targetLOD, size_t maxEntries, uint32_t residencyEpoch, const StateFn& state, std::vector<uint32_t>& outHandles);

        // Per-face block counts for the UI (remaining = not yet resident; refreshed by BuildLoadList).
        uint32_t FaceTotal(int face) const     { return m_total[face]; }
        uint32_t FaceRemaining(int face) const { return m_remaining[face]; }

        // Rank of a view-driven request for a block that is neither a silhouette edge nor part of the
        // coarse bootstrap envelope (the caller ranks those two tiers itself, above everything here).
        // centerFactor: 1 at the centre of the view falling to 0 at 90 degrees off-axis; proximityFactor:
        // 1 nearest the camera falling to 0 at the far range. Higher = delivered sooner.
        static float DemandPriority(int lodLevel, float detailScore, bool inFrustum, bool inNeighborBand,
                                    float centerFactor, float proximityFactor, bool prioritizeView);

        // Deals one frame's deliveries out across the faces. Begin() once per pass, then repeatedly
        // Next() (which face to serve now; -1 when no face has anything left) and Took() with the
        // bytes actually delivered from that face.
        class FaceMux
        {
        public:
            void Begin(const StreamOrder& order);
            int  Next(const bool* faceHasMore) const;
            void Took(int face, size_t bytes);
        private:
            float m_weight[kOctahedronFaces] = {};
            float m_taken[kOctahedronFaces]  = {};
        };

    private:
        std::vector<Block>    m_blocks;                                    // In the scheduler's own order
        std::vector<uint8_t>  m_faceOfHandle;                              // Indexed by handle
        std::vector<uint32_t> m_faceList[kOctahedronFaces][kMaxStreamLevels]; // Indices into m_blocks
        size_t   m_cursor[kOctahedronFaces][kMaxStreamLevels] = {};
        uint32_t m_cursorEpoch      = 0xFFFFFFFFu;
        uint32_t m_recountCountdown = 0;
        float    m_facing[kOctahedronFaces] = {};
        uint8_t  m_visibleMask = 0xFF;
        float    m_camDir[3] = { 1.0f, 0.0f, 0.0f };
        uint32_t m_total[kOctahedronFaces] = {};
        uint32_t m_remaining[kOctahedronFaces] = {};
    };
}
