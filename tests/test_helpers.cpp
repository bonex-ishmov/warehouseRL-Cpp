#include "test_helpers.hpp"

namespace wrl::test {

ScenarioConfig smallScenario(int tasks) {
    Json j = loadJsonWithBase(std::string(WRL_SOURCE_DIR) + "/configs/robot/default.json");
    j.merge_patch(Json::parse(R"({
        "name": "prueba",
        "map": {
            "procedural": null,
            "width": 16, "height": 12,
            "shelves": [ { "x": 6, "y": 4, "w": 4, "h": 2 } ],
            "pickups": [ [8, 3.2] ],
            "dropoffs": [ [8, 9] ],
            "chargers": [ [2, 10.5] ],
            "robotStart": { "x": 2, "y": 2, "theta": 0 }
        },
        "sensors": { "lidarNoise": 0.0 },
        "sim": { "maxTime": 120 }
    })"));
    j["tasks"] = {{"count", tasks}, {"initial", tasks}, {"priorityWeights", {1, 0, 0, 0}}, {"loadTypes", {"liviana"}}};
    return parseScenario(j);
}

ScenarioConfig repoScenario(const std::string& file) {
    return loadScenario(std::string(WRL_SOURCE_DIR) + "/configs/scenarios/" + file);
}

}  // namespace wrl::test
