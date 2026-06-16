#ifndef LOCAL_GCEM_HPP
#define LOCAL_GCEM_HPP

#define GCEM_PI 3.141592653589793238462643383279502884L
#define GCEM_HALF_PI 1.570796326794896619231321691639751442L
#define GCEM_LOG_10 2.302585092994045684017991454684364208L

namespace gcem {

constexpr double abs(double value) {
    return value < 0.0 ? -value : value;
}

constexpr double fabs(double value) {
    return abs(value);
}

constexpr double normalize_angle(double value) {
    while (value > GCEM_PI) {
        value -= 2.0 * GCEM_PI;
    }
    while (value < -GCEM_PI) {
        value += 2.0 * GCEM_PI;
    }
    return value;
}

constexpr double sin(double value) {
    value = normalize_angle(value);
    double term = value;
    double sum = value;
    const double x2 = value * value;
    for (int n = 1; n < 16; ++n) {
        term *= -x2 / static_cast<double>((2 * n) * (2 * n + 1));
        sum += term;
    }
    return sum;
}

constexpr double cos(double value) {
    value = normalize_angle(value);
    double term = 1.0;
    double sum = 1.0;
    const double x2 = value * value;
    for (int n = 1; n < 16; ++n) {
        term *= -x2 / static_cast<double>((2 * n - 1) * (2 * n));
        sum += term;
    }
    return sum;
}

constexpr double exp(double value) {
    if (value < 0.0) {
        return 1.0 / exp(-value);
    }

    double term = 1.0;
    double sum = 1.0;
    for (int n = 1; n < 96; ++n) {
        term *= value / static_cast<double>(n);
        sum += term;
    }
    return sum;
}

constexpr double log(double value) {
    if (value <= 0.0) {
        return 0.0;
    }

    int exponent = 0;
    while (value > 2.0) {
        value *= 0.5;
        ++exponent;
    }
    while (value < 0.5) {
        value *= 2.0;
        --exponent;
    }

    const double y = (value - 1.0) / (value + 1.0);
    const double y2 = y * y;
    double term = y;
    double sum = 0.0;
    for (int n = 1; n < 160; n += 2) {
        sum += term / static_cast<double>(n);
        term *= y2;
    }
    return (2.0 * sum) + (static_cast<double>(exponent) * 0.693147180559945309417232121458176568L);
}

constexpr double pow(double base, double exponent) {
    return exp(exponent * log(base));
}

} // namespace gcem

#endif
