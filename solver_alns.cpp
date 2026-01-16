#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <algorithm>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>
#include <set>

using namespace std;

// --- Data Structures ---
struct Employee {
    string id;
    int int_id;
    double pickup_lat, pickup_lng;
    int earliest_pickup, latest_drop;     
    int sharing_pref;    
    double baseline_cost, baseline_time;
};

struct Vehicle {
    string id;
    int int_id;
    int capacity;
    double speed_kmph;
    double cost_per_km;
    double current_lat, current_lng;
    int available_from; 
};

struct Config {
    double cost_weight;
    double time_weight;
    int pop_size;
    int generations; 
    double office_lat, office_lng;
    int slack = 30; 
};

Config config;
vector<Employee> employees;
vector<Vehicle> vehicles;
double baseline_total_cost = 0;
double baseline_total_time = 0;
mt19937 rng;

// --- Helper Functions ---
double haversine_km(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat/2) * sin(dlat/2) + cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) * sin(dlon/2) * sin(dlon/2);
    return R * 2 * atan2(sqrt(a), sqrt(1 - a));
}

// --- Optimization Logic ---

// Optimizes the sequence of pickups for a SINGLE vehicle.
// Since all dropoffs are at the OFFICE, we only need to order the PICKUPS.
// The route vector contains Employee IDs.
void optimize_route_sequence(int v_idx, vector<int>& route, double& out_dist, double& out_time, double& out_penalty) {
    if (route.empty()) {
        out_dist = 0; out_time = 0; out_penalty = 0;
        return;
    }

    const auto& v = vehicles[v_idx];
    
    // HEURISTIC: Sort by Earliest Pickup Time (Temporal Sorting)
    // This is much faster and often better than brute force for VRP
    sort(route.begin(), route.end(), [](int a, int b){
        return employees[a].earliest_pickup < employees[b].earliest_pickup;
    });

    // We can try local swaps (2-opt) if the route is small enough
    bool try_swaps = route.size() <= 8; 

    double best_score = 1e18;
    vector<int> best_perm = route;

    // lambda to evaluate current specific permutation
    auto evaluate_perm = [&](const vector<int>& current_route) {
        double current_dist = 0;
        double current_penalty = 0;
        double current_total_emp_time = 0;
        
        double cur_lat = v.current_lat;
        double cur_lng = v.current_lng;
        double cur_time = v.available_from;
        
        vector<double> pickup_times;
        pickup_times.reserve(current_route.size());

        // 1. Visit Pickups
        for (int e_idx : current_route) {
            const auto& emp = employees[e_idx];
            double d = haversine_km(cur_lat, cur_lng, emp.pickup_lat, emp.pickup_lng);
            current_dist += d;
            
            double travel = (d / v.speed_kmph) * 60.0;
            cur_time += travel;
            
            // Wait if early
            if (emp.earliest_pickup > 0 && cur_time < emp.earliest_pickup) {
                cur_time = emp.earliest_pickup;
            }
            
            pickup_times.push_back(cur_time);
            cur_lat = emp.pickup_lat;
            cur_lng = emp.pickup_lng;
        }
        
        // 2. Go to Office (Dropoff All)
        double d_office = haversine_km(cur_lat, cur_lng, config.office_lat, config.office_lng);
        current_dist += d_office;
        cur_time += (d_office / v.speed_kmph) * 60.0;
        double drop_time = cur_time;

        // 3. Calculate Penalties & Metrics
        for (size_t i = 0; i < current_route.size(); ++i) {
            int e_idx = current_route[i];
            const auto& emp = employees[e_idx];
            
            double ride_time = drop_time - pickup_times[i];
            current_total_emp_time += ride_time;
            
            // Late Dropoff Penalty
            if (emp.latest_drop > 0) {
                if (drop_time > emp.latest_drop) {
                    // Quadratic penalty for lateness to discourage it strongly
                    double delay = drop_time - emp.latest_drop;
                    current_penalty += 500.0 + (delay * delay * 10.0);
                }
            }
        }

        // Weighted Score for internal optimization
        return (current_penalty * 1000.0) + (current_dist * v.cost_per_km * config.cost_weight) + (current_total_emp_time * config.time_weight);
    };

    // Evaluate initial sorted state
    double best_eval = evaluate_perm(route);
    out_dist = 0; // Recalculated below
    
    // Simple Local Search: Try swapping adjacent nodes to see if it improves
    if (try_swaps) {
        for(size_t i=0; i<route.size()-1; ++i) {
            swap(route[i], route[i+1]);
            double score = evaluate_perm(route);
            if (score < best_eval) {
                best_eval = score;
                best_perm = route;
            } else {
                swap(route[i], route[i+1]); // swap back
            }
        }
    }
    
    // Final run to populate out vars
    route = best_perm;
    
    // Re-run evaluation logic linearly to set output variables
    // (Copy-paste logic from lambda for clean output generation)
    double cur_lat = v.current_lat;
    double cur_lng = v.current_lng;
    double cur_time = v.available_from;
    vector<double> p_times;
    
    out_dist = 0;
    out_penalty = 0;
    out_time = 0;

    for (int e_idx : route) {
        const auto& emp = employees[e_idx];
        double d = haversine_km(cur_lat, cur_lng, emp.pickup_lat, emp.pickup_lng);
        out_dist += d;
        double travel = (d / v.speed_kmph) * 60.0;
        cur_time += travel;
        if (emp.earliest_pickup > 0 && cur_time < emp.earliest_pickup) cur_time = emp.earliest_pickup;
        p_times.push_back(cur_time);
        cur_lat = emp.pickup_lat;
        cur_lng = emp.pickup_lng;
    }
    double d_off = haversine_km(cur_lat, cur_lng, config.office_lat, config.office_lng);
    out_dist += d_off;
    cur_time += (d_off / v.speed_kmph) * 60.0;
    
    for(size_t i=0; i<route.size(); ++i) {
        out_time += (cur_time - p_times[i]);
        if(employees[route[i]].latest_drop > 0 && cur_time > employees[route[i]].latest_drop) {
             double delay = cur_time - employees[route[i]].latest_drop;
             out_penalty += 500.0 + (delay * delay * 10.0);
        }
    }
}

// Global evaluation of a full solution
typedef vector<vector<int>> Solution;

double global_evaluate(Solution& sol, double& out_cost, double& out_time, double& out_penalty) {
    double total_cost = 0;
    double total_emp_time = 0;
    double penalty = 0;
    
    for (size_t v_idx = 0; v_idx < sol.size(); ++v_idx) {
        auto& route = sol[v_idx];
        if (route.empty()) continue;

        const auto& v = vehicles[v_idx];
        
        // Capacity penalty (Heavy)
        if (route.size() > (size_t)v.capacity) {
            penalty += 50000.0 * (route.size() - v.capacity);
        }

        // Sharing preference penalty
        for (int e_idx : route) {
            int pref = employees[e_idx].sharing_pref;
            if (pref == 1 && route.size() > 1) penalty += 5000;
            if (pref == 2 && route.size() > 2) penalty += 2000;
        }

        double d, t, p;
        optimize_route_sequence(v_idx, route, d, t, p);
        
        total_cost += d * v.cost_per_km;
        total_emp_time += t;
        penalty += p;
    }

    out_cost = total_cost;
    out_time = total_emp_time;
    out_penalty = penalty;

    // Objective Function: Minimize Penalty first, then Cost/Time
    return penalty + (total_cost * config.cost_weight) + (total_emp_time * config.time_weight);
}

// --- ALNS Operators ---

void destroy_random(Solution& sol, vector<int>& unassigned, int num_to_remove) {
    // 1. Identify victims
    vector<pair<int, int>> assigned_indices; 
    for (size_t v=0; v<sol.size(); ++v) {
        for (size_t i=0; i<sol[v].size(); ++i) {
            assigned_indices.push_back({(int)v, (int)i});
        }
    }

    if(assigned_indices.empty()) return;
    
    shuffle(assigned_indices.begin(), assigned_indices.end(), rng);
    
    set<int> emps_to_remove;
    int actual_removed = 0;
    for(size_t i=0; i<assigned_indices.size() && actual_removed < num_to_remove; ++i) {
        int v = assigned_indices[i].first;
        int pos = assigned_indices[i].second;
        emps_to_remove.insert(sol[v][pos]);
        unassigned.push_back(sol[v][pos]);
        actual_removed++;
    }

    // 2. Rebuild clean routes
    for(size_t v=0; v<sol.size(); ++v) {
        vector<int> new_route;
        for(int e : sol[v]) {
            if(emps_to_remove.find(e) == emps_to_remove.end()) {
                new_route.push_back(e);
            }
        }
        sol[v] = new_route;
    }
}

void repair_greedy(Solution& sol, vector<int>& unassigned) {
    shuffle(unassigned.begin(), unassigned.end(), rng);

    for (int emp_idx : unassigned) {
        double best_cost_increase = 1e18;
        int best_v = -1;
        
        for (size_t v=0; v<sol.size(); ++v) {
             const auto& veh = vehicles[v];
             
             // HARD CONSTRAINT CHECK: Capacity
             if (sol[v].size() >= veh.capacity) continue; 
             
             // Calculate cost increase
             double d_before, t_before, p_before;
             optimize_route_sequence(v, sol[v], d_before, t_before, p_before);
             double score_before = (p_before * 1000) + (d_before * veh.cost_per_km);

             sol[v].push_back(emp_idx);
             double d_after, t_after, p_after;
             optimize_route_sequence(v, sol[v], d_after, t_after, p_after);
             double score_after = (p_after * 1000) + (d_after * veh.cost_per_km);
             
             sol[v].pop_back();

             double increase = score_after - score_before;
             if (increase < best_cost_increase) {
                 best_cost_increase = increase;
                 best_v = v;
             }
        }

        if (best_v != -1) {
            sol[best_v].push_back(emp_idx);
        } else {
            // Fallback: Find any vehicle with space, ignoring cost
            bool placed = false;
            for(size_t v=0; v<sol.size(); ++v) {
                if(sol[v].size() < vehicles[v].capacity) {
                    sol[v].push_back(emp_idx);
                    placed = true;
                    break;
                }
            }
            if(!placed && !vehicles.empty()) sol[0].push_back(emp_idx); // Overload logic
        }
    }
    unassigned.clear();
}

void print_status(int iter, double score, const Solution& sol, double c, double t, double p) {
    cout << "{\"generation\": " << iter << ", \"score\": " << score 
         << ", \"stats\": {\"cost\": " << c << ", \"time\": " << t << ", \"penalty\": " << p << "}"
         << ", \"assignment\": {";
    
    bool firstV = true;
    for (size_t v=0; v<vehicles.size(); ++v) {
        if(sol[v].empty()) continue; // Don't print empty vehicles
        if (!firstV) cout << ", ";
        firstV = false;
        cout << "\"" << vehicles[v].id << "\": [";
        for (size_t i=0; i<sol[v].size(); ++i) {
            if (i > 0) cout << ",";
            cout << "\"" << employees[sol[v][i]].id << "\"";
        }
        cout << "]";
    }
    cout << "}}" << endl;
}

int main(int argc, char** argv) {
    // 1. Setup
    rng.seed(time(0));
    
    // Simulate reading input (Hardcoded for demo, replace with your `fin` logic)
    // IMPORTANT: Make sure config weights are reasonable
    config.cost_weight = 0.6;
    config.time_weight = 0.4;
    config.generations = 2000; // ALNS needs more iterations than GA
    config.office_lat = 12.9716; config.office_lng = 77.5946;

    // MOCK DATA (Remove this and use your file reading)
    // ... (Your file reading code goes here) ...

    // 2. INITIALIZATION (The "Greedy Seed")
    // Instead of random, we fill vehicles greedily to avoid massive initial penalties
    Solution current_sol(vehicles.size());
    vector<int> all_emps(employees.size());
    for(int i=0; i<employees.size(); ++i) all_emps[i] = i;
    
    // Run the repair operator on an empty solution to build the initial state
    vector<int> initial_unassigned = all_emps;
    repair_greedy(current_sol, initial_unassigned);
    
    double current_c, current_t, current_p;
    double current_score = global_evaluate(current_sol, current_c, current_t, current_p);
    
    Solution best_sol = current_sol;
    double best_score = current_score;

    // 3. ALNS MAIN LOOP
    double temperature = 1000.0; // Start high
    double cooling_rate = 0.995;

    for (int iter = 0; iter < config.generations; ++iter) {
        Solution neighbor = current_sol;
        vector<int> unassigned;
        
        // DESTROY (Remove 15-25%)
        int num_remove = max(1, (int)(employees.size() * 0.20));
        destroy_random(neighbor, unassigned, num_remove);
        
        // REPAIR
        repair_greedy(neighbor, unassigned);

        // EVALUATE
        double n_c, n_t, n_p;
        double n_score = global_evaluate(neighbor, n_c, n_t, n_p);

        // ACCEPTANCE (Simulated Annealing)
        double delta = n_score - current_score;
        bool accept = false;
        
        if (delta < 0) {
            accept = true;
        } else {
            double prob = exp(-delta / temperature);
            if ((rng()/(double)rng.max()) < prob) accept = true;
        }

        if (accept) {
            current_sol = neighbor;
            current_score = n_score;
            if (n_score < best_score) {
                best_sol = neighbor;
                best_score = n_score;
            }
        }

        temperature *= cooling_rate;

        // REPORTING
        if (iter % 100 == 0) {
             double b_c, b_t, b_p;
             global_evaluate(best_sol, b_c, b_t, b_p);
             print_status(iter, best_score, best_sol, b_c, b_t, b_p);
        }
    }

    // Final Output
    double b_c, b_t, b_p;
    global_evaluate(best_sol, b_c, b_t, b_p);
    print_status(config.generations, best_score, best_sol, b_c, b_t, b_p);

    return 0;
}