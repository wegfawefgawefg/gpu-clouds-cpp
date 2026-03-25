#include "app.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include "settings_io.h"

namespace
{
constexpr std::string_view kWindowTitle = "gpu-clouds-cpp";
constexpr std::string_view kUiFontPath = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
constexpr std::string_view kSettingsPath = "cloud-settings.json";
constexpr int kInitialWindowWidth = 1600;
constexpr int kInitialWindowHeight = 960;
constexpr float kOverlayRefreshPeriod = 0.12f;
constexpr float kDegreesToRadians = 3.1415926535f / 180.0f;
constexpr std::string_view kX11DialogWindowType = "_NET_WM_WINDOW_TYPE_DIALOG";

void CenterWindowOnPrimaryDisplay(SDL_Window* window, int width, int height)
{
    const SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
    if (primaryDisplay == 0)
    {
        return;
    }

    SDL_Rect bounds;
    if (!SDL_GetDisplayBounds(primaryDisplay, &bounds))
    {
        return;
    }

    const int centeredX = bounds.x + (bounds.w - width) / 2;
    const int centeredY = bounds.y + (bounds.h - height) / 2;
    SDL_SetWindowPosition(window, centeredX, centeredY);
}
} // namespace

App::~App()
{
    Shutdown();
}

void App::Run()
{
    Initialize();

    auto previousTime = std::chrono::steady_clock::now();
    while (m_running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            HandleEvent(event);
        }

        if ((SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) != 0)
        {
            SDL_Delay(10);
            previousTime = std::chrono::steady_clock::now();
            continue;
        }

        const auto currentTime = std::chrono::steady_clock::now();
        const std::chrono::duration<float> delta = currentTime - previousTime;
        previousTime = currentTime;

        const float deltaSeconds = delta.count();
        Update(deltaSeconds);

        if (deltaSeconds > 0.0f)
        {
            const float instantFps = 1.0f / deltaSeconds;
            if (m_smoothedFps <= 0.0f)
            {
                m_smoothedFps = instantFps;
            }
            else
            {
                m_smoothedFps = m_smoothedFps * 0.94f + instantFps * 0.06f;
            }
        }

        m_overlayRefreshSeconds -= deltaSeconds;
        if (m_overlayRefreshSeconds <= 0.0f)
        {
            UpdateOverlayText();
            m_overlayRefreshSeconds = kOverlayRefreshPeriod;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        BuildUi();
        ImGui::Render();

        m_renderer.Render(BuildFrameParams(), m_overlayPixels, ImGui::GetDrawData());
    }
}

void App::Initialize()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        throw std::runtime_error("SDL_Init failed");
    }

    SDL_SetHint(SDL_HINT_X11_WINDOW_TYPE, kX11DialogWindowType.data());

    m_displayScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (m_displayScale <= 0.0f)
    {
        m_displayScale = 1.0f;
    }

    m_window = SDL_CreateWindow(
        kWindowTitle.data(),
        static_cast<int>(kInitialWindowWidth * m_displayScale),
        static_cast<int>(kInitialWindowHeight * m_displayScale),
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (m_window == nullptr)
    {
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    CenterWindowOnPrimaryDisplay(
        m_window,
        static_cast<int>(kInitialWindowWidth * m_displayScale),
        static_cast<int>(kInitialWindowHeight * m_displayScale)
    );

    if (!TTF_Init())
    {
        throw std::runtime_error("TTF_Init failed");
    }

    m_uiFont = TTF_OpenFont(kUiFontPath.data(), 20.0f * m_displayScale);
    if (m_uiFont == nullptr)
    {
        throw std::runtime_error("TTF_OpenFont failed");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ConfigureImGui();
    if (!ImGui_ImplSDL3_InitForVulkan(m_window))
    {
        throw std::runtime_error("ImGui_ImplSDL3_InitForVulkan failed");
    }

    m_renderer.Initialize(m_window);
    LoadSettings();
    SyncRendererSize();
    m_renderer.InitializeImGui();
    UpdateOverlayText();
}

void App::Shutdown()
{
    m_renderer.ShutdownImGui();
    m_renderer.Shutdown();

    ImGui_ImplSDL3_Shutdown();
    if (ImGui::GetCurrentContext() != nullptr)
    {
        ImGui::DestroyContext();
    }

    if (m_uiFont != nullptr)
    {
        TTF_CloseFont(m_uiFont);
        m_uiFont = nullptr;
    }

    TTF_Quit();

    if (m_window != nullptr)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

void App::HandleEvent(const SDL_Event& event)
{
    ImGui_ImplSDL3_ProcessEvent(&event);

    ImGuiIO& io = ImGui::GetIO();
    switch (event.type)
    {
    case SDL_EVENT_QUIT:
        m_running = false;
        break;

    case SDL_EVENT_KEY_DOWN:
        if (event.key.key == SDLK_ESCAPE)
        {
            m_running = false;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_RIGHT && !io.WantCaptureMouse)
        {
            m_captureMouse = true;
            SDL_SetWindowRelativeMouseMode(m_window, true);
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_RIGHT)
        {
            m_captureMouse = false;
            SDL_SetWindowRelativeMouseMode(m_window, false);
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (m_captureMouse)
        {
            m_camera.UpdateLook(event.motion.xrel, event.motion.yrel);
        }
        break;

    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        SyncRendererSize();
        break;

    default:
        break;
    }
}

void App::Update(float deltaSeconds)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard)
    {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        if (keys != nullptr)
        {
            m_camera.UpdateMovement(keys, deltaSeconds);
        }
    }

    m_elapsedTime += deltaSeconds * m_cloudSettings.animationRate;
}

void App::SyncRendererSize()
{
    int pixelWidth = 0;
    int pixelHeight = 0;
    if (!SDL_GetWindowSizeInPixels(m_window, &pixelWidth, &pixelHeight))
    {
        return;
    }

    m_windowWidth = static_cast<std::uint32_t>(std::max(pixelWidth, 1));
    m_windowHeight = static_cast<std::uint32_t>(std::max(pixelHeight, 1));

    const std::uint32_t divisor = ResolutionDivisor(m_cloudSettings.resolution);
    m_renderWidth = std::max(1u, m_windowWidth / divisor);
    m_renderHeight = std::max(1u, m_windowHeight / divisor);
    m_renderer.Resize(m_windowWidth, m_windowHeight, m_renderWidth, m_renderHeight);
}

void App::BuildUi()
{
    static constexpr std::array kResolutions = {
        ResolutionPreset::Full,
        ResolutionPreset::Half,
        ResolutionPreset::Quarter,
        ResolutionPreset::Eighth,
    };

    ImGui::SetNextWindowPos(
        ImVec2(20.0f * m_displayScale, 20.0f * m_displayScale),
        ImGuiCond_FirstUseEver
    );
    ImGui::SetNextWindowSize(
        ImVec2(420.0f * m_displayScale, 520.0f * m_displayScale),
        ImGuiCond_FirstUseEver
    );
    ImGui::Begin("Atmosphere Controls");

    int currentResolution = static_cast<int>(m_cloudSettings.resolution);
    if (ImGui::BeginCombo("Internal Resolution", ResolutionLabel(m_cloudSettings.resolution)))
    {
        for (const ResolutionPreset preset : kResolutions)
        {
            const int index = static_cast<int>(preset);
            const bool selected = index == currentResolution;
            if (ImGui::Selectable(ResolutionLabel(preset), selected))
            {
                m_cloudSettings.resolution = preset;
                SyncRendererSize();
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Save Settings"))
    {
        SaveSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
    {
        LoadSettings();
        SyncRendererSize();
        UpdateOverlayText();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        ResetSettings();
        SyncRendererSize();
        UpdateOverlayText();
    }

    ImGui::SeparatorText("Clouds");
    ImGui::SliderFloat("Coverage", &m_cloudSettings.coverage, 0.18f, 0.95f);
    ImGui::SliderFloat("Density", &m_cloudSettings.density, 0.15f, 1.8f);
    ImGui::SliderFloat("Absorption", &m_cloudSettings.absorption, 0.25f, 2.4f);
    ImGui::SliderFloat("Sky Light", &m_cloudSettings.ambient, 0.0f, 0.7f);
    ImGui::SliderFloat("Base Height", &m_cloudSettings.baseHeight, 600.0f, 3000.0f, "%.0f m");
    ImGui::SliderFloat("Thickness", &m_cloudSettings.thickness, 250.0f, 2200.0f, "%.0f m");
    ImGui::SliderFloat("Primary Scale", &m_cloudSettings.primaryScale, 0.00015f, 0.0014f, "%.5f");
    ImGui::SliderFloat("Detail Scale", &m_cloudSettings.detailScale, 0.0008f, 0.0080f, "%.5f");
    ImGui::SliderFloat("Detail Weight", &m_cloudSettings.detailWeight, 0.0f, 0.85f);
    ImGui::SliderFloat("Edge Softness", &m_cloudSettings.edgeSoftness, 0.03f, 0.4f);
    ImGui::SliderInt("Primary Steps", &m_cloudSettings.primarySteps, 24, 128);
    ImGui::SliderInt("Light Steps", &m_cloudSettings.lightSteps, 2, 16);

    ImGui::SeparatorText("Light");
    ImGui::SliderFloat("Sun Azimuth", &m_lightSettings.azimuthDegrees, -180.0f, 180.0f, "%.0f deg");
    ImGui::SliderFloat(
        "Sun Elevation",
        &m_lightSettings.elevationDegrees,
        -5.0f,
        85.0f,
        "%.0f deg"
    );
    ImGui::SliderFloat("Sun Intensity", &m_lightSettings.sunIntensity, 0.1f, 3.5f);
    ImGui::SliderFloat("Forward Scatter", &m_lightSettings.forwardScattering, 0.0f, 0.92f);
    ImGui::SliderFloat("Silver Lining", &m_lightSettings.silverLining, 0.0f, 1.0f);
    ImGui::ColorEdit3("Sun Color", &m_lightSettings.sunColor.x);
    ImGui::ColorEdit3("Sky Top", &m_lightSettings.skyTopColor.x);
    ImGui::ColorEdit3("Sky Horizon", &m_lightSettings.skyHorizonColor.x);

    ImGui::SeparatorText("Wind");
    ImGui::SliderFloat("Wind Speed", &m_cloudSettings.windSpeed, 0.0f, 120.0f, "%.1f m/s");
    ImGui::SliderFloat(
        "Wind Heading",
        &m_cloudSettings.windDirectionDegrees,
        -180.0f,
        180.0f,
        "%.0f deg"
    );
    ImGui::SliderFloat("Animation Rate", &m_cloudSettings.animationRate, 0.0f, 3.0f);

    ImGui::SeparatorText("Post");
    ImGui::SliderFloat("Exposure", &m_postSettings.exposure, 0.7f, 1.5f);
    ImGui::SliderFloat("Contrast", &m_postSettings.contrast, 0.8f, 1.4f);
    ImGui::SliderFloat("Bloom", &m_postSettings.bloomIntensity, 0.0f, 0.6f);
    ImGui::SliderFloat("Bloom Threshold", &m_postSettings.bloomThreshold, 0.35f, 1.2f);
    ImGui::SliderFloat("Bloom Radius", &m_postSettings.bloomRadius, 0.5f, 2.5f);
    ImGui::SliderFloat("God Rays", &m_postSettings.shaftIntensity, 0.0f, 0.6f);
    ImGui::SliderFloat("Ray Decay", &m_postSettings.shaftDecay, 0.75f, 0.98f);

    ImGui::Spacing();
    ImGui::Text("RMB to capture mouse");
    ImGui::Text("WASD + Space/Shift move camera");
    ImGui::Text("Preset file: %s", kSettingsPath.data());
    ImGui::Text(
        "Camera %.0f %.0f %.0f",
        m_camera.position.x,
        m_camera.position.y,
        m_camera.position.z
    );
    ImGui::End();
}

void App::ConfigureImGui()
{
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(m_displayScale);
    style.FontScaleDpi = m_displayScale;

    style.WindowRounding = 12.0f;
    style.FrameRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.10f, 0.13f, 0.92f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.23f, 0.38f, 0.49f, 0.78f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.31f, 0.47f, 0.60f, 0.90f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.19f, 0.34f, 0.44f, 0.88f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.47f, 0.59f, 0.95f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.16f, 0.20f, 0.88f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.93f, 0.76f, 0.50f, 0.90f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.98f, 0.86f, 0.58f, 1.00f);
}

void App::UpdateOverlayText()
{
    m_overlayPixels.fill(0);
    m_overlayWidth = 0;
    m_overlayHeight = 0;

    if (m_uiFont == nullptr)
    {
        return;
    }

    const float msPerFrame = m_smoothedFps > 0.0f ? 1000.0f / m_smoothedFps : 0.0f;
    char line[256] = {};
    std::snprintf(
        line,
        sizeof(line),
        "%.2f ms  %.0f fps  %ux%u  %s  cov %.2f  dens %.2f  sun %.0f/%.0f",
        msPerFrame,
        m_smoothedFps,
        m_renderWidth,
        m_renderHeight,
        ResolutionLabel(m_cloudSettings.resolution),
        m_cloudSettings.coverage,
        m_cloudSettings.density,
        m_lightSettings.azimuthDegrees,
        m_lightSettings.elevationDegrees
    );

    const SDL_Color color = {255, 238, 212, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(m_uiFont, line, std::strlen(line), color);
    if (surface == nullptr)
    {
        return;
    }

    SDL_Surface* rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
    SDL_DestroySurface(surface);
    if (rgbaSurface == nullptr)
    {
        return;
    }

    m_overlayWidth = std::min(static_cast<std::uint32_t>(rgbaSurface->w), kOverlayBufferWidth);
    m_overlayHeight = std::min(static_cast<std::uint32_t>(rgbaSurface->h), kOverlayBufferHeight);

    if (m_overlayWidth > 0 && m_overlayHeight > 0 && SDL_LockSurface(rgbaSurface))
    {
        const auto* sourceRows = static_cast<const std::uint8_t*>(rgbaSurface->pixels);
        for (std::uint32_t row = 0; row < m_overlayHeight; ++row)
        {
            const auto* source = sourceRows + row * rgbaSurface->pitch;
            auto* destination =
                reinterpret_cast<std::uint8_t*>(m_overlayPixels.data() + row * kOverlayBufferWidth);
            std::memcpy(destination, source, m_overlayWidth * sizeof(std::uint32_t));
        }
        SDL_UnlockSurface(rgbaSurface);
    }

    SDL_DestroySurface(rgbaSurface);
}

void App::LoadSettings()
{
    try
    {
        LoadSettingsFile(kSettingsPath, m_cloudSettings, m_lightSettings, m_postSettings);
    }
    catch (const std::exception& exception)
    {
        SDL_Log("Settings load failed: %s", exception.what());
    }
}

void App::SaveSettings() const
{
    try
    {
        SaveSettingsFile(kSettingsPath, m_cloudSettings, m_lightSettings, m_postSettings);
    }
    catch (const std::exception& exception)
    {
        SDL_Log("Settings save failed: %s", exception.what());
    }
}

void App::ResetSettings()
{
    m_cloudSettings = {};
    m_lightSettings = {};
    m_postSettings = {};
}

GpuFrameParams App::BuildFrameParams() const
{
    const Float3 forward = m_camera.GetForward();
    const Float3 right = m_camera.GetRight();
    const Float3 up = m_camera.GetUp();
    const float aspect = static_cast<float>(m_renderWidth) / static_cast<float>(m_renderHeight);
    const float halfHeight = std::tan(0.5f * m_camera.verticalFovDegrees * kDegreesToRadians);
    const float halfWidth = aspect * halfHeight;
    const Float3 sunDirection =
        DirectionFromAngles(m_lightSettings.azimuthDegrees, m_lightSettings.elevationDegrees);

    const float windRadians = m_cloudSettings.windDirectionDegrees * kDegreesToRadians;
    const Float3 windDirection = {
        std::cos(windRadians),
        0.0f,
        std::sin(windRadians),
    };
    const float sunForward = Dot(sunDirection, forward);
    const float sunRight = Dot(sunDirection, right);
    const float sunUp = Dot(sunDirection, up);
    float sunScreenX = 0.5f;
    float sunScreenY = 0.5f;
    float sunVisible = 0.0f;
    if (sunForward > 0.001f)
    {
        const float projectedX = sunRight / (sunForward * std::max(halfWidth, 0.0001f));
        const float projectedY = sunUp / (sunForward * std::max(halfHeight, 0.0001f));
        sunScreenX = projectedX * 0.5f + 0.5f;
        sunScreenY = 0.5f - projectedY * 0.5f;
        sunVisible = 1.0f;
    }

    return {
        .cameraPosition = ToFloat4(m_camera.position, m_lightSettings.silverLining),
        .cameraForward = ToFloat4(forward, 0.0f),
        .cameraRight = ToFloat4(right * halfWidth, 0.0f),
        .cameraUp = ToFloat4(up * halfHeight, 0.0f),
        .renderInfo =
            {
                static_cast<float>(m_renderWidth),
                static_cast<float>(m_renderHeight),
                static_cast<float>(m_windowWidth),
                static_cast<float>(m_windowHeight),
            },
        .timeInfo =
            {
                m_elapsedTime,
                m_elapsedTime * 60.0f,
                static_cast<float>(m_cloudSettings.primarySteps),
                static_cast<float>(m_cloudSettings.lightSteps),
            },
        .cloudShape =
            {
                m_cloudSettings.baseHeight,
                m_cloudSettings.thickness,
                m_cloudSettings.primaryScale,
                m_cloudSettings.detailScale,
            },
        .cloudMarch =
            {
                m_cloudSettings.coverage,
                m_cloudSettings.detailWeight,
                m_cloudSettings.edgeSoftness,
                m_cloudSettings.density,
            },
        .lightDirection =
            {
                sunDirection.x,
                sunDirection.y,
                sunDirection.z,
                static_cast<float>(m_cloudSettings.lightSteps),
            },
        .sunColorIntensity =
            {
                m_lightSettings.sunColor.x,
                m_lightSettings.sunColor.y,
                m_lightSettings.sunColor.z,
                m_lightSettings.sunIntensity,
            },
        .skyTopColor =
            {
                m_lightSettings.skyTopColor.x,
                m_lightSettings.skyTopColor.y,
                m_lightSettings.skyTopColor.z,
                m_cloudSettings.absorption,
            },
        .skyHorizonColor =
            {
                m_lightSettings.skyHorizonColor.x,
                m_lightSettings.skyHorizonColor.y,
                m_lightSettings.skyHorizonColor.z,
                m_cloudSettings.ambient,
            },
        .wind =
            {
                windDirection.x,
                windDirection.z,
                m_cloudSettings.windSpeed,
                m_lightSettings.forwardScattering,
            },
        .postInfo =
            {
                m_postSettings.exposure,
                m_postSettings.contrast,
                m_postSettings.bloomIntensity,
                m_postSettings.bloomThreshold,
            },
        .sunScreen =
            {
                sunScreenX,
                sunScreenY,
                sunVisible,
                m_postSettings.shaftIntensity,
            },
        .postExtra =
            {
                m_postSettings.shaftDecay,
                m_postSettings.bloomRadius,
                0.0f,
                0.0f,
            },
        .overlayInfo =
            {
                static_cast<float>(m_overlayWidth),
                static_cast<float>(m_overlayHeight),
                20.0f * m_displayScale,
                20.0f * m_displayScale,
            },
    };
}
