#pragma once
#include <cmath>
#include <cstddef>

inline float compute_rms(const float* samples, std::size_t n) {
    if (!samples || n == 0) return 0.0f;
    double acc = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double s = static_cast<double>(samples[i]);
        acc += s * s;
    }
    const double mean = acc / static_cast<double>(n);
    return static_cast<float>(std::sqrt(mean));
}
