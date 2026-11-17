#include "warehouse/io/trace.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace wrl {

void Trace::save(const std::filesystem::path& path) const {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    Json acts = Json::array();
    for (const auto& a : actions) acts.push_back({a.linear, a.angular});
    const Json j{{"scenario", scenario}, {"seed", seed}, {"policy", policy}, {"actions", acts}};
    std::ofstream out(path);
    if (!out) throw std::runtime_error("no se pudo escribir " + path.string());
    out << j.dump();
}

Trace Trace::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("no se pudo abrir " + path.string());
    const Json j = Json::parse(in);
    Trace t;
    t.scenario = j.at("scenario");
    t.seed = j.at("seed").get<uint64_t>();
    t.policy = j.value("policy", std::string());
    for (const auto& a : j.at("actions")) t.actions.push_back({a[0].get<float>(), a[1].get<float>()});
    return t;
}

void StateRecorder::record(const WarehouseEnv& env, const StepResult& r) {
    const Pose p = env.robot().pose();
    std::ostringstream s;
    s.precision(5);
    s << env.time() << ',' << p.pos.x << ',' << p.pos.y << ',' << p.theta << ',' << env.robot().forwardSpeed() << ','
      << env.robot().angularSpeed() << ',' << env.batteryFraction() << ',' << env.robot().payload() << ','
      << toString(env.decision().phase) << ',' << env.decision().taskId << ',' << env.lastAction().linear << ','
      << env.lastAction().angular << ',' << r.reward;
    rows_.push_back(s.str());
}

void StateRecorder::save(const std::filesystem::path& csvPath) const {
    if (csvPath.has_parent_path()) std::filesystem::create_directories(csvPath.parent_path());
    std::ofstream out(csvPath);
    if (!out) throw std::runtime_error("no se pudo escribir " + csvPath.string());
    out << "t,x,y,theta,v,w,bateria,carga_kg,fase,tarea,accion_lineal,accion_angular,recompensa\n";
    for (const auto& r : rows_) out << r << '\n';
}

}  // namespace wrl
