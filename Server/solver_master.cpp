/*
 * KRITI 2026 - Employee Vehicle Routing Optimization
 * Master Solver - Competition Grade Solution
 * 
 * Algorithms: Hybrid (Greedy + Simulated Annealing + Local Search + ALNS)
 * Author: Optimized for Kriti 2026 Problem Statement
 */

#include <bits/stdc++.h>
using namespace std;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Penalty constants
const double PENALTY_SHARING = 3000.0;
const double PENALTY_VEHICLE_PREF = 1000.0;
const double PENALTY_UNASSIGNED = 10000.0;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct Employee {
    string id;
    int idx;
    int priority;           // 1-5 (1 = highest)
    double pickup_lat, pickup_lng;
    double drop_lat, drop_lng;
    int earliest_pickup;    // minutes from midnight
    int latest_drop;        // minutes from midnight
    int vehicle_pref;       // 0=any, 1=premium, 2=normal
    int sharing_pref;       // 1=single, 2=double, 3=triple
    double baseline_cost;
    double baseline_time;
    
    // Computed fields
    double direct_dist;     // direct distance to office
    double urgency_score;   // for sorting heuristics
};

struct Vehicle {
    string id;
    int idx;
    string fuel_type;
    string vehicle_type;
    int capacity;
    double cost_per_km;
    double avg_speed;
    double current_lat, current_lng;
    int available_from;     // minutes from midnight
    int category;           // 1=premium, 2=normal
    
    // Computed fields
    double efficiency_score;
};

struct Config {
    string test_case_id;
    string city;
    string distance_method;
    bool allow_external_maps;
    double cost_weight;
    double time_weight;
    int priority_max_delay[5];  // index 0 = priority 1
    
    // Office location (from employees' drop location)
    double office_lat, office_lng;
    
    // Algorithm parameters
    int max_iterations = 30000;
    double initial_temp = 10000.0;
    double cooling_rate = 0.9995;
    double reheat_threshold = 0.1;
    int no_improve_limit = 2000;
};

// Solution representation: routes[vehicle_idx] = ordered list of employee indices
typedef vector<vector<int>> Solution;

struct SolutionStats {
    double total_cost;
    double total_time;
    double total_penalty;
    double objective;
    int employees_served;
    int vehicles_used;
    map<string, double> breakdown;
};

// ============================================================================
// GLOBAL STATE
// ============================================================================

Config config;
vector<Employee> employees;
vector<Vehicle> vehicles;
double baseline_total_cost = 0;
double baseline_total_time = 0;
mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

// Distance cache for performance
map<pair<int,int>, double> dist_cache;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

int parseTime(const string& t) {
    // Parse "HH:MM" format to minutes from midnight
    int h = 0, m = 0;
    size_t colon = t.find(':');
    if (colon != string::npos) {
        h = stoi(t.substr(0, colon));
        m = stoi(t.substr(colon + 1));
    }
    return h * 60 + m;
}

string formatTime(int minutes) {
    int h = minutes / 60;
    int m = minutes % 60;
    char buf[10];
    sprintf(buf, "%02d:%02d", h, m);
    return string(buf);
}

double haversine(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0; // Earth radius in km
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double dphi = (lat2 - lat1) * M_PI / 180.0;
    double dlam = (lon2 - lon1) * M_PI / 180.0;
    
    double a = sin(dphi/2) * sin(dphi/2) +
               cos(phi1) * cos(phi2) * sin(dlam/2) * sin(dlam/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return R * c;
}

double getDistance(int from_type, int from_idx, int to_type, int to_idx) {
    // Types: 0=employee pickup, 1=employee drop, 2=vehicle start, 3=office
    auto key = make_pair(from_type * 1000 + from_idx, to_type * 1000 + to_idx);
    
    if (dist_cache.count(key)) return dist_cache[key];
    
    double lat1, lon1, lat2, lon2;
    
    // Get from coordinates
    if (from_type == 0) { lat1 = employees[from_idx].pickup_lat; lon1 = employees[from_idx].pickup_lng; }
    else if (from_type == 1) { lat1 = employees[from_idx].drop_lat; lon1 = employees[from_idx].drop_lng; }
    else if (from_type == 2) { lat1 = vehicles[from_idx].current_lat; lon1 = vehicles[from_idx].current_lng; }
    else { lat1 = config.office_lat; lon1 = config.office_lng; }
    
    // Get to coordinates
    if (to_type == 0) { lat2 = employees[to_idx].pickup_lat; lon2 = employees[to_idx].pickup_lng; }
    else if (to_type == 1) { lat2 = employees[to_idx].drop_lat; lon2 = employees[to_idx].drop_lng; }
    else if (to_type == 2) { lat2 = vehicles[to_idx].current_lat; lon2 = vehicles[to_idx].current_lng; }
    else { lat2 = config.office_lat; lon2 = config.office_lng; }
    
    double d = haversine(lat1, lon1, lat2, lon2);
    dist_cache[key] = d;
    return d;
}

double empToEmpDist(int e1, int e2) {
    return getDistance(0, e1, 0, e2);
}

double empToOfficeDist(int e) {
    return getDistance(0, e, 3, 0);
}

double vehicleToEmpDist(int v, int e) {
    return getDistance(2, v, 0, e);
}

// ============================================================================
// INPUT PARSING
// ============================================================================

void parseInput(const string& filename) {
    ifstream fin(filename);
    if (!fin.is_open()) {
        cerr << "Error: Cannot open file " << filename << endl;
        exit(1);
    }
    
    string line;
    string current_sheet = "";
    vector<string> headers;
    
    while (getline(fin, line)) {
        // Trim whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
            line.pop_back();
        if (line.empty()) continue;
        
        // Check for sheet marker
        if (line[0] == '#') {
            if (line.find("employees") != string::npos) current_sheet = "employees";
            else if (line.find("vehicles") != string::npos) current_sheet = "vehicles";
            else if (line.find("baseline") != string::npos) current_sheet = "baseline";
            else if (line.find("metadata") != string::npos) current_sheet = "metadata";
            headers.clear();
            continue;
        }
        
        // Parse tab-separated values
        vector<string> tokens;
        stringstream ss(line);
        string token;
        while (getline(ss, token, '\t')) {
            tokens.push_back(token);
        }
        
        if (tokens.empty()) continue;
        
        // First non-comment line in each sheet is header
        if (headers.empty()) {
            headers = tokens;
            continue;
        }
        
        // Parse data based on sheet
        if (current_sheet == "employees") {
            Employee e;
            for (size_t i = 0; i < tokens.size() && i < headers.size(); i++) {
                const string& h = headers[i];
                const string& v = tokens[i];
                
                if (h == "employee_id") e.id = v;
                else if (h == "priority") e.priority = stoi(v);
                else if (h == "pickup_lat") e.pickup_lat = stod(v);
                else if (h == "pickup_lng") e.pickup_lng = stod(v);
                else if (h == "drop_lat") e.drop_lat = stod(v);
                else if (h == "drop_lng") e.drop_lng = stod(v);
                else if (h == "earliest_pickup") e.earliest_pickup = parseTime(v);
                else if (h == "latest_drop") e.latest_drop = parseTime(v);
                else if (h == "vehicle_preference") {
                    if (v == "premium") e.vehicle_pref = 1;
                    else if (v == "normal") e.vehicle_pref = 2;
                    else e.vehicle_pref = 0;
                }
                else if (h == "sharing_preference") {
                    if (v == "single") e.sharing_pref = 1;
                    else if (v == "double") e.sharing_pref = 2;
                    else e.sharing_pref = 3;
                }
            }
            e.idx = employees.size();
            employees.push_back(e);
            
            // Set office location from first employee's drop
            if (employees.size() == 1) {
                config.office_lat = e.drop_lat;
                config.office_lng = e.drop_lng;
            }
        }
        else if (current_sheet == "vehicles") {
            Vehicle v;
            for (size_t i = 0; i < tokens.size() && i < headers.size(); i++) {
                const string& h = headers[i];
                const string& val = tokens[i];
                
                if (h == "vehicle_id") v.id = val;
                else if (h == "fuel_type") v.fuel_type = val;
                else if (h == "vehicle_type") v.vehicle_type = val;
                else if (h == "capacity") v.capacity = stoi(val);
                else if (h == "cost_per_km") v.cost_per_km = stod(val);
                else if (h == "avg_speed_kmph") v.avg_speed = stod(val);
                else if (h == "current_lat") v.current_lat = stod(val);
                else if (h == "current_lng") v.current_lng = stod(val);
                else if (h == "available_from") v.available_from = parseTime(val);
                else if (h == "category") {
                    if (val == "premium") v.category = 1;
                    else v.category = 2;
                }
            }
            v.idx = vehicles.size();
            vehicles.push_back(v);
        }
        else if (current_sheet == "baseline") {
            string emp_id = tokens[0];
            double cost = stod(tokens[1]);
            double time = stod(tokens[2]);
            
            for (auto& e : employees) {
                if (e.id == emp_id) {
                    e.baseline_cost = cost;
                    e.baseline_time = time;
                    baseline_total_cost += cost;
                    baseline_total_time += time;
                    break;
                }
            }
        }
        else if (current_sheet == "metadata") {
            if (tokens.size() >= 2) {
                const string& key = tokens[0];
                const string& val = tokens[1];
                
                if (key == "test_case_id") config.test_case_id = val;
                else if (key == "city") config.city = val;
                else if (key == "distance_method") config.distance_method = val;
                else if (key == "allow_external_maps") config.allow_external_maps = (val == "TRUE");
                else if (key == "objective_cost_weight") config.cost_weight = stod(val);
                else if (key == "objective_time_weight") config.time_weight = stod(val);
                else if (key == "priority_1_max_delay_min") config.priority_max_delay[0] = stoi(val);
                else if (key == "priority_2_max_delay_min") config.priority_max_delay[1] = stoi(val);
                else if (key == "priority_3_max_delay_min") config.priority_max_delay[2] = stoi(val);
                else if (key == "priority_4_max_delay_min") config.priority_max_delay[3] = stoi(val);
                else if (key == "priority_5_max_delay_min") config.priority_max_delay[4] = stoi(val);
            }
        }
    }
    
    // Compute derived fields
    for (auto& e : employees) {
        e.direct_dist = haversine(e.pickup_lat, e.pickup_lng, config.office_lat, config.office_lng);
        // Urgency: higher priority + tighter time window = more urgent
        double time_window = e.latest_drop - e.earliest_pickup;
        e.urgency_score = (6 - e.priority) * 100 + (1000.0 / (time_window + 1));
    }
    
    for (auto& v : vehicles) {
        // Efficiency: capacity / cost (prefer high capacity, low cost)
        v.efficiency_score = v.capacity / v.cost_per_km;
    }
}

// ============================================================================
// ROUTE EVALUATION
// ============================================================================

struct RouteResult {
    double distance;
    double time;
    double penalty;
    vector<double> pickup_times;
    vector<double> arrival_times;
    double drop_time;
    bool feasible;
};

RouteResult evaluateRoute(int v_idx, const vector<int>& route) {
    RouteResult result;
    result.distance = 0;
    result.time = 0;
    result.penalty = 0;
    result.feasible = true;
    result.drop_time = 0;
    
    if (route.empty()) return result;
    
    const Vehicle& v = vehicles[v_idx];
    
    double cur_lat = v.current_lat;
    double cur_lng = v.current_lng;
    double cur_time = v.available_from;
    
    // Capacity check
    if ((int)route.size() > v.capacity) {
        result.penalty += 5000.0 * (route.size() - v.capacity);
        result.feasible = false;
    }
    
    // Sharing preference check - STRICT enforcement
    for (int e_idx : route) {
        const Employee& emp = employees[e_idx];
        if (emp.sharing_pref == 1 && route.size() > 1) {
            // Single preference employee CANNOT share - EXTREMELY high penalty
            result.penalty += 50000.0; // Make it prohibitively expensive
            result.feasible = false;
        }
        if (emp.sharing_pref == 2 && route.size() > 2) {
            // Double preference - can share with at most 1 other person
            result.penalty += 3000.0;
        }
        // Triple (3) or any - no penalty for sharing
        
        // Vehicle preference check
        if (emp.vehicle_pref == 1 && v.category != 1) {
            // Premium employee in non-premium vehicle
            result.penalty += 800.0 * (6 - emp.priority); // Higher penalty for high priority
        }
        else if (emp.vehicle_pref == 2 && v.category == 1) {
            // Normal preference in premium vehicle (waste)
            result.penalty += 50.0;
        }
    }
    
    // Also check if any single-preference employee is paired (redundant but extra strict)
    int single_count = 0;
    for (int e_idx : route) {
        if (employees[e_idx].sharing_pref == 1) single_count++;
    }
    if (single_count > 0 && route.size() > 1) {
        // Each single-preference employee gets heavy penalty when grouped
        result.penalty += single_count * 50000.0;
        result.feasible = false;
    }
    
    // Traverse route - pickup all employees then go to office
    for (int e_idx : route) {
        const Employee& emp = employees[e_idx];
        
        double d = haversine(cur_lat, cur_lng, emp.pickup_lat, emp.pickup_lng);
        result.distance += d;
        
        double travel_time = (d / v.avg_speed) * 60.0; // minutes
        cur_time += travel_time;
        
        // Wait if arrived early
        if (cur_time < emp.earliest_pickup) {
            cur_time = emp.earliest_pickup;
        }
        
        // Penalty for arriving too early (vehicle waiting)
        if (cur_time < emp.earliest_pickup - 15) {
            result.penalty += (emp.earliest_pickup - 15 - cur_time) * 2;
        }
        
        result.pickup_times.push_back(cur_time);
        result.arrival_times.push_back(cur_time);
        
        cur_lat = emp.pickup_lat;
        cur_lng = emp.pickup_lng;
    }
    
    // Go to office
    double d_office = haversine(cur_lat, cur_lng, config.office_lat, config.office_lng);
    result.distance += d_office;
    double travel_office = (d_office / v.avg_speed) * 60.0;
    cur_time += travel_office;
    result.drop_time = cur_time;
    
    // Check time constraints for all employees
    for (size_t i = 0; i < route.size(); i++) {
        int e_idx = route[i];
        const Employee& emp = employees[e_idx];
        
        double ride_time = result.drop_time - result.pickup_times[i];
        result.time += ride_time;
        
        // Priority-based delay check
        int priority_idx = max(0, min(4, emp.priority - 1));
        int max_delay = config.priority_max_delay[priority_idx];
        
        if (result.drop_time > emp.latest_drop + max_delay) {
            // Hard constraint violation
            double excess = result.drop_time - emp.latest_drop - max_delay;
            double priority_mult = 6.0 - emp.priority;
            result.penalty += (2000.0 + excess * 50.0) * priority_mult;
            result.feasible = false;
        }
        else if (result.drop_time > emp.latest_drop) {
            // Within allowed delay but still late
            double delay = result.drop_time - emp.latest_drop;
            double priority_mult = 6.0 - emp.priority;
            result.penalty += delay * 5.0 * priority_mult;
        }
    }
    
    return result;
}

// ============================================================================
// SOLUTION EVALUATION
// ============================================================================

SolutionStats evaluateSolution(const Solution& sol) {
    SolutionStats stats;
    stats.total_cost = 0;
    stats.total_time = 0;
    stats.total_penalty = 0;
    stats.employees_served = 0;
    stats.vehicles_used = 0;
    
    for (size_t v_idx = 0; v_idx < sol.size(); v_idx++) {
        const auto& route = sol[v_idx];
        if (route.empty()) continue;
        
        stats.vehicles_used++;
        stats.employees_served += route.size();
        
        RouteResult rr = evaluateRoute(v_idx, route);
        stats.total_cost += rr.distance * vehicles[v_idx].cost_per_km;
        stats.total_time += rr.time;
        stats.total_penalty += rr.penalty;
    }
    
    // Check for unassigned employees
    set<int> assigned;
    for (const auto& route : sol) {
        for (int e : route) assigned.insert(e);
    }
    int unassigned = employees.size() - assigned.size();
    stats.total_penalty += unassigned * 10000.0; // Heavy penalty for unassigned
    
    // Compute objective
    double norm_cost = stats.total_cost / (baseline_total_cost + 1e-6);
    double norm_time = stats.total_time / (baseline_total_time + 1e-6);
    stats.objective = config.cost_weight * norm_cost + 
                      config.time_weight * norm_time + 
                      stats.total_penalty / 10000.0;
    
    stats.breakdown["cost"] = stats.total_cost;
    stats.breakdown["time"] = stats.total_time;
    stats.breakdown["penalty"] = stats.total_penalty;
    stats.breakdown["norm_cost"] = norm_cost;
    stats.breakdown["norm_time"] = norm_time;
    
    return stats;
}

// ============================================================================
// ROUTE OPTIMIZATION (TSP-like for pickup sequence)
// ============================================================================

void optimizeRouteSequence(int v_idx, vector<int>& route) {
    if (route.size() <= 1) return;
    
    // For small routes, try all permutations
    if (route.size() <= 6) {
        vector<int> best_route = route;
        double best_penalty = 1e18;
        
        sort(route.begin(), route.end());
        do {
            RouteResult rr = evaluateRoute(v_idx, route);
            double score = rr.distance * vehicles[v_idx].cost_per_km + rr.time + rr.penalty;
            if (score < best_penalty) {
                best_penalty = score;
                best_route = route;
            }
        } while (next_permutation(route.begin(), route.end()));
        
        route = best_route;
    }
    else {
        // For larger routes, use 2-opt improvement
        bool improved = true;
        while (improved) {
            improved = false;
            for (size_t i = 0; i < route.size() - 1; i++) {
                for (size_t j = i + 2; j < route.size(); j++) {
                    // Try reversing segment [i+1, j]
                    vector<int> new_route = route;
                    reverse(new_route.begin() + i + 1, new_route.begin() + j + 1);
                    
                    RouteResult rr_old = evaluateRoute(v_idx, route);
                    RouteResult rr_new = evaluateRoute(v_idx, new_route);
                    
                    double old_score = rr_old.distance + rr_old.penalty;
                    double new_score = rr_new.distance + rr_new.penalty;
                    
                    if (new_score < old_score - 0.001) {
                        route = new_route;
                        improved = true;
                    }
                }
            }
        }
    }
}

// ============================================================================
// FEASIBILITY CHECK FOR SHARING PREFERENCES
// These are now SOFT constraints - we penalize violations but don't block
// ============================================================================

// Count sharing violations in a route
int countSharingViolations(const vector<int>& route) {
    if (route.empty()) return 0;
    
    int violations = 0;
    for (int e_idx : route) {
        const Employee& emp = employees[e_idx];
        // Single preference: wants to be alone
        if (emp.sharing_pref == 1 && route.size() > 1) violations++;
        // Double preference: wants max 2 people
        if (emp.sharing_pref == 2 && route.size() > 2) violations++;
        // Triple preference: max 3 is implicitly handled by capacity usually
        if (emp.sharing_pref == 3 && route.size() > 3) violations++;
    }
    return violations;
}

// Check if route is strictly feasible for sharing (used for greedy priority)
bool isRouteFeasibleForSharing(const vector<int>& route) {
    return countSharingViolations(route) == 0;
}

// Check if relocation would be feasible - now returns penalty instead of bool
int relocationSharingPenalty(const Solution& sol, int v_to, int emp_idx) {
    // Check capacity first - this is still a hard constraint
    if ((int)sol[v_to].size() >= vehicles[v_to].capacity) return 1000000;
    
    vector<int> test_route = sol[v_to];
    test_route.push_back(emp_idx);
    
    return countSharingViolations(test_route) * PENALTY_SHARING;
}

bool isRelocationFeasible(const Solution& sol, int v_to, int emp_idx) {
    // Only check capacity - sharing is now soft
    return (int)sol[v_to].size() < vehicles[v_to].capacity;
}

// ============================================================================
// INITIAL SOLUTION CONSTRUCTION
// ============================================================================

Solution constructGreedySolution() {
    Solution sol(vehicles.size());
    
    // Sort employees by urgency (priority + time window tightness)
    vector<int> emp_order(employees.size());
    iota(emp_order.begin(), emp_order.end(), 0);
    
    // Sort employees: single preference FIRST, then by urgency
    sort(emp_order.begin(), emp_order.end(), [](int a, int b) {
        // Single-preference employees get absolute priority for empty vehicles
        if (employees[a].sharing_pref != employees[b].sharing_pref) {
            return employees[a].sharing_pref < employees[b].sharing_pref;
        }
        return employees[a].urgency_score > employees[b].urgency_score;
    });
    
    // Sort vehicles: premium first, but ALSO consider capacity=1 vehicles for single
    vector<int> veh_order(vehicles.size());
    iota(veh_order.begin(), veh_order.end(), 0);
    sort(veh_order.begin(), veh_order.end(), [](int a, int b) {
        if (vehicles[a].category != vehicles[b].category)
            return vehicles[a].category < vehicles[b].category;
        return vehicles[a].efficiency_score > vehicles[b].efficiency_score;
    });
    
    set<int> assigned;
    
    // Pass 0: Assign SINGLE preference employees to dedicated vehicles
    // This is critical - they MUST get their own vehicle
    for (int e_idx : emp_order) {
        if (assigned.count(e_idx)) continue;
        const Employee& emp = employees[e_idx];
        
        if (emp.sharing_pref != 1) continue; // Only single-pref in this pass
        
        int best_v = -1;
        double best_score = 1e18;
        
        for (int v_idx : veh_order) {
            if (!sol[v_idx].empty()) continue; // MUST be empty for single-pref
            
            // Prefer matching vehicle type (premium gets premium)
            double type_bonus = 0;
            if (emp.vehicle_pref == 1 && vehicles[v_idx].category == 1) type_bonus = -1000;
            if (emp.vehicle_pref == 2 && vehicles[v_idx].category == 2) type_bonus = -500;
            
            double dist = vehicleToEmpDist(v_idx, e_idx);
            double score = dist + type_bonus;
            
            if (score < best_score) {
                best_score = score;
                best_v = v_idx;
            }
        }
        
        if (best_v >= 0) {
            sol[best_v].push_back(e_idx);
            assigned.insert(e_idx);
        }
    }
    
    // First pass: assign premium employees to premium vehicles (if still unassigned)
    for (int e_idx : emp_order) {
        if (assigned.count(e_idx)) continue;
        const Employee& emp = employees[e_idx];
        
        if (emp.vehicle_pref == 1) { // Premium preference
            int best_v = -1;
            double best_score = 1e18;
            
            for (int v_idx : veh_order) {
                if (vehicles[v_idx].category != 1) continue; // Only premium
                if ((int)sol[v_idx].size() >= vehicles[v_idx].capacity) continue;
                
                // Check sharing preference compatibility
                if (emp.sharing_pref == 1 && !sol[v_idx].empty()) continue;
                if (!sol[v_idx].empty()) {
                    bool compat = true;
                    for (int e : sol[v_idx]) {
                        if (employees[e].sharing_pref == 1) { compat = false; break; }
                    }
                    if (!compat) continue;
                }
                
                // Score: distance + time window compatibility
                double dist = vehicleToEmpDist(v_idx, e_idx);
                if (!sol[v_idx].empty()) {
                    dist = empToEmpDist(sol[v_idx].back(), e_idx);
                }
                
                if (dist < best_score) {
                    best_score = dist;
                    best_v = v_idx;
                }
            }
            
            if (best_v >= 0) {
                sol[best_v].push_back(e_idx);
                assigned.insert(e_idx);
            }
        }
    }
    
    // Second pass: assign remaining employees - use soft constraints
    for (int e_idx : emp_order) {
        if (assigned.count(e_idx)) continue;
        const Employee& emp = employees[e_idx];
        
        int best_v = -1;
        double best_score = 1e18;
        
        for (int v_idx : veh_order) {
            if ((int)sol[v_idx].size() >= vehicles[v_idx].capacity) continue;
            
            // Try to respect sharing preferences when possible
            // But don't block - we'll penalize violations
            bool skip_soft = false;
            
            // If current employee wants single and vehicle is not empty, 
            // try to find an empty one first (soft preference)
            if (emp.sharing_pref == 1 && !sol[v_idx].empty()) skip_soft = true;
            
            // If current employee wants double and vehicle already has 2+
            if (emp.sharing_pref == 2 && sol[v_idx].size() >= 2) skip_soft = true;
            
            // Check existing employees - if any wants single, prefer elsewhere
            for (int e : sol[v_idx]) {
                if (employees[e].sharing_pref == 1) { skip_soft = true; break; }
            }
            
            // Compute insertion cost including penalties
            vector<int> test_route = sol[v_idx];
            test_route.push_back(e_idx);
            RouteResult rr = evaluateRoute(v_idx, test_route);
            
            double score = rr.distance * vehicles[v_idx].cost_per_km + rr.penalty;
            
            // Add soft preference bonus/penalty
            if (skip_soft) score += 10000; // Prefer other options but don't block
            
            if (score < best_score) {
                best_score = score;
                best_v = v_idx;
            }
        }
        
        if (best_v >= 0) {
            sol[best_v].push_back(e_idx);
            assigned.insert(e_idx);
        }
    }
    
    // Emergency pass: FORCE assign any remaining employees to minimize penalty
    for (int e_idx : emp_order) {
        if (assigned.count(e_idx)) continue;
        
        int best_v = -1;
        double best_score = 1e18;
        
        for (size_t v_idx = 0; v_idx < vehicles.size(); v_idx++) {
            // Only check capacity - it's the only hard constraint
            if ((int)sol[v_idx].size() >= vehicles[v_idx].capacity) continue;
            
            vector<int> test_route = sol[v_idx];
            test_route.push_back(e_idx);
            
            RouteResult rr = evaluateRoute(v_idx, test_route);
            // Score includes penalties for violations
            double score = rr.penalty + rr.distance * vehicles[v_idx].cost_per_km;
            
            if (score < best_score) {
                best_score = score;
                best_v = v_idx;
            }
        }
        
        if (best_v >= 0) {
            sol[best_v].push_back(e_idx);
            assigned.insert(e_idx);
        } else {
            // This should never happen if total capacity >= employees
            cerr << "ERROR: Cannot assign employee " << employees[e_idx].id 
                 << " - insufficient total capacity!" << endl;
        }
    }
    
    // Final check - ensure all employees assigned
    if (assigned.size() != employees.size()) {
        cerr << "ERROR: Only " << assigned.size() << "/" << employees.size() 
             << " employees assigned!" << endl;
    }
    
    // Optimize each route sequence
    for (size_t v = 0; v < sol.size(); v++) {
        optimizeRouteSequence(v, sol[v]);
    }
    
    return sol;
}

// ============================================================================
// NEIGHBORHOOD OPERATORS
// ============================================================================

Solution relocateEmployee(const Solution& sol) {
    Solution new_sol = sol;
    
    // Find a non-empty route
    vector<int> non_empty;
    for (size_t v = 0; v < sol.size(); v++) {
        if (!sol[v].empty()) non_empty.push_back(v);
    }
    if (non_empty.empty()) return new_sol;
    
    // Pick a random employee to relocate
    int v_from = non_empty[rng() % non_empty.size()];
    int pos = rng() % new_sol[v_from].size();
    int emp = new_sol[v_from][pos];
    
    // Find best destination
    int best_v = -1;
    int best_pos = 0;
    double best_cost = 1e18;
    
    // Remove employee from source
    new_sol[v_from].erase(new_sol[v_from].begin() + pos);
    
    for (size_t v_to = 0; v_to < vehicles.size(); v_to++) {
        // Check capacity
        if ((int)new_sol[v_to].size() >= vehicles[v_to].capacity) continue;
        
        for (size_t insert_pos = 0; insert_pos <= new_sol[v_to].size(); insert_pos++) {
            vector<int> test = new_sol[v_to];
            test.insert(test.begin() + insert_pos, emp);
            
            RouteResult rr = evaluateRoute(v_to, test);
            double cost = rr.distance * vehicles[v_to].cost_per_km + rr.penalty;
            
            if (cost < best_cost) {
                best_cost = cost;
                best_v = v_to;
                best_pos = insert_pos;
            }
        }
    }
    
    if (best_v >= 0) {
        new_sol[best_v].insert(new_sol[best_v].begin() + best_pos, emp);
    } else {
        // No feasible position found - put back in original
        new_sol[v_from].insert(new_sol[v_from].begin() + pos, emp);
    }
    
    return new_sol;
}

Solution swapEmployees(const Solution& sol) {
    Solution new_sol = sol;
    
    vector<int> non_empty;
    for (size_t v = 0; v < sol.size(); v++) {
        if (!sol[v].empty()) non_empty.push_back(v);
    }
    if (non_empty.size() < 2) return new_sol;
    
    int idx1 = rng() % non_empty.size();
    int idx2 = rng() % non_empty.size();
    while (idx2 == idx1) idx2 = rng() % non_empty.size();
    
    int v1 = non_empty[idx1];
    int v2 = non_empty[idx2];
    
    int pos1 = rng() % new_sol[v1].size();
    int pos2 = rng() % new_sol[v2].size();
    
    swap(new_sol[v1][pos1], new_sol[v2][pos2]);
    
    return new_sol;
}

Solution swapWithinRoute(const Solution& sol) {
    Solution new_sol = sol;
    
    vector<int> has_multiple;
    for (size_t v = 0; v < sol.size(); v++) {
        if (sol[v].size() >= 2) has_multiple.push_back(v);
    }
    if (has_multiple.empty()) return new_sol;
    
    int v = has_multiple[rng() % has_multiple.size()];
    int pos1 = rng() % new_sol[v].size();
    int pos2 = rng() % new_sol[v].size();
    while (pos2 == pos1) pos2 = rng() % new_sol[v].size();
    
    swap(new_sol[v][pos1], new_sol[v][pos2]);
    
    return new_sol;
}

Solution twoOptWithinRoute(const Solution& sol) {
    Solution new_sol = sol;
    
    vector<int> has_multiple;
    for (size_t v = 0; v < sol.size(); v++) {
        if (sol[v].size() >= 3) has_multiple.push_back(v);
    }
    if (has_multiple.empty()) return new_sol;
    
    int v = has_multiple[rng() % has_multiple.size()];
    int i = rng() % (new_sol[v].size() - 1);
    int j = i + 1 + rng() % (new_sol[v].size() - i - 1);
    
    reverse(new_sol[v].begin() + i, new_sol[v].begin() + j + 1);
    
    return new_sol;
}

Solution destroyAndRepair(const Solution& sol, int destroy_count) {
    Solution new_sol = sol;
    vector<int> removed;
    
    // Destroy: remove random employees
    for (int d = 0; d < destroy_count; d++) {
        vector<int> non_empty;
        for (size_t v = 0; v < new_sol.size(); v++) {
            if (!new_sol[v].empty()) non_empty.push_back(v);
        }
        if (non_empty.empty()) break;
        
        int v = non_empty[rng() % non_empty.size()];
        int pos = rng() % new_sol[v].size();
        removed.push_back(new_sol[v][pos]);
        new_sol[v].erase(new_sol[v].begin() + pos);
    }
    
    // Repair: reinsert ALL removed employees (MUST reinsert everyone)
    shuffle(removed.begin(), removed.end(), rng);
    
    for (int emp : removed) {
        int best_v = -1;
        int best_pos = 0;
        double best_cost = 1e18;
        
        // First try to find a "nice" position (respecting soft constraints)
        for (size_t v = 0; v < new_sol.size(); v++) {
            if ((int)new_sol[v].size() >= vehicles[v].capacity) continue;
            
            for (size_t pos = 0; pos <= new_sol[v].size(); pos++) {
                vector<int> test = new_sol[v];
                test.insert(test.begin() + pos, emp);
                
                RouteResult rr = evaluateRoute(v, test);
                double cost = rr.distance * vehicles[v].cost_per_km + rr.penalty;
                
                if (cost < best_cost) {
                    best_cost = cost;
                    best_v = v;
                    best_pos = pos;
                }
            }
        }
        
        if (best_v >= 0) {
            new_sol[best_v].insert(new_sol[best_v].begin() + best_pos, emp);
        } else {
            // Should never happen if capacity is sufficient, but force insert
            for (size_t v = 0; v < new_sol.size(); v++) {
                if ((int)new_sol[v].size() < vehicles[v].capacity) {
                    new_sol[v].push_back(emp);
                    break;
                }
            }
        }
    }
    
    return new_sol;
}

// ============================================================================
// SIMULATED ANNEALING
// ============================================================================

Solution simulatedAnnealing(const Solution& initial) {
    Solution current = initial;
    Solution best = current;
    
    SolutionStats current_stats = evaluateSolution(current);
    SolutionStats best_stats = current_stats;
    
    double temp = config.initial_temp;
    int no_improve = 0;
    
    for (int iter = 0; iter < config.max_iterations; iter++) {
        // Generate neighbor
        Solution neighbor;
        double r = (double)rng() / rng.max();
        
        if (r < 0.3) neighbor = relocateEmployee(current);
        else if (r < 0.5) neighbor = swapEmployees(current);
        else if (r < 0.65) neighbor = swapWithinRoute(current);
        else if (r < 0.8) neighbor = twoOptWithinRoute(current);
        else neighbor = destroyAndRepair(current, 2 + rng() % 4);
        
        SolutionStats neighbor_stats = evaluateSolution(neighbor);
        
        double delta = neighbor_stats.objective - current_stats.objective;
        
        if (delta < 0 || (double)rng() / rng.max() < exp(-delta / temp)) {
            current = neighbor;
            current_stats = neighbor_stats;
            
            if (current_stats.objective < best_stats.objective) {
                best = current;
                best_stats = current_stats;
                no_improve = 0;
            }
        }
        
        no_improve++;
        
        // Cooling
        temp *= config.cooling_rate;
        
        // Reheat if stuck
        if (no_improve > config.no_improve_limit) {
            temp = config.initial_temp * config.reheat_threshold;
            no_improve = 0;
            
            // Apply local search to best
            for (size_t v = 0; v < best.size(); v++) {
                optimizeRouteSequence(v, best[v]);
            }
            current = best;
            current_stats = evaluateSolution(current);
        }
        
        // Progress output
        if (iter % 5000 == 0) {
            cerr << "Iter " << iter << " | Temp: " << fixed << setprecision(2) << temp 
                 << " | Best: " << best_stats.objective 
                 << " | Cost: " << best_stats.total_cost
                 << " | Penalty: " << best_stats.total_penalty << endl;
        }
    }
    
    return best;
}

// ============================================================================
// OUTPUT FORMATTING
// ============================================================================

void outputJSON(const Solution& sol, const SolutionStats& stats) {
    cout << "{" << endl;
    cout << "  \"test_case_id\": \"" << config.test_case_id << "\"," << endl;
    cout << "  \"objective\": " << fixed << setprecision(6) << stats.objective << "," << endl;
    cout << "  \"stats\": {" << endl;
    cout << "    \"total_cost\": " << stats.total_cost << "," << endl;
    cout << "    \"total_time\": " << stats.total_time << "," << endl;
    cout << "    \"total_penalty\": " << stats.total_penalty << "," << endl;
    cout << "    \"employees_served\": " << stats.employees_served << "," << endl;
    cout << "    \"vehicles_used\": " << stats.vehicles_used << endl;
    cout << "  }," << endl;
    
    cout << "  \"routes\": {" << endl;
    bool first_v = true;
    for (size_t v = 0; v < sol.size(); v++) {
        if (sol[v].empty()) continue;
        
        if (!first_v) cout << "," << endl;
        first_v = false;
        
        cout << "    \"" << vehicles[v].id << "\": {" << endl;
        cout << "      \"employees\": [";
        for (size_t i = 0; i < sol[v].size(); i++) {
            if (i > 0) cout << ", ";
            cout << "\"" << employees[sol[v][i]].id << "\"";
        }
        cout << "]," << endl;
        
        // Route details
        RouteResult rr = evaluateRoute(v, sol[v]);
        cout << "      \"distance_km\": " << fixed << setprecision(2) << rr.distance << "," << endl;
        cout << "      \"total_time_min\": " << rr.time << "," << endl;
        cout << "      \"drop_time\": \"" << formatTime((int)rr.drop_time) << "\"," << endl;
        
        cout << "      \"pickups\": [" << endl;
        for (size_t i = 0; i < sol[v].size(); i++) {
            int e_idx = sol[v][i];
            cout << "        {";
            cout << "\"employee\": \"" << employees[e_idx].id << "\", ";
            cout << "\"pickup_time\": \"" << formatTime((int)rr.pickup_times[i]) << "\", ";
            cout << "\"earliest\": \"" << formatTime(employees[e_idx].earliest_pickup) << "\", ";
            cout << "\"latest_drop\": \"" << formatTime(employees[e_idx].latest_drop) << "\"";
            cout << "}";
            if (i < sol[v].size() - 1) cout << ",";
            cout << endl;
        }
        cout << "      ]" << endl;
        cout << "    }";
    }
    cout << endl << "  }," << endl;
    
    // Assignment map for compatibility
    cout << "  \"assignment\": {" << endl;
    first_v = true;
    for (size_t v = 0; v < sol.size(); v++) {
        if (!first_v) cout << "," << endl;
        first_v = false;
        
        cout << "    \"" << vehicles[v].id << "\": [";
        for (size_t i = 0; i < sol[v].size(); i++) {
            if (i > 0) cout << ", ";
            cout << "\"" << employees[sol[v][i]].id << "\"";
        }
        cout << "]";
    }
    cout << endl << "  }" << endl;
    cout << "}" << endl;
}

// Compact JSON output for streaming
void outputCompactJSON(int gen, const Solution& sol, const SolutionStats& stats) {
    cout << "{\"generation\": " << gen 
         << ", \"score\": " << fixed << setprecision(4) << stats.objective
         << ", \"stats\": {\"cost\": " << setprecision(2) << stats.total_cost 
         << ", \"time\": " << stats.total_time 
         << ", \"penalty\": " << stats.total_penalty << "}"
         << ", \"assignment\": {";
    
    bool first = true;
    for (size_t v = 0; v < sol.size(); v++) {
        if (!first) cout << ", ";
        first = false;
        cout << "\"" << vehicles[v].id << "\": [";
        for (size_t i = 0; i < sol[v].size(); i++) {
            if (i > 0) cout << ",";
            cout << "\"" << employees[sol[v][i]].id << "\"";
        }
        cout << "]";
    }
    cout << "}}" << endl;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input_file> [--verbose]" << endl;
        return 1;
    }
    
    bool verbose = (argc > 2 && string(argv[2]) == "--verbose");
    
    // Parse input
    parseInput(argv[1]);
    
    cerr << "=== KRITI 2026 Vehicle Routing Optimizer ===" << endl;
    cerr << "Test Case: " << config.test_case_id << endl;
    cerr << "Employees: " << employees.size() << endl;
    cerr << "Vehicles: " << vehicles.size() << endl;
    cerr << "Baseline Cost: " << baseline_total_cost << endl;
    cerr << "Baseline Time: " << baseline_total_time << endl;
    cerr << "=============================================" << endl;
    
    // Construct initial solution
    cerr << "Constructing initial solution..." << endl;
    Solution initial = constructGreedySolution();
    SolutionStats initial_stats = evaluateSolution(initial);
    cerr << "Initial: Obj=" << initial_stats.objective 
         << " Cost=" << initial_stats.total_cost
         << " Time=" << initial_stats.total_time
         << " Penalty=" << initial_stats.total_penalty << endl;
    
    // Run optimization
    cerr << "Running Simulated Annealing..." << endl;
    Solution best = simulatedAnnealing(initial);
    
    // Final local search
    cerr << "Final local search..." << endl;
    for (size_t v = 0; v < best.size(); v++) {
        optimizeRouteSequence(v, best[v]);
    }
    
    SolutionStats final_stats = evaluateSolution(best);
    
    cerr << "=============================================" << endl;
    cerr << "FINAL RESULT:" << endl;
    cerr << "  Objective: " << final_stats.objective << endl;
    cerr << "  Cost: " << final_stats.total_cost << endl;
    cerr << "  Time: " << final_stats.total_time << endl;
    cerr << "  Penalty: " << final_stats.total_penalty << endl;
    cerr << "  Vehicles Used: " << final_stats.vehicles_used << endl;
    cerr << "  Employees Served: " << final_stats.employees_served << "/" << employees.size() << endl;
    cerr << "=============================================" << endl;
    
    // Output result
    if (verbose) {
        outputJSON(best, final_stats);
    } else {
        outputCompactJSON(config.max_iterations, best, final_stats);
    }
    
    return 0;
}
