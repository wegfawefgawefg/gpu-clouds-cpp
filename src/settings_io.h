#pragma once

#include <string_view>

#include "cloud_types.h"

void LoadSettingsFile(
    std::string_view path,
    CloudSettings& cloudSettings,
    LightSettings& lightSettings,
    PostSettings& postSettings
);

void SaveSettingsFile(
    std::string_view path,
    const CloudSettings& cloudSettings,
    const LightSettings& lightSettings,
    const PostSettings& postSettings
);
