#ifndef VRP_UTILS_H
#define VRP_UTILS_H

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

inline double haversine_km(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0;
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double dphi = (lat2 - lat1) * M_PI / 180.0;
    double dlambda = (lon2 - lon1) * M_PI / 180.0;
    double a = std::sin(dphi/2) * std::sin(dphi/2) + 
               std::cos(phi1) * std::cos(phi2) * std::sin(dlambda/2) * std::sin(dlambda/2);
    return R * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

inline int time_to_min(const std::string& time_str) {
    if (time_str.empty()) return 0;
    size_t colon = time_str.find(':');
    if (colon == std::string::npos) return std::stoi(time_str);
    return std::stoi(time_str.substr(0, colon)) * 60 + std::stoi(time_str.substr(colon + 1));
}

inline std::string min_to_time(int minutes) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << (minutes/60) << ":"
        << std::setfill('0') << std::setw(2) << (minutes%60);
    return oss.str();
}

inline void build_distance_matrix(const std::vector<std::pair<double,double>>& locs,
                                   std::vector<std::vector<double>>& dist) {
    int n = locs.size();
    dist.resize(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (i != j) dist[i][j] = haversine_km(locs[i].first, locs[i].second,
                                                    locs[j].first, locs[j].second);
}

inline int get_category_code(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "premium") return 1;
    if (lower == "normal") return 2;
    return 0;
}

inline int get_sharing_code(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "single") return 1;
    if (lower == "double") return 2;
    return 3;
}

#endif
