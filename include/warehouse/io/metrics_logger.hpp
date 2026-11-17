#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "warehouse/env/warehouse_env.hpp"

namespace wrl {

/// Escribe una fila CSV por episodio.
class MetricsLogger {
public:
    explicit MetricsLogger(const std::filesystem::path& csvPath);
    void log(const EpisodeMetrics& m, const std::string& scenario, const std::string& policy, uint64_t seed);

    static std::string header();
    static std::string row(const EpisodeMetrics& m, const std::string& scenario, const std::string& policy, uint64_t seed);

private:
    std::ofstream out_;
};

struct CriterionResult {
    std::string name;
    double value = 0.0;
    double threshold = 0.0;
    bool lowerIsBetter = false;
    bool pass = false;
};

/// Resumen de un conjunto de episodios (medias y tasas de la sección 10.2).
struct MetricsSummary {
    int episodes = 0;
    double completionRate = 0, onTimeRate = 0, collisionFreeEpisodes = 0, harmfulCollisionsPerEpisode = 0;
    double grazesPerEpisode = 0, workerSevere = 0, pathRatio = 0, loadIncidentRate = 0, depletionRate = 0;
    double energyPerDeliveryWh = 0, rechargesPerEpisode = 0, meanDeliveryTime = 0, meanDelay = 0;
    double meanReturn = 0, stdReturn = 0, meanJerk = 0, meanAccel = 0, criticalOverduePerEpisode = 0;
    double meanSimTime = 0;

    static MetricsSummary from(const std::vector<EpisodeMetrics>& eps);
    Json toJson() const;
    /// Evalúa los criterios "acceptance" del escenario.
    std::vector<CriterionResult> check(const Json& acceptance) const;
};

}  // namespace wrl
