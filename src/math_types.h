#pragma once

#include <cmath>
#include <cstdint>

struct Float3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct alignas(16) Float4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

inline Float3 operator+(const Float3& a, const Float3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Float3 operator-(const Float3& a, const Float3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Float3 operator*(const Float3& value, float scalar)
{
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

inline Float3 operator*(float scalar, const Float3& value)
{
    return value * scalar;
}

inline Float3 operator/(const Float3& value, float scalar)
{
    return {value.x / scalar, value.y / scalar, value.z / scalar};
}

inline Float3& operator+=(Float3& a, const Float3& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

inline float Dot(const Float3& a, const Float3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Float3 Cross(const Float3& a, const Float3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float Length(const Float3& value)
{
    return std::sqrt(Dot(value, value));
}

inline Float3 Normalize(const Float3& value)
{
    const float length = Length(value);
    if (length <= 0.0f)
    {
        return {};
    }

    return value / length;
}

inline Float4 ToFloat4(const Float3& value, float w = 0.0f)
{
    return {value.x, value.y, value.z, w};
}
