#include <iostream>
#include <array>
#include <cmath>
#include <stdexcept>

class P2QuantileEstimator {
public:
    explicit P2QuantileEstimator(double quantile) : p(quantile) {
        if (p <= 0.0 || p >= 1.0) {
            throw std::invalid_argument("Quantile must be between 0 and 1");
        }
    }

    void add(double x) {
        if (n < 5) {
            initial[n++] = x;
            if (n == 5) {
                std::sort(initial.begin(), initial.end());
                for (int i = 0; i < 5; ++i) {
                    q[i] = initial[i];
                    npos[i] = i + 1;
                }
                np[0] = 1;
                np[1] = 1 + 2 * p;
                np[2] = 1 + 4 * p;
                np[3] = 3 + 2 * p;
                np[4] = 5;
            }
            return;
        }

        int k;
        if (x < q[0]) {
            q[0] = x;
            k = 0;
        } else if (x < q[1]) {
            k = 0;
        } else if (x < q[2]) {
            k = 1;
        } else if (x < q[3]) {
            k = 2;
        } else if (x < q[4]) {
            k = 3;
        } else {
            q[4] = x;
            k = 3;
        }

        for (int i = k + 1; i < 5; ++i) {
            npos[i] += 1;
        }
        for (int i = 0; i < 5; ++i) {
            np[i] += dn[i];
        }

        for (int i = 1; i < 4; ++i) {
            double d = np[i] - npos[i];
            if ((d >= 1.0 && npos[i+1] - npos[i] > 1) || (d <= -1.0 && npos[i-1] - npos[i] < -1)) {
                int d_int = (d > 0 ? 1 : -1);
                double newq = parabolic(i, d_int);
                if (q[i-1] < newq && newq < q[i+1]) {
                    q[i] = newq;
                } else {
                    q[i] = linear(i, d_int);
                }
                npos[i] += d_int;
            }
        }
    }

    double getQuantile() const {
        if (n < 5) {
            throw std::runtime_error("Not enough samples yet");
        }
        return q[2];
    }

private:
    double p;
    int n = 0;
    std::array<double, 5> initial{};
    std::array<double, 5> q{};
    std::array<double, 5> npos{};
    std::array<double, 5> np{};
    std::array<double, 5> dn{0, p/2, p, (1+p)/2, 1};

    double parabolic(int i, int d) const {
        return q[i] + d / (npos[i+1] - npos[i-1]) *
            ((npos[i] - npos[i-1] + d) * (q[i+1] - q[i]) / (npos[i+1] - npos[i]) +
             (npos[i+1] - npos[i] - d) * (q[i] - q[i-1]) / (npos[i] - npos[i-1]));
    }

    double linear(int i, int d) const {
        return q[i] + d * (q[i+d] - q[i]) / (npos[i+d] - npos[i]);
    }
};