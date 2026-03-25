#include "settings_io.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace
{
using json = nlohmann::json;

template <typename T>
void LoadOptionalNumber(const json& object, const char* key, T& value)
{
    const auto it = object.find(key);
    if (it != object.end() && it->is_number())
    {
        value = it->get<T>();
    }
}

void LoadOptionalColor(const json& object, const char* key, Float3& value)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_array() || it->size() != 3)
    {
        return;
    }

    value.x = (*it)[0].get<float>();
    value.y = (*it)[1].get<float>();
    value.z = (*it)[2].get<float>();
}

json ColorToJson(const Float3& value)
{
    return json::array({value.x, value.y, value.z});
}
} // namespace

void LoadSettingsFile(
    std::string_view path,
    CloudSettings& cloudSettings,
    LightSettings& lightSettings,
    PostSettings& postSettings
)
{
    std::ifstream file(path.data());
    if (!file.is_open())
    {
        return;
    }

    json root = json::parse(file, nullptr, true, true);

    if (const auto cloudIt = root.find("cloud"); cloudIt != root.end() && cloudIt->is_object())
    {
        const json& cloud = *cloudIt;
        if (const auto resolutionIt = cloud.find("resolution");
            resolutionIt != cloud.end() && resolutionIt->is_number_integer())
        {
            const int resolution = resolutionIt->get<int>();
            if (resolution >= static_cast<int>(ResolutionPreset::Full) &&
                resolution <= static_cast<int>(ResolutionPreset::Eighth))
            {
                cloudSettings.resolution = static_cast<ResolutionPreset>(resolution);
            }
        }

        LoadOptionalNumber(cloud, "coverage", cloudSettings.coverage);
        LoadOptionalNumber(cloud, "density", cloudSettings.density);
        LoadOptionalNumber(cloud, "absorption", cloudSettings.absorption);
        LoadOptionalNumber(cloud, "ambient", cloudSettings.ambient);
        LoadOptionalNumber(cloud, "sky_light", cloudSettings.ambient);
        LoadOptionalNumber(cloud, "base_height", cloudSettings.baseHeight);
        LoadOptionalNumber(cloud, "thickness", cloudSettings.thickness);
        LoadOptionalNumber(cloud, "primary_scale", cloudSettings.primaryScale);
        LoadOptionalNumber(cloud, "detail_scale", cloudSettings.detailScale);
        LoadOptionalNumber(cloud, "detail_weight", cloudSettings.detailWeight);
        LoadOptionalNumber(cloud, "edge_softness", cloudSettings.edgeSoftness);
        LoadOptionalNumber(cloud, "wind_speed", cloudSettings.windSpeed);
        LoadOptionalNumber(cloud, "wind_direction_degrees", cloudSettings.windDirectionDegrees);
        LoadOptionalNumber(cloud, "animation_rate", cloudSettings.animationRate);
        LoadOptionalNumber(cloud, "primary_steps", cloudSettings.primarySteps);
        LoadOptionalNumber(cloud, "light_steps", cloudSettings.lightSteps);
    }

    if (const auto lightIt = root.find("light"); lightIt != root.end() && lightIt->is_object())
    {
        const json& light = *lightIt;
        LoadOptionalNumber(light, "azimuth_degrees", lightSettings.azimuthDegrees);
        LoadOptionalNumber(light, "elevation_degrees", lightSettings.elevationDegrees);
        LoadOptionalNumber(light, "sun_intensity", lightSettings.sunIntensity);
        LoadOptionalNumber(light, "forward_scattering", lightSettings.forwardScattering);
        LoadOptionalNumber(light, "silver_lining", lightSettings.silverLining);
        LoadOptionalColor(light, "sun_color", lightSettings.sunColor);
        LoadOptionalColor(light, "sky_top_color", lightSettings.skyTopColor);
        LoadOptionalColor(light, "sky_horizon_color", lightSettings.skyHorizonColor);
    }

    if (const auto postIt = root.find("post"); postIt != root.end() && postIt->is_object())
    {
        const json& post = *postIt;
        LoadOptionalNumber(post, "exposure", postSettings.exposure);
        LoadOptionalNumber(post, "contrast", postSettings.contrast);
        LoadOptionalNumber(post, "bloom_intensity", postSettings.bloomIntensity);
        LoadOptionalNumber(post, "bloom_threshold", postSettings.bloomThreshold);
        LoadOptionalNumber(post, "shaft_intensity", postSettings.shaftIntensity);
        LoadOptionalNumber(post, "shaft_decay", postSettings.shaftDecay);
        LoadOptionalNumber(post, "bloom_radius", postSettings.bloomRadius);
    }
}

void SaveSettingsFile(
    std::string_view path,
    const CloudSettings& cloudSettings,
    const LightSettings& lightSettings,
    const PostSettings& postSettings
)
{
    json root;
    root["version"] = 1;
    root["cloud"] = {
        {"resolution", static_cast<int>(cloudSettings.resolution)},
        {"coverage", cloudSettings.coverage},
        {"density", cloudSettings.density},
        {"absorption", cloudSettings.absorption},
        {"sky_light", cloudSettings.ambient},
        {"base_height", cloudSettings.baseHeight},
        {"thickness", cloudSettings.thickness},
        {"primary_scale", cloudSettings.primaryScale},
        {"detail_scale", cloudSettings.detailScale},
        {"detail_weight", cloudSettings.detailWeight},
        {"edge_softness", cloudSettings.edgeSoftness},
        {"wind_speed", cloudSettings.windSpeed},
        {"wind_direction_degrees", cloudSettings.windDirectionDegrees},
        {"animation_rate", cloudSettings.animationRate},
        {"primary_steps", cloudSettings.primarySteps},
        {"light_steps", cloudSettings.lightSteps},
    };
    root["light"] = {
        {"azimuth_degrees", lightSettings.azimuthDegrees},
        {"elevation_degrees", lightSettings.elevationDegrees},
        {"sun_intensity", lightSettings.sunIntensity},
        {"forward_scattering", lightSettings.forwardScattering},
        {"silver_lining", lightSettings.silverLining},
        {"sun_color", ColorToJson(lightSettings.sunColor)},
        {"sky_top_color", ColorToJson(lightSettings.skyTopColor)},
        {"sky_horizon_color", ColorToJson(lightSettings.skyHorizonColor)},
    };
    root["post"] = {
        {"exposure", postSettings.exposure},
        {"contrast", postSettings.contrast},
        {"bloom_intensity", postSettings.bloomIntensity},
        {"bloom_threshold", postSettings.bloomThreshold},
        {"bloom_radius", postSettings.bloomRadius},
        {"shaft_intensity", postSettings.shaftIntensity},
        {"shaft_decay", postSettings.shaftDecay},
    };

    std::ofstream file(path.data());
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open settings file for writing");
    }

    file << root.dump(2) << '\n';
}
