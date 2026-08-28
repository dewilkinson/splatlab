#pragma once
#include "stdafx.h"

// GPU-side rendering for the sample: a bare Cauldron render loop (device/swapchain
// are owned by FrameworkWindows) that draws a procedural, instanced point-splat
// cloud -- a "surfel" for now is just a camera-facing billboard quad placed on a
// Fibonacci sphere, with no geometry buffers and no lighting/GI yet. It exists as
// the minimal scaffold to build an actual surfel-based GI renderer on top of.
class SurfelsRenderer
{
public:
    struct State
    {
        uint32_t surfelCount  = 4096;
        float    splatRadius  = 0.035f;
        float    sphereRadius = 1.6f;
        bool     autoRotate   = true;
        float    time         = 0.0f;

        float    camYaw      = 0.6f;
        float    camPitch    = 0.35f;
        float    camDistance = 4.0f;
        float    aspectRatio = 16.0f / 9.0f;
    };

    void OnCreate(CAULDRON_DX12::Device* pDevice, CAULDRON_DX12::SwapChain* pSwapChain);
    void OnDestroy();

    void OnCreateWindowSizeDependentResources(CAULDRON_DX12::SwapChain* pSwapChain, uint32_t width, uint32_t height);
    void OnDestroyWindowSizeDependentResources();
    void OnUpdateDisplayDependentResources(CAULDRON_DX12::SwapChain* pSwapChain);

    void OnRender(State* pState, CAULDRON_DX12::SwapChain* pSwapChain);

private:
    struct SurfelsCB
    {
        XMFLOAT4X4 viewProj;
        XMFLOAT3   camRight;
        float      radius;
        XMFLOAT3   camUp;
        float      time;
        XMFLOAT3   sphereCenter;
        float      sphereRadius;
        uint32_t   surfelCount;
        XMFLOAT3   pad;
    };

    CAULDRON_DX12::Device* m_pDevice = nullptr;

    CAULDRON_DX12::ResourceViewHeaps m_resourceViewHeaps;
    CAULDRON_DX12::UploadHeap        m_uploadHeap;
    CAULDRON_DX12::DynamicBufferRing m_constantBufferRing;
    CAULDRON_DX12::CommandListRing   m_commandListRing;
    CAULDRON_DX12::ImGUI             m_imGui;

    ID3D12RootSignature* m_pRootSignature = nullptr;
    ID3D12PipelineState* m_pPipelineState = nullptr;

    uint32_t m_width  = 0;
    uint32_t m_height = 0;
};
