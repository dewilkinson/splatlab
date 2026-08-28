#pragma once
#include "SurfelsRenderer.h"

// The 'application' shell: window/input handling and per-frame state, delegating
// all GPU work to SurfelsRenderer. Mirrors the standard Cauldron sample split
// (see AMD's GLTFSample / FidelityFX-CAS sample apps) minus scene loading.
class SurfelsSample : public CAULDRON_DX12::FrameworkWindows
{
public:
    explicit SurfelsSample(LPCSTR name);

    void OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight) override;
    void OnCreate() override;
    void OnDestroy() override;
    void OnRender() override;
    bool OnEvent(MSG msg) override;
    void OnResize(bool resizeRender) override;
    void OnUpdateDisplay() override;

private:
    void BuildUI();
    void UpdateCamera(const ImGuiIO& io);

    SurfelsRenderer*       m_pRenderer = nullptr;
    SurfelsRenderer::State m_state;

    float m_yaw = 0.6f;
    float m_pitch = 0.35f;
    float m_distance = 4.0f;
};
