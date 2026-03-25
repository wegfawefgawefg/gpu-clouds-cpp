#pragma once

#include "math_types.h"

struct Camera
{
    Camera();

    void UpdateLook(float deltaX, float deltaY);
    bool UpdateMovement(const bool* keys, float deltaSeconds);
    Float3 GetForward() const;
    Float3 GetRight() const;
    Float3 GetUp() const;

    Float3 position;
    float yawDegrees = -90.0f;
    float pitchDegrees = 10.0f;
    float verticalFovDegrees = 54.0f;
    float moveSpeed = 120.0f;
    float lookSensitivity = 0.11f;
};
