#ifndef VRP_UTILS_H
#define VRP_UTILS_H

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

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
    try {
        size_t colon = time_str.find(':');
        if (colon == std::string::npos) return std::stoi(time_str);
        int hours = std::stoi(time_str.substr(0, colon));
        int mins = std::stoi(time_str.substr(colon + 1));
        return hours * 60 + mins;
    } catch (const std::exception&) {
        std::cerr << "Warning: Invalid time string '" << time_str << "', defaulting to 0" << std::endl;
        return 0;
    }
}

inline std::string min_to_time(int minutes) {
    // Clamp to valid range [0, 1440) to prevent negative or >24h output
    if (minutes < 0) minutes = 0;
    minutes = minutes % 1440;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << (minutes/60) << ":"
        << std::setfill('0') << std::setw(2) << (minutes%60);
    return oss.str();
}

inline void build_distance_matrix(const std::vector<std::pair<double,double>>& locs,
                                   std::vector<std::vector<double>>& dist,
                                   double multiplier = 1.0) {
    int n = locs.size();
    dist.resize(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (i != j) {
                double base_dist = haversine_km(locs[i].first, locs[i].second,
                                                locs[j].first, locs[j].second);
                dist[i][j] = base_dist * multiplier;
            }
}

// Vehicle mode inference from capacity (Phase 3B - Parragh Multi-Resource Model)
// 2-wheeler: capacity 1-2 (bike/auto), 4-wheeler: capacity 3-4 (sedan/SUV), van: capacity 5+ (minibus)
inline int infer_vehicle_mode(int capacity) {
    if (capacity <= 2) return 1;  // two_wheeler / auto
    if (capacity <= 4) return 2;  // four_wheeler / sedan
    return 3;                      // van / minibus
}

// Vehicle-employee mode compatibility check (Phase 3B)
// Returns false if the assignment is definitely incompatible
inline bool is_mode_compatible(int emp_priority, int emp_vehicle_pref, int veh_mode) {
    if (veh_mode == 1) {  // 2-wheeler
        // P1 employees should not ride 2-wheelers (safety/comfort)
        if (emp_priority <= 1) return false;
        // Premium preference employees should not ride 2-wheelers
        if (emp_vehicle_pref == 1) return false;
    }
    return true;
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
