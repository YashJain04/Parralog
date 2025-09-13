#pragma once

#include <vector>
#include <stdexcept>
#include <algorithm>

class P2QuantileSketch {
public:
    explicit P2QuantileSketch(double quantile) : quantile(quantile) {}

    void insert(double value) {
        data.push_back(value);
    }

    double getQuantile() {
        if (data.empty()) {
            throw std::runtime_error("No data for quantile calculation");
        }
        std::sort(data.begin(), data.end());
        size_t idx = static_cast<size_t>(quantile * (data.size() - 1));
        return data[idx];
    }

    void merge(const P2QuantileSketch& other) {
        data.insert(data.end(), other.data.begin(), other.data.end());
    }


private:
    double quantile;
    std::vector<double> data;
};
