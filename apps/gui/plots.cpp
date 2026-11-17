#include "plots.hpp"

#include <algorithm>
#include <cstdio>

namespace wrl::gui {

void linePlot(const char* title, std::span<const Series> series, PlotRange range, float height, const char* valueFmt) {
    float xMax = 1.0f, yMin = range.yMin, yMax = range.yMax;
    for (const auto& s : series) {
        if (!s.x || s.x->empty()) continue;
        xMax = std::max(xMax, s.x->back());
        if (range.autoMax)
            for (float v : *s.y) yMax = std::max(yMax, v), yMin = std::min(yMin, v);
    }
    if (yMax - yMin < 1e-6f) yMax = yMin + 1.0f;

    ImGui::TextUnformatted(title);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float width = std::max(50.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 p1(p0.x + width, p0.y + height);
    ImGui::InvisibleButton(title, ImVec2(width, height));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, p1, IM_COL32(25, 27, 32, 255), 3.0f);
    dl->AddRect(p0, p1, IM_COL32(70, 74, 82, 255), 3.0f);

    auto toScreen = [&](float x, float y) {
        return ImVec2(p0.x + x / xMax * width, p1.y - (y - yMin) / (yMax - yMin) * height);
    };
    if (yMin < 0.0f && yMax > 0.0f) {
        const float zy = toScreen(0, 0).y;
        dl->AddLine(ImVec2(p0.x, zy), ImVec2(p1.x, zy), IM_COL32(90, 90, 90, 255));
    }

    // Como mucho un punto por píxel: los episodios largos tienen miles de muestras.
    for (const auto& s : series) {
        if (!s.x || s.x->size() < 2) continue;
        const size_t n = s.x->size();
        const size_t stride = std::max<size_t>(1, n / static_cast<size_t>(width));
        ImVec2 prev = toScreen((*s.x)[0], (*s.y)[0]);
        for (size_t i = stride; i < n + stride; i += stride) {
            const size_t k = std::min(i, n - 1);
            const ImVec2 cur = toScreen((*s.x)[k], (*s.y)[k]);
            dl->AddLine(prev, cur, s.color, 1.5f);
            prev = cur;
            if (k == n - 1) break;
        }
    }

    char buf[64];
    std::snprintf(buf, sizeof buf, valueFmt, yMax);
    dl->AddText(ImVec2(p0.x + 4, p0.y + 2), IM_COL32(150, 150, 150, 255), buf);
    std::snprintf(buf, sizeof buf, valueFmt, yMin);
    dl->AddText(ImVec2(p0.x + 4, p1.y - ImGui::GetTextLineHeight() - 2), IM_COL32(150, 150, 150, 255), buf);
    std::snprintf(buf, sizeof buf, "%.0f s", xMax);
    dl->AddText(ImVec2(p1.x - ImGui::CalcTextSize(buf).x - 4, p1.y - ImGui::GetTextLineHeight() - 2),
                IM_COL32(150, 150, 150, 255), buf);

    if (ImGui::IsItemHovered()) {
        const float mx = std::clamp(ImGui::GetIO().MousePos.x, p0.x, p1.x);
        const float t = (mx - p0.x) / width * xMax;
        dl->AddLine(ImVec2(mx, p0.y), ImVec2(mx, p1.y), IM_COL32(200, 200, 200, 120));
        ImGui::BeginTooltip();
        ImGui::Text("t = %.1f s", t);
        for (const auto& s : series) {
            if (!s.x || s.x->empty() || t > s.x->back()) continue;
            const auto it = std::lower_bound(s.x->begin(), s.x->end(), t);
            const size_t k = static_cast<size_t>(std::min<std::ptrdiff_t>(it - s.x->begin(), s.x->size() - 1));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(s.color), valueFmt, (*s.y)[k]);
        }
        ImGui::EndTooltip();
    }
}

void signedBar(const char* label, float value, ImU32 color) {
    const float labelW = 70.0f;
    ImGui::TextUnformatted(label);
    ImGui::SameLine(labelW);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float width = std::max(40.0f, ImGui::GetContentRegionAvail().x - 50.0f);
    const float h = ImGui::GetTextLineHeight();
    ImGui::Dummy(ImVec2(width, h));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + width, p0.y + h);
    dl->AddRectFilled(p0, p1, IM_COL32(40, 43, 50, 255), 2.0f);
    const float mid = p0.x + width * 0.5f;
    const float end = mid + std::clamp(value, -1.0f, 1.0f) * width * 0.5f;
    dl->AddRectFilled(ImVec2(std::min(mid, end), p0.y + 2), ImVec2(std::max(mid, end), p1.y - 2), color, 2.0f);
    dl->AddLine(ImVec2(mid, p0.y), ImVec2(mid, p1.y), IM_COL32(160, 160, 160, 255));
    ImGui::SameLine();
    ImGui::Text("%+.2f", value);
}

}  // namespace wrl::gui
