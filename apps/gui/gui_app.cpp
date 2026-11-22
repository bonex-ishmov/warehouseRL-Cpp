#include "gui_app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <imgui-SFML.h>
#include <imgui.h>

#include "../policy_factory.hpp"
#include "plots.hpp"

namespace wrl::gui {
namespace fs = std::filesystem;

namespace {

// Sin amarillo: el robot se pinta de amarillo mientras recarga.
const sf::Color kAgentColors[] = {sf::Color(66, 135, 245),  sf::Color(245, 130, 48), sf::Color(60, 190, 90),
                                  sf::Color(190, 90, 220),  sf::Color(70, 210, 220), sf::Color(240, 90, 130)};

struct KindOption {
    const char* kind;
    const char* label;
};
const KindOption kKinds[] = {{"sac", "SAC (modelo entrenado)"},
                             {"dwa", "A* + DWA (línea base)"},
                             {"random", "Aleatoria"},
                             {"manual", "Manual (teclado)"}};

ImU32 toU32(sf::Color c, std::uint8_t alpha = 255) { return IM_COL32(c.r, c.g, c.b, alpha); }

ImVec4 toVec4(sf::Color c) { return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f); }

const char* phaseLabel(Phase p) {
    switch (p) {
        case Phase::ToPickup: return "hacia recogida";
        case Phase::ToDropoff: return "hacia entrega";
        case Phase::ToCharger: return "hacia cargador";
        case Phase::Idle: return "sin tarea";
    }
    return "";
}

const char* reasonLabel(EndReason r) {
    switch (r) {
        case EndReason::None: return "en curso";
        case EndReason::AllDelivered: return "todas las tareas entregadas";
        case EndReason::BatteryDepleted: return "batería agotada";
        case EndReason::TimeLimit: return "tiempo máximo";
        case EndReason::WorkerCollision: return "choque grave con un operario";
        case EndReason::CollisionLimit: return "demasiadas colisiones";
        case EndReason::Stalled: return "sin progreso";
    }
    return "";
}

const char* priorityLabel(Priority p) {
    switch (p) {
        case Priority::Low: return "baja";
        case Priority::Medium: return "media";
        case Priority::High: return "alta";
        case Priority::Critical: return "crítica";
    }
    return "";
}

/// Frase que resume qué está haciendo el robot ahora mismo.
std::string statusLine(const WarehouseEnv& env) {
    char buf[160];
    const auto& d = env.decision();
    const double now = env.time();
    auto deadlineText = [&](const Task& t) -> std::string {
        if (!t.hasDeadline()) return "";
        const double left = t.deadline - now;
        char b[48];
        if (left >= 0) std::snprintf(b, sizeof b, " · vence en %.0f s", left);
        else std::snprintf(b, sizeof b, " · vencida hace %.0f s", -left);
        return b;
    };
    if (env.charging()) {
        std::snprintf(buf, sizeof buf, "Recargando · batería %.0f%%", 100.0 * env.batteryFraction());
        return buf;
    }
    switch (d.phase) {
        case Phase::ToPickup: {
            const Task& t = env.tasks().get(d.taskId);
            std::snprintf(buf, sizeof buf, "Va a recoger #%d (prioridad %s)%s", t.id, priorityLabel(t.priority), deadlineText(t).c_str());
            return buf;
        }
        case Phase::ToDropoff: {
            const Task& t = env.tasks().get(d.taskId);
            std::snprintf(buf, sizeof buf, "Lleva #%d al destino (%.0f kg, %s)%s", t.id, static_cast<double>(env.robot().payload()),
                          priorityLabel(t.priority), deadlineText(t).c_str());
            return buf;
        }
        case Phase::ToCharger:
            std::snprintf(buf, sizeof buf, "Va al cargador · batería %.0f%%", 100.0 * env.batteryFraction());
            return buf;
        case Phase::Idle: return "Sin tareas disponibles: espera";
    }
    return "";
}

struct Badge {
    const char* text;
    ImU32 color;
};

/// Avisos del momento: lo que conviene mirar en la vista del agente.
std::vector<Badge> liveBadges(const AgentRunner& a) {
    std::vector<Badge> out;
    const auto& env = a.env();
    const auto& h = a.history();
    if (!h.flags.empty() && (h.flags.back() & kSlipping)) out.push_back({"DERRAPANDO", IM_COL32(200, 50, 50, 230)});
    if (env.loadStress() > 0.7f) out.push_back({"CARGA INESTABLE", IM_COL32(220, 110, 20, 230)});
    if (env.batteryFraction() < 0.2f && !env.charging()) out.push_back({"BATERÍA BAJA", IM_COL32(200, 160, 20, 230)});
    if (env.map().inRestricted(env.robot().position())) out.push_back({"EN ZONA RESTRINGIDA", IM_COL32(200, 40, 40, 230)});
    if (!env.done() && std::abs(env.measuredSpeed()) < 0.03f && std::abs(env.measuredAngular()) < 0.05f && !env.charging() &&
        env.decision().phase != Phase::Idle)
        out.push_back({"DETENIDO", IM_COL32(100, 100, 110, 230)});
    return out;
}

ImU32 eventColor(const EventMark& e) {
    using namespace palette;
    switch (e.kind) {
        case EventKind::Collision: return toU32(kSeverity[std::clamp(e.detail, 0, 2)]);
        case EventKind::Delivered: return toU32(kDelivered);
        case EventKind::DeliveredLate: return toU32(kLate);
        case EventKind::Pickup: return toU32(kPickupEvent);
        case EventKind::LoadDrop: return toU32(kLoadDrop);
        case EventKind::RechargeStart: return toU32(kCharger);
        case EventKind::RestrictedEntry: return toU32(kRestricted);
        case EventKind::CriticalOverdue: return IM_COL32(255, 60, 60, 255);
    }
    return IM_COL32_WHITE;
}

std::string eventText(const EventMark& e) {
    static const char* kSeverityName[3] = {"roce", "choque leve", "choque grave"};
    if (e.kind == EventKind::Collision) return kSeverityName[std::clamp(e.detail, 0, 2)];
    std::string s = describe(e.kind);
    if (e.detail >= 0 && e.kind != EventKind::RechargeStart) s += " #" + std::to_string(e.detail);
    return s;
}

/// Icono de un suceso dibujado con ImGui, igual que en el mapa.
void eventIcon(ImDrawList* dl, ImVec2 c, float s, const EventMark& e) {
    const ImU32 col = eventColor(e);
    switch (e.kind) {
        case EventKind::Collision:
            dl->AddLine(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), col, 2.5f);
            dl->AddLine(ImVec2(c.x - s, c.y + s), ImVec2(c.x + s, c.y - s), col, 2.5f);
            if (e.detail == 2) dl->AddCircle(c, s * 1.5f, col, 12, 1.5f);
            break;
        case EventKind::Delivered:
        case EventKind::DeliveredLate:
            dl->AddCircleFilled(c, s * 1.2f, col);
            dl->AddLine(ImVec2(c.x - s * 0.6f, c.y), ImVec2(c.x - s * 0.1f, c.y + s * 0.5f), IM_COL32_WHITE, 1.8f);
            dl->AddLine(ImVec2(c.x - s * 0.1f, c.y + s * 0.5f), ImVec2(c.x + s * 0.7f, c.y - s * 0.6f), IM_COL32_WHITE, 1.8f);
            break;
        case EventKind::Pickup:
            dl->AddTriangleFilled(ImVec2(c.x, c.y - s * 1.2f), ImVec2(c.x - s, c.y + s * 0.8f), ImVec2(c.x + s, c.y + s * 0.8f), col);
            break;
        case EventKind::LoadDrop:
            dl->AddTriangleFilled(ImVec2(c.x, c.y + s * 1.2f), ImVec2(c.x - s * 1.1f, c.y - s * 0.8f), ImVec2(c.x + s * 1.1f, c.y - s * 0.8f), col);
            break;
        case EventKind::RechargeStart:
            dl->AddTriangleFilled(ImVec2(c.x + s * 0.3f, c.y - s * 1.2f), ImVec2(c.x - s * 0.5f, c.y + s * 0.1f), ImVec2(c.x + s * 0.2f, c.y + s * 0.1f), col);
            dl->AddTriangleFilled(ImVec2(c.x - s * 0.2f, c.y - s * 0.1f), ImVec2(c.x + s * 0.5f, c.y - s * 0.1f), ImVec2(c.x - s * 0.3f, c.y + s * 1.2f), col);
            break;
        case EventKind::RestrictedEntry:
            dl->AddCircle(c, s, col, 12, 2.0f);
            dl->AddLine(ImVec2(c.x - s * 0.7f, c.y + s * 0.7f), ImVec2(c.x + s * 0.7f, c.y - s * 0.7f), col, 2.0f);
            break;
        case EventKind::CriticalOverdue:
            dl->AddRectFilled(ImVec2(c.x - s * 0.25f, c.y - s * 1.2f), ImVec2(c.x + s * 0.25f, c.y + s * 0.4f), col);
            dl->AddRectFilled(ImVec2(c.x - s * 0.25f, c.y + s * 0.7f), ImVec2(c.x + s * 0.25f, c.y + s * 1.2f), col);
            break;
    }
}

void sectionTitle(const char* text) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.75f, 1.0f, 1.0f));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::Separator();
}

constexpr ImGuiWindowFlags kPanelFlags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

}  // namespace

GuiApp::GuiApp(sf::RenderWindow& window, Session& session, fs::path root)
    : window_(window), session_(session), root_(std::move(root)) {
    scenarios_ = listScenarios(root_ / "configs" / "scenarios");
    try {
        generator_ = std::make_unique<ScenarioGenerator>(loadScenario(root_ / "configs" / "robot" / "default.json"));
    } catch (const std::exception& e) {
        message_ = std::string("Generador no disponible: ") + e.what();
    }
    refreshModels();
    for (size_t i = 0; i < scenarios_.size(); ++i)
        if (std::error_code ec; fs::equivalent(scenarios_[i].path, session_.scenarioPath(), ec)) scenarioIndex_ = static_cast<int>(i);
    if (!hasSacSupport()) newKind_ = 1;
}

void GuiApp::refreshModels() {
    models_ = listModels({root_ / "models", root_ / "runs"}, root_);
    // El modelo entregado primero: es el que se quiere ver casi siempre.
    std::stable_partition(models_.begin(), models_.end(),
                          [](const CatalogEntry& e) { return e.path.filename() == "sac_final.pt" && e.label.rfind("models", 0) == 0; });
    newModel_ = std::clamp(newModel_, 0, std::max(0, static_cast<int>(models_.size()) - 1));
}

sf::Color GuiApp::agentColor(size_t i) const { return kAgentColors[i % std::size(kAgentColors)]; }

bool GuiApp::hasManualAgent() const {
    for (size_t i = 0; i < session_.agentCount(); ++i)
        if (session_.agent(i).spec().kind == "manual") return true;
    return false;
}

// --- Entrada -----------------------------------------------------------------

void GuiApp::processEvent(const sf::Event& event) {
    const auto& io = ImGui::GetIO();
    if (const auto* k = event.getIf<sf::Event::KeyPressed>()) {
        if (!io.WantCaptureKeyboard) handleKey(k->code);
    }
}

void GuiApp::handleKey(sf::Keyboard::Key key) {
    using Key = sf::Keyboard::Key;
    switch (key) {
        case Key::Space: session_.setPaused(!session_.paused()); break;
        case Key::N: session_.requestStep(); break;
        case Key::R: session_.reset(session_.seed()); break;
        case Key::Enter: session_.nextEpisode(); break;
        case Key::Equal:
        case Key::Add: session_.setSpeed(session_.speed() * 2.0); break;
        case Key::Hyphen:
        case Key::Subtract: session_.setSpeed(session_.speed() * 0.5); break;
        case Key::L: view_.lidar = !view_.lidar; break;
        case Key::P: view_.path = !view_.path; break;
        case Key::T: view_.trail = !view_.trail; break;
        case Key::V:
            view_.trailMode = view_.trailMode == TrailMode::State ? TrailMode::Speed : TrailMode::State;
            break;
        case Key::I: view_.intent = !view_.intent; break;
        case Key::E: view_.events = !view_.events; break;
        case Key::K: view_.labels = !view_.labels; break;
        case Key::C: view_.follow = !view_.follow; break;
        case Key::F1: showHelp_ = !showHelp_; break;
        default: break;
    }
}

void GuiApp::updateManualControl() {
    if (!hasManualAgent()) return;
    Action a;
    if (window_.hasFocus() && !ImGui::GetIO().WantCaptureKeyboard) {
        using Key = sf::Keyboard::Key;
        auto down = [](Key k) { return sf::Keyboard::isKeyPressed(k); };
        const bool up = down(Key::W) || down(Key::Up), back = down(Key::S) || down(Key::Down);
        const bool left = down(Key::A) || down(Key::Left), right = down(Key::D) || down(Key::Right);
        a.linear = up ? 1.0f : back ? -1.0f : 0.0f;
        a.angular = left ? 0.8f : right ? -0.8f : 0.0f;
    }
    session_.setManualAction(a);
}

// --- Cuadro ------------------------------------------------------------------

void GuiApp::frame() {
    updateManualControl();
    drawMenuBar();

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float top = ImGui::GetFrameHeight();
    const float h = display.y - top;
    const float left = std::clamp(display.x * 0.2f, 290.0f, 360.0f);
    const float right = std::clamp(display.x * 0.24f, 320.0f, 430.0f);
    drawControlPanel(0, top, left, h);
    drawViewports(left, top, display.x - left - right, h);
    drawMetricsPanel(display.x - right, top, right, h);

    if (showHelp_) {
        ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::Begin("Atajos de teclado", &showHelp_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted(
            "Espacio   pausar / reanudar\n"
            "N         avanzar un paso (en pausa)\n"
            "R         reiniciar el episodio (misma semilla)\n"
            "Enter     siguiente semilla\n"
            "+ / -     velocidad x2 / x0.5\n"
            "L P T C   LiDAR, ruta, recorrido, seguir al robot\n"
            "V         recorrido coloreado por estado / por velocidad\n"
            "I E K     intención, sucesos, etiquetas del mapa\n"
            "Rueda     zoom sobre una vista;  arrastrar = mover la cámara\n"
            "Doble clic  centrar la cámara\n"
            "W A S D / flechas   conducir los agentes manuales\n"
            "F1        esta ayuda");
        ImGui::End();
    }
}

void GuiApp::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("Archivo")) {
        if (ImGui::MenuItem("Buscar modelos de nuevo")) refreshModels();
        ImGui::Separator();
        if (ImGui::MenuItem("Salir", "Esc")) window_.close();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Ver")) {
        ImGui::MenuItem("LiDAR", "L", &view_.lidar);
        ImGui::MenuItem("Ruta planificada", "P", &view_.path);
        ImGui::MenuItem("Recorrido", "T", &view_.trail);
        ImGui::MenuItem("Intención de la política", "I", &view_.intent);
        ImGui::MenuItem("Sucesos en el mapa", "E", &view_.events);
        ImGui::MenuItem("Etiquetas", "K", &view_.labels);
        ImGui::MenuItem("Seguir al robot", "C", &view_.follow);
        if (ImGui::MenuItem("Restablecer cámara")) view_.zoom = 1.0f, view_.pan = {};
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Ayuda")) {
        ImGui::MenuItem("Atajos de teclado", "F1", &showHelp_);
        ImGui::EndMenu();
    }
    ImGui::SameLine(ImGui::GetWindowWidth() - 330);
    ImGui::TextDisabled("WarehouseRL · %s · %.0f FPS", session_.scenario().name.c_str(), ImGui::GetIO().Framerate);
    ImGui::EndMainMenuBar();
}

// --- Panel izquierdo: escenario, agentes, reproducción ------------------------

void GuiApp::drawControlPanel(float x, float y, float w, float h) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::Begin("Configuración", nullptr, kPanelFlags);
    scenarioSection();
    agentsSection();
    playbackSection();
    viewSection();
    if (!message_.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
        ImGui::TextWrapped("%s", message_.c_str());
        ImGui::PopStyleColor();
        if (ImGui::SmallButton("Cerrar aviso")) message_.clear();
    }
    ImGui::End();
}

void GuiApp::selectScenario(int index) {
    try {
        session_.setScenario(scenarios_[static_cast<size_t>(index)].path);
        scenarioIndex_ = index;
        message_.clear();
    } catch (const std::exception& e) {
        message_ = std::string("No se pudo cargar el escenario: ") + e.what();
    }
}

void GuiApp::scenarioSection() {
    sectionTitle("Escenario");
    if (session_.scenarioPath().empty()) scenarioIndex_ = -1;  // escenario generado, no de un archivo
    const std::string generatedLabel = session_.hasScenarioProvider() ? "Generado (uno nuevo por episodio)" : "Generado";
    const char* current = scenarioIndex_ >= 0 ? scenarios_[static_cast<size_t>(scenarioIndex_)].label.c_str()
                          : session_.scenario().name == "Generado" ? generatedLabel.c_str()
                                                                   : session_.scenario().name.c_str();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##escenario", current)) {
        for (int i = 0; i < static_cast<int>(scenarios_.size()); ++i) {
            const auto& s = scenarios_[static_cast<size_t>(i)];
            if (ImGui::Selectable(s.label.c_str(), i == scenarioIndex_)) selectScenario(i);
            if (ImGui::IsItemHovered() && !s.description.empty()) ImGui::SetTooltip("%s", s.description.c_str());
        }
        ImGui::EndCombo();
    }
    const auto& cfg = session_.scenario();
    if (!cfg.description.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::TextWrapped("%s", cfg.description.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    const float mw = cfg.map.procedural ? cfg.map.proc.width : cfg.map.width;
    const float mh = cfg.map.procedural ? cfg.map.proc.height : cfg.map.height;
    ImGui::TextWrapped("Mapa %.0fx%.0f m · %d tareas%s · %d-%d obstáculos móviles · batería %.1f Wh (%.0f%% al inicio) · máx. %.0f s",
                       mw, mh, cfg.tasks.count, cfg.tasks.deadlines ? " con plazos" : "", cfg.obstacles.minCount,
                       cfg.obstacles.maxCount, cfg.energy.capacityWh, 100.0 * cfg.energy.initialFraction, cfg.sim.maxTime);
    ImGui::PopStyleColor();
    generatorSection();
}

void GuiApp::generatorSection() {
    if (!generator_) return;
    if (!ImGui::TreeNode("Escenario aleatorio (generador)")) return;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("El mismo generador del entrenamiento. 0 = trivial, 1 = parecido a E5.");
    ImGui::PopStyleColor();
    static const char* kNames[kSkills] = {"Mapa", "Tareas", "Energía", "Cargas", "Obstáculos", "Terreno"};
    static const char* kHelp[kSkills] = {
        "Tamaño del almacén y anchura de los pasillos.",
        "Cantidad de tareas, prioridades y plazos.",
        "Capacidad de la batería y carga inicial.",
        "Tipos de carga: liviana, media, pesada y frágil.",
        "Montacargas y operarios en movimiento.",
        "Zonas resbaladizas, zona restringida y variación de la física."};
    float all = 0.0f;
    for (float x : genDifficulty_.v) all += x / kSkills;
    ImGui::SetNextItemWidth(-90);
    if (ImGui::SliderFloat("Todas", &all, 0.0f, 1.0f, "%.2f")) genDifficulty_ = Difficulty::uniform(all);
    for (int k = 0; k < kSkills; ++k) {
        ImGui::SetNextItemWidth(-90);
        ImGui::SliderFloat(kNames[k], &genDifficulty_.v[static_cast<size_t>(k)], 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kHelp[k]);
    }
    ImGui::Checkbox("Uno distinto en cada episodio", &genEachEpisode_);
    if (ImGui::Button("Generar", ImVec2(-1, 0))) {
        try {
            if (genEachEpisode_) {
                const ScenarioGenerator* gen = generator_.get();
                const Difficulty d = genDifficulty_;
                session_.setScenarioProvider([gen, d](uint64_t seed) { return gen->generate(d, seed); });
            } else {
                session_.setScenario(generator_->generate(genDifficulty_, session_.seed()));
            }
            scenarioIndex_ = -1;
            message_.clear();
        } catch (const std::exception& e) {
            message_ = std::string("No se pudo generar: ") + e.what();
        }
    }
    ImGui::TreePop();
}

void GuiApp::agentsSection() {
    sectionTitle("Agentes");
    int remove = -1;
    for (size_t i = 0; i < session_.agentCount(); ++i) {
        const auto& a = session_.agent(i);
        ImGui::PushID(static_cast<int>(i));
        ImGui::ColorButton("##color", toVec4(agentColor(i)), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                           ImVec2(14, 14));
        ImGui::SameLine();
        ImGui::TextUnformatted(a.spec().label.c_str());
        if (ImGui::IsItemHovered() && !a.spec().model.empty()) ImGui::SetTooltip("%s", a.spec().model.c_str());
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 20);
        if (ImGui::SmallButton("x")) remove = static_cast<int>(i);
        ImGui::PopID();
    }
    if (remove >= 0) session_.removeAgent(static_cast<size_t>(remove));
    if (session_.agentCount() == 0) ImGui::TextDisabled("Añade al menos un agente.");

    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##tipo", kKinds[newKind_].label)) {
        for (int k = 0; k < static_cast<int>(std::size(kKinds)); ++k) {
            const bool disabled = std::string(kKinds[k].kind) == "sac" && !hasSacSupport();
            ImGui::BeginDisabled(disabled);
            if (ImGui::Selectable(kKinds[k].label, k == newKind_)) newKind_ = k;
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    const bool sac = std::string(kKinds[newKind_].kind) == "sac";
    if (sac) {
        ImGui::SetNextItemWidth(-1);
        const char* preview = models_.empty() ? "(no hay modelos .pt)" : models_[static_cast<size_t>(newModel_)].label.c_str();
        if (ImGui::BeginCombo("##modelo", preview)) {
            for (int m = 0; m < static_cast<int>(models_.size()); ++m)
                if (ImGui::Selectable(models_[static_cast<size_t>(m)].label.c_str(), m == newModel_)) newModel_ = m;
            ImGui::EndCombo();
        }
    }
    const bool full = session_.agentCount() >= kMaxAgents;
    ImGui::BeginDisabled(full || (sac && models_.empty()));
    if (ImGui::Button(full ? "Máximo de agentes" : "Añadir agente", ImVec2(-1, 0))) {
        AgentSpec spec;
        spec.kind = kKinds[newKind_].kind;
        if (sac) {
            const auto& m = models_[static_cast<size_t>(newModel_)];
            spec.model = m.path.string();
            spec.label = "SAC · " + m.label;
            // Etiquetas más cortas para los modelos de la carpeta models/.
            if (m.label.rfind("models/", 0) == 0) spec.label = "SAC · " + m.path.stem().string();
        }
        try {
            session_.addAgent(spec);
            message_.clear();
        } catch (const std::exception& e) {
            message_ = std::string("No se pudo crear el agente: ") + e.what();
        }
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("Todos los agentes corren el mismo episodio\n(misma semilla) en mundos separados.");
}

void GuiApp::playbackSection() {
    sectionTitle("Episodio y reproducción");
    if (session_.seed() != shownSeed_) seedInput_ = shownSeed_ = session_.seed();
    ImGui::SetNextItemWidth(110);
    ImGui::InputScalar("##semilla", ImGuiDataType_U64, &seedInput_);
    ImGui::SameLine();
    if (ImGui::Button("Usar semilla")) session_.reset(seedInput_);
    if (ImGui::Button("Reiniciar (R)")) session_.reset(session_.seed());
    ImGui::SameLine();
    if (ImGui::Button("Siguiente semilla")) session_.nextEpisode();

    const float bw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button(session_.paused() ? "Reanudar (Espacio)" : "Pausar (Espacio)", ImVec2(bw, 0)))
        session_.setPaused(!session_.paused());
    ImGui::SameLine();
    if (ImGui::Button("Un paso (N)", ImVec2(bw, 0))) {
        session_.setPaused(true);
        session_.requestStep();
    }

    bool maxSpeed = session_.maxSpeed();
    ImGui::BeginDisabled(maxSpeed);
    float speed = static_cast<float>(session_.speed());
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##velocidad", &speed, 0.0625f, 64.0f, "velocidad x%.2f", ImGuiSliderFlags_Logarithmic))
        session_.setSpeed(speed);
    ImGui::EndDisabled();
    if (ImGui::Checkbox("Velocidad máxima", &maxSpeed)) session_.setMaxSpeed(maxSpeed);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Simula tan rápido como permite la CPU; útil para acumular\nestadísticas de muchos episodios.");
    bool autoAdvance = session_.autoAdvance();
    if (ImGui::Checkbox("Pasar al siguiente episodio al terminar", &autoAdvance)) session_.setAutoAdvance(autoAdvance);
    bool shield = session_.shieldEnabled();
    if (ImGui::Checkbox("Capa de seguridad", &shield)) session_.setShield(shield);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Recorta la consigna de cualquier agente para que no se acerque a\n"
                          "muros, estanterías u obstáculos más rápido de lo que puede frenar\n"
                          "(como el escáner de seguridad de un AGV). Reinicia el episodio.");
}

void GuiApp::viewSection() {
    sectionTitle("Vista");
    ImGui::Checkbox("LiDAR", &view_.lidar);
    ImGui::SameLine(150);
    ImGui::Checkbox("Ruta A*", &view_.path);
    ImGui::Checkbox("Recorrido", &view_.trail);
    ImGui::SameLine(150);
    ImGui::Checkbox("Intención", &view_.intent);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Curva punteada: dónde acabaría el robot en 1.5 s\nsi mantuviera la acción que acaba de elegir la política.");
    ImGui::Checkbox("Sucesos", &view_.events);
    ImGui::SameLine(150);
    ImGui::Checkbox("Etiquetas", &view_.labels);
    ImGui::Checkbox("Seguir robot", &view_.follow);
    int mode = static_cast<int>(view_.trailMode);
    ImGui::TextUnformatted("Color del recorrido:");
    ImGui::RadioButton("estado", &mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("velocidad", &mode, 1);
    view_.trailMode = static_cast<TrailMode>(mode);
    if (ImGui::Button("Restablecer cámara")) view_.zoom = 1.0f, view_.pan = {};
    ImGui::SameLine();
    if (ImGui::Button("Atajos (F1)")) showHelp_ = true;
}

// --- Centro: una vista por agente ---------------------------------------------

void GuiApp::drawViewports(float x, float y, float w, float h) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    ImGui::Begin("Vistas", nullptr, kPanelFlags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    const size_t n = session_.agentCount();
    if (n == 0) {
        ImGui::TextDisabled("Sin agentes. Añade uno en el panel de la izquierda.");
    } else {
        // Rejilla que aprovecha mejor el espacio para el ancho del almacén.
        const auto& m = session_.scenario().map;
        const float mapAspect = m.width > 0 && m.height > 0 ? m.width / m.height : 1.5f;
        size_t bestCols = 1;
        float bestArea = 0.0f;
        for (size_t cols = 1; cols <= n; ++cols) {
            const size_t rows = (n + cols - 1) / cols;
            const float cw = w / static_cast<float>(cols), ch = h / static_cast<float>(rows);
            const float fit = std::min(cw, ch * mapAspect);
            if (fit > bestArea) bestArea = fit, bestCols = cols;
        }
        const size_t rows = (n + bestCols - 1) / bestCols;
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float tw = std::floor((avail.x - 4.0f * static_cast<float>(bestCols - 1)) / static_cast<float>(bestCols));
        const float th = std::floor((avail.y - 4.0f * static_cast<float>(rows - 1)) / static_cast<float>(rows));
        for (size_t i = 0; i < n; ++i) {
            if (i % bestCols != 0) ImGui::SameLine();
            drawTile(i, tw, th);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void GuiApp::drawTile(size_t i, float w, float h) {
    // Franja de título arriba, mapa en medio y franja de estado abajo: los
    // textos no tapan el almacén.
    const float lh = ImGui::GetTextLineHeight();
    const float bar = lh + 8.0f;
    const float mapH = std::max(16.0f, h - 2.0f * bar - 2.0f * ImGui::GetStyle().ItemSpacing.y);

    if (tiles_.size() <= i) tiles_.resize(i + 1);
    Tile& tile = tiles_[i];
    const sf::Vector2u size(static_cast<unsigned>(std::max(16.0f, w)), static_cast<unsigned>(mapH));
    if (!tile.texture || tile.texture->getSize() != size) {
        sf::ContextSettings settings;
        settings.antiAliasingLevel = 4;
        if (!tile.texture) {
            tile.texture = std::make_unique<sf::RenderTexture>(size, settings);
            tile.renderer = std::make_unique<Renderer>(*tile.texture);
        } else if (!tile.texture->resize(size, settings)) {
            return;
        }
    }

    const AgentRunner& a = session_.agent(i);
    const WarehouseEnv& env = a.env();
    RenderOptions opt = view_;
    opt.robotColor = agentColor(i);
    opt.history = &a.history();
    tile.texture->clear(sf::Color(20, 22, 26));
    tile.renderer->draw(env, opt);
    tile.texture->display();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const auto& m = env.metrics();
    char buf[200];

    ImGui::BeginGroup();
    // Título: color y nombre del agente, y contadores a la derecha.
    dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + bar), IM_COL32(16, 17, 21, 255));
    dl->AddRectFilled(ImVec2(p0.x, p0.y + bar - 2), ImVec2(p0.x + w, p0.y + bar), toU32(agentColor(i)));
    dl->AddRectFilled(ImVec2(p0.x + 6, p0.y + 5), ImVec2(p0.x + 4 + lh, p0.y + 3 + lh), toU32(agentColor(i)), 2.0f);
    dl->AddText(ImVec2(p0.x + 10 + lh, p0.y + 4), IM_COL32(240, 240, 240, 255), a.spec().label.c_str());
    dl->PushClipRect(p0, ImVec2(p0.x + w, p0.y + h), true);
    // Contadores: la versión más larga que quepa junto al nombre.
    const float freeW = w - (14 + lh + ImGui::CalcTextSize(a.spec().label.c_str()).x) - 16;
    const int hits = m.collisions[0] + m.collisions[1] + m.collisions[2];
    std::snprintf(buf, sizeof buf, "t %.0f s · %d/%d entregadas · %d choques · batería %.0f%%", env.time(), m.delivered,
                  m.tasksTotal, hits, 100.0 * env.batteryFraction());
    if (ImGui::CalcTextSize(buf).x > freeW)
        std::snprintf(buf, sizeof buf, "%.0f s · %d/%d · %d choques", env.time(), m.delivered, m.tasksTotal, hits);
    if (ImGui::CalcTextSize(buf).x > freeW) std::snprintf(buf, sizeof buf, "%d/%d", m.delivered, m.tasksTotal);
    dl->AddText(ImVec2(p0.x + w - ImGui::CalcTextSize(buf).x - 8, p0.y + 4), IM_COL32(200, 200, 200, 255), buf);
    ImGui::Dummy(ImVec2(w, bar));

    const ImVec2 imgPos = ImGui::GetCursorScreenPos();
    ImGui::PushID(static_cast<int>(i));
    ImGui::Image(*tile.texture, sf::Vector2f(w, mapH));

    // Cámara compartida por todas las vistas para comparar en el mismo encuadre.
    if (ImGui::IsItemHovered()) {
        const auto& io = ImGui::GetIO();
        if (io.MouseWheel != 0.0f) view_.zoom = std::clamp(view_.zoom * (io.MouseWheel > 0 ? 1.15f : 1.0f / 1.15f), 0.5f, 8.0f);
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            // Metros por píxel según el encuadre que usa el renderizador.
            const float viewH = std::max(env.map().height, env.map().width / (w / mapH)) * 1.06f / view_.zoom;
            const float mpp = viewH / mapH;
            view_.pan.x -= io.MouseDelta.x * mpp;
            view_.pan.y += io.MouseDelta.y * mpp;
        }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) view_.zoom = 1.0f, view_.pan = {};
    }
    ImGui::PopID();
    if (a.finished()) dl->AddRectFilled(imgPos, ImVec2(imgPos.x + w, imgPos.y + mapH), IM_COL32(0, 0, 0, 70));

    // Estado: qué está haciendo o cómo terminó, y avisos del momento.
    const ImVec2 f0 = ImGui::GetCursorScreenPos();
    const float ty = f0.y + 4;
    if (a.finished()) {
        const bool ok = a.error().empty() && m.reason == EndReason::AllDelivered;
        dl->AddRectFilled(f0, ImVec2(f0.x + w, f0.y + bar), ok ? IM_COL32(25, 90, 45, 255) : IM_COL32(110, 32, 32, 255));
        if (a.error().empty())
            std::snprintf(buf, sizeof buf, "TERMINADO: %s · %d/%d entregadas en %.0f s", reasonLabel(m.reason), m.delivered, m.tasksTotal,
                          m.simTime);
        else
            std::snprintf(buf, sizeof buf, "ERROR EN LA POLÍTICA: %s", a.error().c_str());
        dl->AddText(ImVec2(f0.x + 8, ty), IM_COL32_WHITE, buf);
    } else {
        const std::string status = statusLine(env);
        dl->AddRectFilled(f0, ImVec2(f0.x + w, f0.y + bar), IM_COL32(16, 17, 21, 255));
        const sf::Color pc = env.charging() ? palette::kCharger : palette::kPhase[static_cast<int>(env.decision().phase)];
        dl->AddRectFilled(ImVec2(f0.x + 6, ty + 1), ImVec2(f0.x + 10, ty + lh - 1), toU32(pc));
        dl->AddText(ImVec2(f0.x + 16, ty), IM_COL32(235, 235, 235, 255), status.c_str());
        float bx = f0.x + w - 8;
        auto badges = liveBadges(a);
        if (session_.paused()) badges.push_back({"PAUSA", IM_COL32(150, 120, 30, 230)});
        const float statusEnd = f0.x + 16 + ImGui::CalcTextSize(status.c_str()).x + 8;
        for (const auto& b : badges) {
            const ImVec2 ts = ImGui::CalcTextSize(b.text);
            const float x0 = bx - ts.x - 12;
            if (x0 < statusEnd) break;  // sin sitio: no tapar el estado
            dl->AddRectFilled(ImVec2(x0, ty - 2), ImVec2(bx, ty + lh + 2), b.color, 4.0f);
            dl->AddText(ImVec2(x0 + 6, ty), IM_COL32_WHITE, b.text);
            bx = x0 - 6;
        }
    }
    dl->PopClipRect();
    ImGui::Dummy(ImVec2(w, bar));
    ImGui::EndGroup();
}

// --- Panel derecho: métricas en vivo, gráficas y resumen ------------------------

void GuiApp::drawMetricsPanel(float x, float y, float w, float h) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::Begin("Métricas", nullptr, kPanelFlags);
    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        auto flags = [&](int tab) {
            return tab == initialTab_ ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        };
        if (ImGui::BeginTabItem("En vivo", nullptr, flags(0))) {
            for (size_t i = 0; i < session_.agentCount(); ++i) agentDetails(i);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Gráficas", nullptr, flags(1))) {
            plotsSection();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Resumen", nullptr, flags(2))) {
            summarySection();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Tiempo", nullptr, flags(3))) {
            timelineSection();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Leyenda", nullptr, flags(4))) {
            legendSection();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        initialTab_ = -1;
    }
    ImGui::End();
}

void GuiApp::agentDetails(size_t i) {
    const AgentRunner& a = session_.agent(i);
    const WarehouseEnv& env = a.env();
    const auto& m = env.metrics();
    const auto& d = env.decision();

    ImGui::PushID(static_cast<int>(i));
    ImGui::PushStyleColor(ImGuiCol_Header, toU32(agentColor(i), 90));
    const bool open = ImGui::CollapsingHeader(a.spec().label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopStyleColor();
    if (open) {
        if (!a.error().empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
            ImGui::TextWrapped("Error: %s", a.error().c_str());
            ImGui::PopStyleColor();
        } else if (env.done()) {
            ImGui::TextColored(m.reason == EndReason::AllDelivered ? ImVec4(0.4f, 0.9f, 0.5f, 1) : ImVec4(1.0f, 0.6f, 0.4f, 1),
                               "Terminado: %s", reasonLabel(m.reason));
        } else {
            ImGui::TextWrapped("t = %.1f s · %s", env.time(), statusLine(env).c_str());
            const auto badges = liveBadges(a);
            for (size_t k = 0; k < badges.size(); ++k) {
                if (k > 0) ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, badges[k].color);
                ImGui::SmallButton(badges[k].text);
                ImGui::PopStyleColor();
            }
        }
        (void)d;

        char buf[64];
        std::snprintf(buf, sizeof buf, "batería %.0f%%%s", 100.0 * env.batteryFraction(), env.charging() ? " · recargando" : "");
        const float bat = env.batteryFraction();
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                              bat > 0.4f ? ImVec4(0.3f, 0.75f, 0.4f, 1) : bat > 0.2f ? ImVec4(0.9f, 0.7f, 0.2f, 1) : ImVec4(0.9f, 0.3f, 0.25f, 1));
        ImGui::ProgressBar(bat, ImVec2(-1, 0), buf);
        ImGui::PopStyleColor();

        ImGui::Text("Carga %.0f kg · fricción %.2f", env.robot().payload(), env.frictionUnderRobot());
        if (env.robot().payload() > 0.0f) {
            const float st = env.loadStress();
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::ColorConvertU32ToFloat4(toU32(palette::stressColor(st))));
            std::snprintf(buf, sizeof buf, "estrés de la carga %.0f%% (100%% = se cae)", 100.0 * st);
            ImGui::ProgressBar(std::min(st, 1.0f), ImVec2(-1, 0), buf);
            ImGui::PopStyleColor();
        }
        ImGui::Text("v %.2f m/s · w %.2f rad/s · derrape %.2f m/s", env.measuredSpeed(), env.measuredAngular(),
                    env.robot().lateralSpeed());
        const Action act = env.lastAction();
        signedBar("lineal", act.linear, toU32(agentColor(i)));
        signedBar("angular", act.angular, toU32(agentColor(i)));

        if (ImGui::BeginTable("##m", 2, ImGuiTableFlags_SizingStretchSame)) {
            auto row = [](const char* k, const char* fmt, auto... v) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", k);
                ImGui::TableNextColumn();
                ImGui::Text(fmt, v...);
            };
            row("Entregadas", "%d / %d", m.delivered, m.tasksTotal);
            row("A tiempo / tarde", "%d / %d", m.onTime, m.late);
            row("Choques (roce/leve/grave)", "%d / %d / %d", m.collisions[0], m.collisions[1], m.collisions[2]);
            row("Incidentes de carga", "%d", m.loadIncidents);
            row("Recargas", "%d", m.recharges);
            row("Energía", "%.2f Wh", m.energyWh);
            row("Distancia", "%.1f m", m.distance);
            row("Retorno", "%.1f", m.totalReturn);
            ImGui::EndTable();
        }
    }
    ImGui::PopID();
}

void GuiApp::plotsSection() {
    const size_t n = session_.agentCount();
    if (n == 0) {
        ImGui::TextDisabled("Sin agentes.");
        return;
    }
    for (size_t i = 0; i < n; ++i) {
        ImGui::ColorButton("##c", toVec4(agentColor(i)), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(10, 10));
        ImGui::SameLine();
        ImGui::TextUnformatted(session_.agent(i).spec().label.c_str());
    }
    ImGui::Spacing();
    std::vector<Series> s(n);
    auto plot = [&](const char* title, auto member, PlotRange range, const char* fmt) {
        for (size_t i = 0; i < n; ++i) {
            const auto& hist = session_.agent(i).history();
            s[i] = {&hist.time, &(hist.*member), toU32(agentColor(i))};
        }
        linePlot(title, s, range, 110.0f, fmt);
        ImGui::Spacing();
    };
    plot("Batería", &EpisodeHistory::battery, {0.0f, 1.0f, false}, "%.2f");
    plot("Velocidad (m/s)", &EpisodeHistory::speed, {-0.2f, 0.5f, true}, "%.2f");
    plot("Retorno acumulado", &EpisodeHistory::cumReturn, {0.0f, 1.0f, true}, "%.1f");
    plot("Tareas entregadas", &EpisodeHistory::delivered, {0.0f, 1.0f, true}, "%.0f");
    plot("Estrés de la carga (1 = se cae)", &EpisodeHistory::loadStress, {0.0f, 1.0f, true}, "%.2f");
}

void GuiApp::summarySection() {
    ImGui::TextWrapped("Medias sobre los episodios terminados en esta sesión (cambiar de escenario las borra).");
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit |
                                      ImGuiTableFlags_ScrollX;
    // Con ScrollX la tabla ocuparía todo el alto restante si no se le da uno.
    const float tableH = ImGui::GetFrameHeightWithSpacing() * (static_cast<float>(session_.agentCount()) + 1.6f);
    if (ImGui::BeginTable("##resumen", 7, flags, ImVec2(0.0f, tableH))) {
        ImGui::TableSetupColumn("Agente");
        ImGui::TableSetupColumn("Ep.");
        ImGui::TableSetupColumn("Compl.");
        ImGui::TableSetupColumn("A tiempo");
        ImGui::TableSetupColumn("Choques");
        ImGui::TableSetupColumn("Wh");
        ImGui::TableSetupColumn("Dur. s");
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < session_.agentCount(); ++i) {
            const auto s = summarize(session_.agent(i).records());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(toVec4(agentColor(i)), "%s", session_.agent(i).spec().label.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", s.episodes);
            if (s.episodes == 0) continue;
            ImGui::TableNextColumn();
            ImGui::Text("%.0f%%", 100.0 * s.completion);
            ImGui::TableNextColumn();
            ImGui::Text("%.0f%%", 100.0 * s.onTime);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", s.harmfulCollisions);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", s.energyWh);
            ImGui::TableNextColumn();
            ImGui::Text("%.0f", s.duration);
        }
        ImGui::EndTable();
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Compl. = tareas entregadas; Choques = leves + graves por episodio; Wh = energía; Dur. = duración.");
    ImGui::PopStyleColor();

    for (size_t i = 0; i < session_.agentCount(); ++i) {
        const auto& recs = session_.agent(i).records();
        if (recs.empty()) continue;
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::TreeNode("##ep", "Episodios de %s", session_.agent(i).spec().label.c_str())) {
            for (const auto& r : recs)
                ImGui::BulletText("semilla %llu: %d/%d, %s, %.0f s", static_cast<unsigned long long>(r.seed), r.metrics.delivered,
                                  r.metrics.tasksTotal, reasonLabel(r.metrics.reason), r.metrics.simTime);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::Spacing();
    if (ImGui::Button("Borrar estadísticas"))
        for (size_t i = 0; i < session_.agentCount(); ++i) session_.agent(i).clearRecords();
}

// --- Línea de tiempo y registro de sucesos -------------------------------------

void GuiApp::timelineSection() {
    const size_t n = session_.agentCount();
    if (n == 0) {
        ImGui::TextDisabled("Sin agentes.");
        return;
    }
    ImGui::TextWrapped("Qué hizo cada agente a lo largo del episodio. Banda: fase; franja inferior: lleva carga; "
                       "iconos: sucesos (pasa el ratón para ver el detalle).");
    float tMax = 10.0f;
    for (size_t i = 0; i < n; ++i)
        if (!session_.agent(i).history().time.empty()) tMax = std::max(tMax, session_.agent(i).history().time.back());

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x;
    const float band = 16.0f, iconRow = 14.0f;
    for (size_t i = 0; i < n; ++i) {
        const AgentRunner& a = session_.agent(i);
        const auto& h = a.history();
        ImGui::Spacing();
        ImGui::TextColored(toVec4(agentColor(i)), "%s", a.spec().label.c_str());
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const float totalH = iconRow + band + 6.0f;
        ImGui::PushID(static_cast<int>(i));
        ImGui::InvisibleButton("##tl", ImVec2(width, totalH));
        ImGui::PopID();
        const float y0 = p0.y + iconRow;
        auto xOf = [&](float t) { return p0.x + t / tMax * width; };
        dl->AddRectFilled(ImVec2(p0.x, y0), ImVec2(p0.x + width, y0 + band), IM_COL32(30, 32, 38, 255));

        // Tramos contiguos con la misma fase (o recargando) en un solo rectángulo.
        size_t start = 0;
        auto key = [&](size_t k) { return (h.flags[k] & kCharging) ? 4 : static_cast<int>(h.phase[k]); };
        for (size_t k = 1; k <= h.size(); ++k) {
            if (k < h.size() && key(k) == key(start)) continue;
            const int st = key(start);
            const sf::Color c = st == 4 ? palette::kCharger : palette::kPhase[st];
            const float xa = xOf(h.time[start]), xb = xOf(h.time[std::min(k, h.size() - 1)]);
            dl->AddRectFilled(ImVec2(xa, y0), ImVec2(std::max(xb, xa + 1.0f), y0 + band), toU32(c, st == 4 ? 255 : 200));
            start = k;
        }
        // Franja de carga y marcas de derrape.
        for (size_t k = 1; k < h.size(); ++k) {
            if (h.flags[k] & kCarrying)
                dl->AddRectFilled(ImVec2(xOf(h.time[k - 1]), y0 + band + 1), ImVec2(xOf(h.time[k]) + 0.5f, y0 + band + 5),
                                  IM_COL32(200, 150, 80, 255));
            if (h.flags[k] & kSlipping) dl->AddLine(ImVec2(xOf(h.time[k]), y0), ImVec2(xOf(h.time[k]), y0 + 4), toU32(palette::kSlip), 1.0f);
        }
        for (const auto& e : h.events) eventIcon(dl, ImVec2(xOf(e.time), p0.y + iconRow * 0.5f), 4.0f, e);
        if (!h.time.empty()) {
            const float xn = xOf(h.time.back());
            dl->AddLine(ImVec2(xn, p0.y), ImVec2(xn, y0 + band + 5), IM_COL32(255, 255, 255, 200), 1.5f);
        }

        if (ImGui::IsItemHovered() && !h.time.empty()) {
            const float t = (ImGui::GetIO().MousePos.x - p0.x) / width * tMax;
            dl->AddLine(ImVec2(ImGui::GetIO().MousePos.x, p0.y), ImVec2(ImGui::GetIO().MousePos.x, y0 + band + 5),
                        IM_COL32(200, 200, 200, 120));
            ImGui::BeginTooltip();
            if (t <= h.time.back()) {
                const auto it = std::lower_bound(h.time.begin(), h.time.end(), t);
                const size_t k = static_cast<size_t>(std::min<std::ptrdiff_t>(it - h.time.begin(), static_cast<std::ptrdiff_t>(h.size()) - 1));
                ImGui::Text("t = %.1f s · %s%s", t, (h.flags[k] & kCharging) ? "recargando" : phaseLabel(static_cast<Phase>(h.phase[k])),
                            (h.flags[k] & kCarrying) ? " · con carga" : "");
                ImGui::Text("batería %.0f%% · %.2f m/s", 100.0 * h.battery[k], static_cast<double>(h.speed[k]));
            } else {
                ImGui::Text("t = %.1f s (el episodio de este agente terminó)", t);
            }
            const float tol = tMax / width * 4.0f;  // ~4 px a cada lado
            for (const auto& e : h.events)
                if (std::abs(e.time - t) <= tol)
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(eventColor(e)), "%.1f s  %s", e.time, eventText(e).c_str());
            ImGui::EndTooltip();
        }
    }
    // Eje de tiempo común.
    const ImVec2 axis = ImGui::GetCursorScreenPos();
    char buf[32];
    for (int k = 0; k <= 4; ++k) {
        const float t = tMax * static_cast<float>(k) / 4.0f;
        std::snprintf(buf, sizeof buf, "%.0f s", t);
        const float tw = ImGui::CalcTextSize(buf).x;
        const float x = std::clamp(axis.x + width * static_cast<float>(k) / 4.0f - tw * 0.5f, axis.x, axis.x + width - tw);
        dl->AddText(ImVec2(x, axis.y), IM_COL32(150, 150, 150, 255), buf);
    }
    ImGui::Dummy(ImVec2(width, ImGui::GetTextLineHeight()));

    // Colores de las fases.
    ImGui::Spacing();
    const char* names[5] = {"a recogida", "a entrega", "al cargador", "sin tarea", "recargando"};
    for (int k = 0; k < 5; ++k) {
        const sf::Color c = k == 4 ? palette::kCharger : palette::kPhase[k];
        ImGui::ColorButton(names[k], toVec4(c), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(12, 12));
        ImGui::SameLine();
        ImGui::TextUnformatted(names[k]);
        if (k % 3 != 2 && k != 4) ImGui::SameLine();
    }

    sectionTitle("Registro de sucesos");
    for (size_t i = 0; i < n; ++i) {
        const auto& events = session_.agent(i).history().events;
        ImGui::PushID(static_cast<int>(i));
        char header[160];
        std::snprintf(header, sizeof header, "%s (%zu)###log", session_.agent(i).spec().label.c_str(), events.size());
        if (ImGui::TreeNodeEx(header, ImGuiTreeNodeFlags_DefaultOpen)) {
            if (events.empty()) ImGui::TextDisabled("Nada todavía.");
            // Los más recientes arriba.
            int shown = 0;
            for (auto it = events.rbegin(); it != events.rend() && shown < 60; ++it, ++shown) {
                const ImVec2 c = ImGui::GetCursorScreenPos();
                eventIcon(ImGui::GetWindowDrawList(), ImVec2(c.x + 7, c.y + ImGui::GetTextLineHeight() * 0.5f), 4.5f, *it);
                ImGui::Dummy(ImVec2(16, ImGui::GetTextLineHeight()));
                ImGui::SameLine();
                ImGui::TextDisabled("%6.1f s", it->time);
                ImGui::SameLine();
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(eventColor(*it)), "%s", eventText(*it).c_str());
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

// --- Leyenda --------------------------------------------------------------------

void GuiApp::legendSection() {
    using namespace palette;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float lh = ImGui::GetTextLineHeight();
    // Fila: icono de 28 px dibujado por `icon` y el texto a su derecha.
    auto row = [&](const char* text, auto icon) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        icon(ImVec2(p.x + 14, p.y + lh * 0.5f + 1));
        ImGui::Dummy(ImVec2(28, lh + 2));
        ImGui::SameLine();
        ImGui::TextWrapped("%s", text);
    };
    auto boxIcon = [&](sf::Color fill, sf::Color border) {
        return [=](ImVec2 c) {
            dl->AddRectFilled(ImVec2(c.x - 10, c.y - 6), ImVec2(c.x + 10, c.y + 6), toU32(fill));
            dl->AddRect(ImVec2(c.x - 10, c.y - 6), ImVec2(c.x + 10, c.y + 6), toU32(border), 0, 0, 1.5f);
        };
    };
    auto lineIcon = [&](sf::Color c, float thick, bool dashedLine) {
        return [=](ImVec2 p) {
            if (!dashedLine) {
                dl->AddLine(ImVec2(p.x - 11, p.y), ImVec2(p.x + 11, p.y), toU32(c), thick);
                return;
            }
            for (float x = -11; x < 11; x += 6) dl->AddLine(ImVec2(p.x + x, p.y), ImVec2(p.x + x + 3.5f, p.y), toU32(c), thick);
        };
    };
    const sf::Color agent = agentColor(0);

    sectionTitle("Robot");
    row("Robot del agente (color = agente). Triángulo blanco: frente; bloques negros: ruedas.", [&](ImVec2 c) {
        dl->AddRectFilled(ImVec2(c.x - 9, c.y - 9), ImVec2(c.x + 9, c.y - 6), IM_COL32(25, 25, 28, 255));
        dl->AddRectFilled(ImVec2(c.x - 9, c.y + 6), ImVec2(c.x + 9, c.y + 9), IM_COL32(25, 25, 28, 255));
        dl->AddRectFilled(ImVec2(c.x - 11, c.y - 6), ImVec2(c.x + 11, c.y + 6), toU32(agent));
        dl->AddTriangleFilled(ImVec2(c.x + 10, c.y), ImVec2(c.x + 4, c.y - 4), ImVec2(c.x + 4, c.y + 4), IM_COL32_WHITE);
    });
    row("Caja de carga: marrón = estable, naranja/rojo = a punto de caerse.", [&](ImVec2 c) {
        dl->AddRectFilled(ImVec2(c.x - 12, c.y - 5), ImVec2(c.x - 3, c.y + 5), toU32(stressColor(0.0f)));
        dl->AddRectFilled(ImVec2(c.x + 3, c.y - 5), ImVec2(c.x + 12, c.y + 5), toU32(stressColor(0.95f)));
    });
    row("Intención: curva que seguiría en 1.5 s con la acción actual de la política.", lineIcon(sf::Color(170, 200, 250), 2.5f, true));
    row("Flecha blanca: velocidad real (lo que avanza en 1 s).", [&](ImVec2 c) {
        dl->AddLine(ImVec2(c.x - 11, c.y), ImVec2(c.x + 6, c.y), IM_COL32_WHITE, 2.0f);
        dl->AddTriangleFilled(ImVec2(c.x + 12, c.y), ImVec2(c.x + 5, c.y - 4), ImVec2(c.x + 5, c.y + 4), IM_COL32_WHITE);
    });
    row("Anillo amarillo pulsante: recargando. Anillo rojo: derrapando.", [&](ImVec2 c) {
        dl->AddCircle(ImVec2(c.x - 6, c.y), 6, toU32(kCharger), 16, 2.5f);
        dl->AddCircle(ImVec2(c.x + 7, c.y), 6, toU32(kSlip), 16, 2.5f);
    });
    row("Barra sobre el robot: batería.", [&](ImVec2 c) {
        dl->AddRectFilled(ImVec2(c.x - 12, c.y - 3), ImVec2(c.x + 12, c.y + 3), IM_COL32(0, 0, 0, 200));
        dl->AddRectFilled(ImVec2(c.x - 11, c.y - 2), ImVec2(c.x + 5, c.y + 2), IM_COL32(80, 200, 100, 255));
    });
    row("Rayos LiDAR: verde lejos, naranja < 2 m, rojo < 0.8 m.", [&](ImVec2 c) {
        dl->AddLine(ImVec2(c.x - 12, c.y + 5), ImVec2(c.x + 12, c.y - 6), IM_COL32(120, 255, 120, 200), 1.5f);
        dl->AddLine(ImVec2(c.x - 12, c.y + 5), ImVec2(c.x + 4, c.y + 4), IM_COL32(255, 80, 70, 230), 1.5f);
    });

    sectionTitle("Recorrido");
    row("Con carga: trazo grueso y opaco.", lineIcon(agent, 4.0f, false));
    row("Sin carga: trazo fino y translúcido.", lineIcon(sf::Color(agent.r, agent.g, agent.b, 120), 1.5f, false));
    row("Mientras recarga: amarillo. Puntos rojos: derrapes.", lineIcon(kCharger, 3.0f, false));
    row("Modo velocidad (tecla V): azul lento → verde → rojo rápido.", [&](ImVec2 c) {
        for (int k = 0; k < 6; ++k)
            dl->AddLine(ImVec2(c.x - 12 + 4.0f * k, c.y), ImVec2(c.x - 8 + 4.0f * k, c.y), toU32(speedColor(k / 5.0f)), 3.5f);
    });
    row("Ruta A* planificada (punteada) y punto que persigue.", lineIcon(kPath, 2.0f, true));

    sectionTitle("Tareas");
    row("Rombo en la recogida, color = prioridad: gris baja, azul media, naranja alta, roja crítica. Contorno blanco: la "
        "tarea elegida ahora.",
        [&](ImVec2 c) {
            for (int k = 0; k < 4; ++k) {
                const ImVec2 q(c.x - 10 + 7.0f * k, c.y);
                dl->AddQuadFilled(ImVec2(q.x, q.y - 4), ImVec2(q.x + 3.5f, q.y), ImVec2(q.x, q.y + 4), ImVec2(q.x - 3.5f, q.y),
                                  toU32(kPriority[k]));
            }
        });
    row("Anillo de plazo: se vacía al acercarse el vencimiento (verde → amarillo → rojo).", [&](ImVec2 c) {
        dl->PathArcTo(c, 7, -1.57f, 2.6f, 16);
        dl->PathStroke(IM_COL32(240, 190, 50, 255), 0, 2.5f);
    });
    row("Destino de la tarea actual: anillo del color de su prioridad.", [&](ImVec2 c) {
        dl->AddCircle(c, 7, toU32(kPriority[2]), 16, 2.0f);
        dl->AddCircleFilled(c, 2.0f, toU32(kPriority[2]));
    });

    sectionTitle("Mapa");
    row("Estantería.", boxIcon(kShelf, sf::Color(90, 60, 40)));
    row("Zona de entrega (E1, E2…).", boxIcon(sf::Color(kDropoff.r, kDropoff.g, kDropoff.b, 60), kDropoff));
    row("Estación de recarga.", [&](ImVec2 c) {
        boxIcon(sf::Color(kCharger.r, kCharger.g, kCharger.b, 50), kCharger)(c);
        EventMark e;
        e.kind = EventKind::RechargeStart;
        eventIcon(dl, c, 4.5f, e);
    });
    row("Suelo resbaladizo (rayas azules, con su coeficiente \xce\xbc).", boxIcon(sf::Color(kFriction.r, kFriction.g, kFriction.b, 70), kFriction));
    row("Zona restringida (rayas rojas): penaliza entrar.", boxIcon(sf::Color(kRestricted.r, kRestricted.g, kRestricted.b, 60), kRestricted));
    row("Montacargas (horquillas delante) y operario; flecha = hacia dónde se mueve.", [&](ImVec2 c) {
        dl->AddRectFilled(ImVec2(c.x - 13, c.y - 5), ImVec2(c.x - 1, c.y + 5), toU32(kForklift));
        dl->AddCircleFilled(ImVec2(c.x + 7, c.y), 5, toU32(kWorker));
    });

    sectionTitle("Sucesos (mapa y línea de tiempo)");
    auto eventRow = [&](const char* text, EventKind kind, int detail) {
        EventMark e;
        e.kind = kind;
        e.detail = detail;
        row(text, [&](ImVec2 c) { eventIcon(dl, c, 5.0f, e); });
    };
    eventRow("Recogió una carga.", EventKind::Pickup, 0);
    eventRow("Entregó a tiempo.", EventKind::Delivered, 0);
    eventRow("Entregó tarde.", EventKind::DeliveredLate, 0);
    eventRow("Roce.", EventKind::Collision, 0);
    eventRow("Choque leve.", EventKind::Collision, 1);
    eventRow("Choque grave (con círculo).", EventKind::Collision, 2);
    eventRow("Se le cayó la carga por acelerar o girar demasiado fuerte.", EventKind::LoadDrop, 0);
    eventRow("Empezó a recargar.", EventKind::RechargeStart, 0);
    eventRow("Entró en zona restringida.", EventKind::RestrictedEntry, 0);
    eventRow("Venció una tarea crítica (solo línea de tiempo).", EventKind::CriticalOverdue, 0);
}

}  // namespace wrl::gui
