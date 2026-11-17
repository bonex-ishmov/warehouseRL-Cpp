#pragma once

// Lector mínimo de argumentos "--clave valor" y "--bandera" compartido por las aplicaciones.

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace wrl::cli {

class Args {
public:
    Args(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a.rfind("--", 0) != 0) {
                positional_.push_back(a);
                continue;
            }
            a = a.substr(2);
            if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) values_[a].push_back(argv[++i]);
            else values_[a].push_back("1");
        }
    }

    bool has(const std::string& k) const { return values_.count(k) > 0; }
    std::string get(const std::string& k, const std::string& def = "") const {
        auto it = values_.find(k);
        return it == values_.end() ? def : it->second.back();
    }
    int getInt(const std::string& k, int def) const { return has(k) ? std::atoi(get(k).c_str()) : def; }
    double getDouble(const std::string& k, double def) const { return has(k) ? std::atof(get(k).c_str()) : def; }
    std::vector<std::string> all(const std::string& k) const {
        auto it = values_.find(k);
        return it == values_.end() ? std::vector<std::string>{} : it->second;
    }
    const std::vector<std::string>& positional() const { return positional_; }

private:
    std::map<std::string, std::vector<std::string>> values_;
    std::vector<std::string> positional_;
};

}  // namespace wrl::cli
