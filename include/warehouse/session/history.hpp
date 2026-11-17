#pragma once

#include <cstdint>
#include <vector>

#include "warehouse/env/warehouse_env.hpp"

namespace wrl {

/// Sucesos de un episodio que la interfaz marca en el mapa y en la línea de tiempo.
enum class EventKind : uint8_t {
    Pickup,           ///< detail = tarea
    Delivered,        ///< detail = tarea
    DeliveredLate,    ///< detail = tarea
    Collision,        ///< detail = gravedad (0 roce, 1 leve, 2 grave)
    LoadDrop,         ///< detail = tarea
    RechargeStart,
    RestrictedEntry,
    CriticalOverdue,
};
const char* describe(EventKind kind);

struct EventMark {
    float time = 0.0f;
    Vec2 pos;
    EventKind kind = EventKind::Pickup;
    int detail = -1;
};

/// Estado del robot en cada muestra (bits).
enum StateFlag : uint8_t { kCarrying = 1, kCharging = 2, kSlipping = 4 };

/// Series temporales de un episodio, una muestra por paso de control.
struct EpisodeHistory {
    std::vector<float> time;
    std::vector<float> battery;    ///< fracción [0, 1]
    std::vector<float> speed;      ///< m/s medida
    std::vector<float> cumReturn;  ///< retorno acumulado
    std::vector<float> delivered;  ///< tareas entregadas
    std::vector<float> loadStress;  ///< aceleración de la carga / su límite
    std::vector<Vec2> trail;       ///< posiciones del robot
    std::vector<uint8_t> phase;    ///< Phase del selector de tareas
    std::vector<uint8_t> flags;    ///< StateFlag
    std::vector<EventMark> events;

    void clear();
    void push(const WarehouseEnv& env, bool slipping);
    size_t size() const { return time.size(); }
};

}  // namespace wrl
