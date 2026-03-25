#pragma once

#include <array>
#include <cstdint>

#include "math_types.h"

constexpr std::uint32_t kOverlayBufferWidth = 1024;
constexpr std::uint32_t kOverlayBufferHeight = 128;
constexpr std::uint32_t kOverlayPixelCount = kOverlayBufferWidth * kOverlayBufferHeight;

enum class ResolutionPreset : std::uint32_t
{
    Full = 0,
    Half = 1,
    Quarter = 2,
    Eighth = 3,
};

inline std::uint32_t ResolutionDivisor(ResolutionPreset preset)
{
    switch (preset)
    {
    case ResolutionPreset::Full:
        return 1;
    case ResolutionPreset::Half:
        return 2;
    case ResolutionPreset::Quarter:
        return 4;
    case ResolutionPreset::Eighth:
        return 8;
    }

    return 2;
}

inline const char* ResolutionLabel(ResolutionPreset preset)
{
    switch (preset)
    {
    case ResolutionPreset::Full:
        return "Full";
    case ResolutionPreset::Half:
        return "Half";
    case ResolutionPreset::Quarter:
        return "Quarter";
    case ResolutionPreset::Eighth:
        return "Eighth";
    }

    return "Half";
}

struct CloudSettings
{
    ResolutionPreset resolution = ResolutionPreset::Half;
    float coverage = 0.60f;
    float density = 0.86f;
    float absorption = 1.08f;
    float ambient = 0.32f;
    float baseHeight = 1800.0f;
    float thickness = 1200.0f;
    float primaryScale = 0.00062f;
    float detailScale = 0.0029f;
    float detailWeight = 0.34f;
    float edgeSoftness = 0.16f;
    float windSpeed = 28.0f;
    float windDirectionDegrees = 20.0f;
    float animationRate = 1.0f;
    std::int32_t primarySteps = 72;
    std::int32_t lightSteps = 8;
};

struct LightSettings
{
    float azimuthDegrees = 34.0f;
    float elevationDegrees = 58.0f;
    float sunIntensity = 1.65f;
    float forwardScattering = 0.48f;
    float silverLining = 0.28f;
    Float3 sunColor = {1.0f, 0.95f, 0.88f};
    Float3 skyTopColor = {0.24f, 0.49f, 0.92f};
    Float3 skyHorizonColor = {0.84f, 0.90f, 0.98f};
};

struct PostSettings
{
    float exposure = 1.22f;
    float contrast = 1.06f;
    float bloomIntensity = 0.65f;
    float bloomThreshold = 0.55f;
    float shaftIntensity = 2.25f;
    float shaftDecay = 0.97f;
    float bloomRadius = 2.2f;
};

struct alignas(16) GpuFrameParams
{
    Float4 cameraPosition;
    Float4 cameraForward;
    Float4 cameraRight;
    Float4 cameraUp;
    Float4 renderInfo;
    Float4 timeInfo;
    Float4 cloudShape;
    Float4 cloudMarch;
    Float4 lightDirection;
    Float4 sunColorIntensity;
    Float4 skyTopColor;
    Float4 skyHorizonColor;
    Float4 wind;
    Float4 postInfo;
    Float4 sunScreen;
    Float4 postExtra;
    Float4 overlayInfo;
};

inline Float3 DirectionFromAngles(float azimuthDegrees, float elevationDegrees)
{
    constexpr float kDegreesToRadians = 3.1415926535f / 180.0f;
    const float azimuth = azimuthDegrees * kDegreesToRadians;
    const float elevation = elevationDegrees * kDegreesToRadians;

    return Normalize({
        std::cos(azimuth) * std::cos(elevation),
        std::sin(elevation),
        std::sin(azimuth) * std::cos(elevation),
    });
}
