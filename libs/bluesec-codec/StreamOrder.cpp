// StreamOrder.cpp
// Surfels -- Copyright (c) 2026 Dave Wilkinson / Blueshell LLC
// SPDX-License-Identifier: Apache-2.0
//
// Open-build streaming order: plain chunk order, not the private scheduler. Blocks are assigned to
// the faces of the bounding octahedron by octant (the Streaming tab's glyph needs that much), each
// face's list is simply coarse-to-fine in chunk-index order, the visible faces are served in a plain
// round-robin one block at a time, and a view-driven request ranks by frustum band and level only.
// Same header, same signatures as the private build, so nothing else in the codebase changes.

#include "StreamOrder.h"
#include <algorithm>
#include <cmath>

namespace Surfels
{
    void StreamOrder::Clear()
    {
        m_blocks.clear();
        m_faceOfHandle.clear();
        for (int f = 0; f < kOctahedronFaces; f++)
        {
            for (int l = 0; l < kMaxStreamLevels; l++) { m_faceList[f][l].clear(); m_cursor[f][l] = 0; }
            m_facing[f] = 1.0f;
            m_total[f] = m_remaining[f] = 0;
        }
        m_visibleMask = 0xFF;
        m_cursorEpoch = 0xFFFFFFFFu;
        m_recountCountdown = 0;
    }

    void StreamOrder::Build(const std::vector<Block>& blocks, const XMFLOAT3& modelCenter)
    {
        Clear();
        m_blocks = blocks;
        // Plain order: coarsest level first, then chunk index.
        std::stable_sort(m_blocks.begin(), m_blocks.end(), [](const Block& a, const Block& b)
        {
            if (a.lodLevel != b.lodLevel) return a.lodLevel > b.lodLevel;
            return a.chunkIndex < b.chunkIndex;
        });
        uint32_t maxHandle = 0;
        for (const Block& b : m_blocks) maxHandle = std::max(maxHandle, b.handle);
        m_faceOfHandle.assign(m_blocks.empty() ? 0 : (size_t)maxHandle + 1, 0);
        for (uint32_t i = 0; i < (uint32_t)m_blocks.size(); i++)
        {
            const Block& b = m_blocks[i];
            const int face = (b.center.x >= modelCenter.x ? 1 : 0) | (b.center.y >= modelCenter.y ? 2 : 0) | (b.center.z >= modelCenter.z ? 4 : 0);
            m_faceOfHandle[b.handle] = (uint8_t)face;
            m_faceList[face][std::max(0, std::min(b.lodLevel, kMaxStreamLevels - 1))].push_back(i);
        }
        for (int f = 0; f < kOctahedronFaces; f++)
        {
            uint32_t n = 0;
            for (int l = 0; l < kMaxStreamLevels; l++) n += (uint32_t)m_faceList[f][l].size();
            m_total[f] = m_remaining[f] = n;
        }
    }

    uint8_t StreamOrder::FaceOf(uint32_t handle) const
    {
        return handle < m_faceOfHandle.size() ? m_faceOfHandle[handle] : 0;
    }

    void StreamOrder::UpdateVisibility(const XMFLOAT3& eyePos, const XMFLOAT3& modelCenter)
    {
        float dx = eyePos.x - modelCenter.x, dy = eyePos.y - modelCenter.y, dz = eyePos.z - modelCenter.z;
        const float len = sqrtf(dx * dx + dy * dy + dz * dz);
        if (len > 1e-4f) { dx /= len; dy /= len; dz /= len; } else { dx = 1.0f; dy = 0.0f; dz = 0.0f; }
        m_camDir[0] = dx; m_camDir[1] = dy; m_camDir[2] = dz;
        m_visibleMask = 0;
        for (int f = 0; f < kOctahedronFaces; f++)
        {
            const float k = 0.57735027f;
            const float facing = ((f & 1) ? k : -k) * dx + ((f & 2) ? k : -k) * dy + ((f & 4) ? k : -k) * dz;
            m_facing[f] = facing;
            if (facing > 0.0f) m_visibleMask |= (uint8_t)(1u << f);
        }
        if (m_visibleMask == 0) m_visibleMask = 0xFF;
    }

    void StreamOrder::BuildLoadList(int /*targetLOD*/, size_t maxEntries, uint32_t residencyEpoch, const StateFn& state, std::vector<uint32_t>& outHandles)
    {
        outHandles.clear();
        if (m_blocks.empty()) return;
        if (m_cursorEpoch != residencyEpoch)
        {
            for (int f = 0; f < kOctahedronFaces; f++)
                for (int l = 0; l < kMaxStreamLevels; l++) m_cursor[f][l] = 0;
            m_cursorEpoch = residencyEpoch;
            m_recountCountdown = 0;
        }
        // Each face's next non-resident block, coarse levels first; visible faces first, then hidden ones.
        // The persistent cursor only moves past resident blocks; a local position walks the rest, so a
        // block offered this frame but not delivered is offered again next frame.
        size_t pos[kOctahedronFaces][kMaxStreamLevels];
        for (int f = 0; f < kOctahedronFaces; f++)
            for (int l = 0; l < kMaxStreamLevels; l++)
            {
                const auto& list = m_faceList[f][l];
                size_t& c = m_cursor[f][l];
                while (c < list.size() && state(m_blocks[list[c]].handle).resident) c++;
                pos[f][l] = c;
            }
        auto nextOf = [&](int f) -> int
        {
            for (int l = kMaxStreamLevels - 1; l >= 0; l--)
            {
                const auto& list = m_faceList[f][l];
                size_t& p = pos[f][l];
                while (p < list.size())
                {
                    const BlockState s = state(m_blocks[list[p]].handle);
                    if (!s.resident && !s.evictionPending) return (int)list[p++];
                    p++;
                }
            }
            return -1;
        };
        for (int pass = 0; pass < 2 && outHandles.size() < maxEntries; pass++)
        {
            bool progress = true;
            while (progress && outHandles.size() < maxEntries)
            {
                progress = false;
                for (int f = 0; f < kOctahedronFaces && outHandles.size() < maxEntries; f++)
                {
                    const bool visible = (m_visibleMask & (1u << f)) != 0;
                    if (visible != (pass == 0)) continue;
                    const int bi = nextOf(f);
                    if (bi >= 0) { outHandles.push_back(m_blocks[bi].handle); progress = true; }
                }
            }
        }
        if (m_recountCountdown == 0)
        {
            for (int f = 0; f < kOctahedronFaces; f++)
            {
                uint32_t n = 0;
                for (int l = 0; l < kMaxStreamLevels; l++)
                    for (uint32_t bi : m_faceList[f][l]) if (!state(m_blocks[bi].handle).resident) n++;
                m_remaining[f] = n;
            }
            m_recountCountdown = 15;
        }
        else
        {
            m_recountCountdown--;
        }
    }

    float StreamOrder::DemandPriority(int lodLevel, float /*detailScore*/, bool inFrustum, bool inNeighborBand,
                                      float /*centerFactor*/, float /*proximityFactor*/, bool prioritizeView)
    {
        // Three plain bands (in view, next to the view, elsewhere), coarser levels first within a band.
        float priority = 1000.0f;
        if (prioritizeView) priority += (inFrustum ? 200.0f : (inNeighborBand ? 100.0f : 0.0f));
        priority += (float)(lodLevel + 1);
        return priority;
    }

    // Plain round-robin: no weighting, no byte accounting.
    void StreamOrder::FaceMux::Begin(const StreamOrder& /*order*/)
    {
        for (int f = 0; f < kOctahedronFaces; f++) { m_weight[f] = 1.0f; m_taken[f] = 0.0f; }
    }

    int StreamOrder::FaceMux::Next(const bool* faceHasMore) const
    {
        int f = -1; float fewest = 0.0f;
        for (int g = 0; g < kOctahedronFaces; g++)
        {
            if (!faceHasMore[g]) continue;
            if (f < 0 || m_taken[g] < fewest) { f = g; fewest = m_taken[g]; }
        }
        return f;
    }

    void StreamOrder::FaceMux::Took(int face, size_t /*bytes*/)
    {
        if (face >= 0 && face < kOctahedronFaces) m_taken[face] += 1.0f;
    }
}
