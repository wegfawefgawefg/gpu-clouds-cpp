#pragma once

#include <array>
#include <cstdint>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "camera.h"
#include "cloud_types.h"
#include "vulkan_renderer.h"

struct App
{
    ~App();

    void Run();

    void Initialize();
    void Shutdown();
    void HandleEvent(const SDL_Event& event);
    void Update(float deltaSeconds);
    void SyncRendererSize();
    void BuildUi();
    void ConfigureImGui();
    void UpdateOverlayText();
    GpuFrameParams BuildFrameParams() const;
    void LoadSettings();
    void SaveSettings() const;
    void ResetSettings();

    SDL_Window* m_window = nullptr;
    TTF_Font* m_uiFont = nullptr;
    bool m_running = true;
    bool m_captureMouse = false;
    std::uint32_t m_windowWidth = 0;
    std::uint32_t m_windowHeight = 0;
    std::uint32_t m_renderWidth = 0;
    std::uint32_t m_renderHeight = 0;
    std::uint32_t m_overlayWidth = 0;
    std::uint32_t m_overlayHeight = 0;
    float m_displayScale = 1.0f;
    float m_elapsedTime = 0.0f;
    float m_smoothedFps = 0.0f;
    float m_overlayRefreshSeconds = 0.0f;

    Camera m_camera;
    CloudSettings m_cloudSettings;
    LightSettings m_lightSettings;
    PostSettings m_postSettings;
    VulkanRenderer m_renderer;
    std::array<std::uint32_t, kOverlayPixelCount> m_overlayPixels = {};
};
