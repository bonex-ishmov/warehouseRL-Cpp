#include "warehouse/io/metrics_logger.hpp"

#include <sstream>
#include <stdexcept>

namespace wrl {

MetricsLogger::MetricsLogger(const std::filesystem::path& csvPath) {
    if (csvPath.has_parent_path()) std::filesystem::create_directories(csvPath.parent_path());
    out_.open(csvPath);
    if (!out_) throw std::runtime_error("no se pudo crear " + csvPath.string());
    out_ << header() << '\n';
}

void MetricsLogger::log(const EpisodeMetrics& m, const std::string& scenario, const std::string& policy, uint64_t seed) {
    out_ << row(m, scenario, policy, seed) << '\n';
    out_.flush();
}

std::string MetricsLogger::header() {
    return "escenario,politica,semilla,fin,tareas,entregadas,a_tiempo,tardias,criticas_vencidas,roces,colisiones_leves,"
           "colisiones_graves,graves_operario,incidentes_carga,recargas,recargas_innecesarias,zonas_restringidas,"
           "bateria_agotada,energia_wh,distancia_m,ratio_ruta,tiempo_entrega_medio_s,retraso_medio_s,retorno,"
           "tiempo_sim_s,pasos,holgura_min_m,brusquedad_media,aceleracion_media,pasos_derrape";
}

std::string MetricsLogger::row(const EpisodeMetrics& m, const std::string& scenario, const std::string& policy, uint64_t seed) {
    std::ostringstream s;
    s.precision(6);
    s << scenario << ',' << policy << ',' << seed << ',' << toString(m.reason) << ',' << m.tasksTotal << ',' << m.delivered
      << ',' << m.onTime << ',' << m.late << ',' << m.criticalOverdue << ',' << m.collisions[0] << ',' << m.collisions[1]
      << ',' << m.collisions[2] << ',' << m.workerSevere << ',' << m.loadIncidents << ',' << m.recharges << ','
      << m.unnecessaryRecharges << ',' << m.restrictedEntries << ',' << (m.batteryDepleted ? 1 : 0) << ',' << m.energyWh
      << ',' << m.distance << ',' << m.pathRatio() << ','
      << (m.delivered ? m.sumDeliveryTime / m.delivered : 0.0) << ',' << (m.late ? m.sumDelay / m.late : 0.0) << ','
      << m.totalReturn << ',' << m.simTime << ',' << m.steps << ',' << m.minClearance << ','
      << (m.steps ? m.sumJerk / m.steps : 0.0) << ',' << (m.simTime > 0 ? m.sumAbsAccel / m.simTime : 0.0) << ','
      << m.slipSteps;
    return s.str();
}

MetricsSummary MetricsSummary::from(const std::vector<EpisodeMetrics>& eps) {
    MetricsSummary s;
    s.episodes = static_cast<int>(eps.size());
    if (eps.empty()) return s;
    double tasks = 0, delivered = 0, onTime = 0, incidents = 0, energy = 0, legA = 0, legS = 0, delTime = 0, delay = 0, late = 0;
    double ret2 = 0;
    for (const auto& m : eps) {
        tasks += m.tasksTotal;
        delivered += m.delivered;
        onTime += m.onTime;
        late += m.late;
        incidents += m.loadIncidents;
        energy += m.energyWh;
        legA += m.legActual;
        legS += m.legShortest;
        delTime += m.sumDeliveryTime;
        delay += m.sumDelay;
        s.collisionFreeEpisodes += m.harmfulCollisions() == 0 ? 1 : 0;
        s.harmfulCollisionsPerEpisode += m.harmfulCollisions();
        s.grazesPerEpisode += m.collisions[0];
        s.workerSevere += m.workerSevere;
        s.depletionRate += m.batteryDepleted ? 1 : 0;
        s.rechargesPerEpisode += m.recharges;
        s.meanReturn += m.totalReturn;
        ret2 += m.totalReturn * m.totalReturn;
        s.meanJerk += m.steps ? m.sumJerk / m.steps : 0.0;
        s.meanAccel += m.simTime > 0 ? m.sumAbsAccel / m.simTime : 0.0;
        s.criticalOverduePerEpisode += m.criticalOverdue;
        s.meanSimTime += m.simTime;
    }
    const double n = static_cast<double>(eps.size());
    s.completionRate = tasks > 0 ? delivered / tasks : 0;
    s.onTimeRate = tasks > 0 ? onTime / tasks : 0;
    s.collisionFreeEpisodes /= n;
    s.harmfulCollisionsPerEpisode /= n;
    s.grazesPerEpisode /= n;
    s.pathRatio = legS > 0 ? legA / legS : 0;
    s.loadIncidentRate = (delivered + incidents) > 0 ? incidents / (delivered + incidents) : 0;
    s.depletionRate /= n;
    s.energyPerDeliveryWh = delivered > 0 ? energy / delivered : 0;
    s.rechargesPerEpisode /= n;
    s.meanDeliveryTime = delivered > 0 ? delTime / delivered : 0;
    s.meanDelay = late > 0 ? delay / late : 0;
    s.meanReturn /= n;
    s.stdReturn = std::sqrt(std::max(0.0, ret2 / n - s.meanReturn * s.meanReturn));
    s.meanJerk /= n;
    s.meanAccel /= n;
    s.criticalOverduePerEpisode /= n;
    s.meanSimTime /= n;
    return s;
}

Json MetricsSummary::toJson() const {
    return Json{{"episodios", episodes},
                {"tasa_completadas", completionRate},
                {"tasa_a_tiempo", onTimeRate},
                {"episodios_sin_colision", collisionFreeEpisodes},
                {"colisiones_por_episodio", harmfulCollisionsPerEpisode},
                {"roces_por_episodio", grazesPerEpisode},
                {"colisiones_graves_operario", workerSevere},
                {"ratio_ruta", pathRatio},
                {"tasa_incidentes_carga", loadIncidentRate},
                {"tasa_bateria_agotada", depletionRate},
                {"energia_por_entrega_wh", energyPerDeliveryWh},
                {"recargas_por_episodio", rechargesPerEpisode},
                {"tiempo_entrega_medio_s", meanDeliveryTime},
                {"retraso_medio_s", meanDelay},
                {"criticas_vencidas_por_episodio", criticalOverduePerEpisode},
                {"retorno_medio", meanReturn},
                {"retorno_desviacion", stdReturn},
                {"brusquedad_media", meanJerk},
                {"aceleracion_media", meanAccel},
                {"duracion_media_s", meanSimTime}};
}

std::vector<CriterionResult> MetricsSummary::check(const Json& acceptance) const {
    std::vector<CriterionResult> out;
    auto add = [&](const char* key, const char* label, double value, bool lowerIsBetter) {
        if (!acceptance.contains(key)) return;
        CriterionResult c;
        c.name = label;
        c.value = value;
        c.threshold = acceptance[key].get<double>();
        c.lowerIsBetter = lowerIsBetter;
        c.pass = lowerIsBetter ? value <= c.threshold + 1e-9 : value >= c.threshold - 1e-9;
        out.push_back(c);
    };
    add("minCompletionRate", "tareas completadas", completionRate, false);
    add("minOnTimeRate", "entregas a tiempo", onTimeRate, false);
    add("minCollisionFreeEpisodes", "episodios sin colisiones", collisionFreeEpisodes, false);
    add("maxWorkerSevere", "colisiones graves con operarios", workerSevere, true);
    add("maxPathRatio", "ratio de ruta frente a la más corta", pathRatio, true);
    add("maxLoadIncidentRate", "incidentes de carga por entrega", loadIncidentRate, true);
    add("maxDepletionRate", "episodios con batería agotada", depletionRate, true);
    return out;
}

}  // namespace wrl
