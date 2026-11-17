#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace wrl {

inline constexpr float kPi = std::numbers::pi_v<float>;
inline constexpr float kGravity = 9.81f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    constexpr bool operator==(const Vec2&) const = default;

    float length() const { return std::sqrt(x * x + y * y); }
    constexpr float lengthSq() const { return x * x + y * y; }
};

constexpr float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
constexpr float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline float distance(Vec2 a, Vec2 b) { return (a - b).length(); }
inline Vec2 normalized(Vec2 v) {
    const float len = v.length();
    return len > 1e-9f ? v / len : Vec2{};
}
inline Vec2 fromAngle(float a) { return {std::cos(a), std::sin(a)}; }

/// Rota un vector del marco mundial al marco local de un cuerpo con orientación theta.
inline Vec2 toLocal(Vec2 v, float theta) {
    const float c = std::cos(theta), s = std::sin(theta);
    return {c * v.x + s * v.y, -s * v.x + c * v.y};
}
inline Vec2 toWorld(Vec2 v, float theta) {
    const float c = std::cos(theta), s = std::sin(theta);
    return {c * v.x - s * v.y, s * v.x + c * v.y};
}

/// Normaliza un ángulo a (-pi, pi].
float wrapAngle(float a);

/// Rectángulo alineado a los ejes definido por esquina mínima y tamaño.
struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

    constexpr Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }
    constexpr bool contains(Vec2 p) const { return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h; }
    /// Distancia desde un punto al rectángulo (0 si está dentro).
    float distanceTo(Vec2 p) const;
    Rect inflated(float m) const { return {x - m, y - m, w + 2 * m, h + 2 * m}; }
};

struct Pose {
    Vec2 pos;
    float theta = 0.0f;
};

}  // namespace wrl
