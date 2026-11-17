#include "warehouse/core/math.hpp"

namespace wrl {

float wrapAngle(float a) {
    a = std::fmod(a + kPi, 2.0f * kPi);
    if (a <= 0.0f) a += 2.0f * kPi;
    return a - kPi;
}

float Rect::distanceTo(Vec2 p) const {
    const float dx = std::max({x - p.x, 0.0f, p.x - (x + w)});
    const float dy = std::max({y - p.y, 0.0f, p.y - (y + h)});
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace wrl
