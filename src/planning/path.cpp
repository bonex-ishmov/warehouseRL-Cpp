#include "warehouse/planning/path.hpp"

namespace wrl {

Path::Path(std::vector<Vec2> points) : pts_(std::move(points)) {
    cum_.reserve(pts_.size());
    float acc = 0.0f;
    for (size_t i = 0; i < pts_.size(); ++i) {
        if (i > 0) acc += distance(pts_[i - 1], pts_[i]);
        cum_.push_back(acc);
    }
}

Path::Projection Path::project(Vec2 p) const {
    if (pts_.empty()) return {};
    if (pts_.size() == 1) return {0.0f, distance(p, pts_[0])};
    Projection best{0.0f, std::numeric_limits<float>::max()};
    for (size_t i = 0; i + 1 < pts_.size(); ++i) {
        const Vec2 a = pts_[i], ab = pts_[i + 1] - a;
        const float l2 = ab.lengthSq();
        const float t = l2 > 1e-9f ? std::clamp(dot(p - a, ab) / l2, 0.0f, 1.0f) : 0.0f;
        const float d = distance(p, a + ab * t);
        if (d < best.lateral) best = {cum_[i] + t * std::sqrt(l2), d};
    }
    return best;
}

Vec2 Path::pointAt(float s) const {
    if (pts_.empty()) return {};
    if (s <= 0.0f) return pts_.front();
    if (s >= length()) return pts_.back();
    const auto it = std::upper_bound(cum_.begin(), cum_.end(), s);
    const size_t i = static_cast<size_t>(it - cum_.begin()) - 1;
    const float seg = cum_[i + 1] - cum_[i];
    const float t = seg > 1e-9f ? (s - cum_[i]) / seg : 0.0f;
    return pts_[i] + (pts_[i + 1] - pts_[i]) * t;
}

float Path::remaining(Vec2 p) const {
    const Projection pr = project(p);
    return (length() - pr.s) + pr.lateral;
}

}  // namespace wrl
