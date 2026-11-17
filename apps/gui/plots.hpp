#pragma once

// Gráficas de líneas con varias series superpuestas (ImGui solo trae una por gráfica).

#include <span>
#include <vector>

#include <imgui.h>

namespace wrl::gui {

struct Series {
    const std::vector<float>* x = nullptr;
    const std::vector<float>* y = nullptr;
    ImU32 color = IM_COL32_WHITE;
};

struct PlotRange {
    float yMin = 0.0f;
    float yMax = 1.0f;
    bool autoMax = true;  ///< amplía yMax para que quepan los datos
};

/// Dibuja las series en un recuadro de `height` px de alto y todo el ancho
/// disponible. Al pasar el ratón muestra el valor de cada serie en ese instante.
void linePlot(const char* title, std::span<const Series> series, PlotRange range, float height, const char* valueFmt);

/// Barra horizontal para un valor en [-1, 1] centrada en cero.
void signedBar(const char* label, float value, ImU32 color);

}  // namespace wrl::gui
