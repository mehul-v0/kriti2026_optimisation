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
#include <ctime>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace std;

// --- Data Structures ---
struct Employee {
    string id;
    int int_id;
    double pickup_lat, pickup_lng;
    int earliest_pickup, latest_drop;     
    int sharing_pref;    
    double baseline_cost, baseline_time;
    int priority;        // 1-5, lower is higher priority
    int vehicle_pref;    // 0=any, 1=premium, 2=normal
};

struct Vehicle {
    string id;
    int int_id;
    int capacity;
    double speed_kmph;
    double cost_per_km;
    double current_lat, current_lng;
    int available_from;
    int category;        // 1=premium, 2=normal
};

struct Config {
    double cost_weight;
    double time_weight;
    int pop_size;
    int iterations; 
    double office_lat, office_lng;
    int slack = 30;
    int priority_max_delay[5] = {5, 8, 12, 18, 30};
    
    // SA specific parameters
    double initial_temp = 10000.0;
    double final_temp = 1.0;
    double cooling_rate = 0.9995;
};

Config config;
vector<Employee> employees;
vector<Vehicle> vehicles;
double baseline_total_cost = 0;
double baseline_total_time = 0;
mt19937 rng;

// Solution representation
typedef vector<vector<int>> Solution;

// --- Helper Functions ---
double haversine_km(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat/2) * sin(dlat/2) + cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) * sin(dlon/2) * sin(dlon/2);
    return R * 2 * atan2(sqrt(a), sqrt(1 - a));
}

// --- Route Evaluation ---
void evaluate_route(int v_idx, const vector<int>& route, double& out_dist, double& out_time, double& out_penalty) {
    if (route.empty()) {
        out_dist = 0; out_time = 0; out_penalty = 0;
        return;
    }

    const auto& v = vehicles[v_idx];
    
    double cur_lat = v.current_lat;
    double cur_lng = v.current_lng;
    double cur_time = v.available_from;
    
    out_dist = 0;
    out_penalty = 0;
    out_time = 0;
    
    vector<double> pickup_times;
    pickup_times.reserve(route.size());

    // Visit Pickups
    for (int e_idx : route) {
        const auto& emp = employees[e_idx];
        double d = haversine_km(cur_lat, cur_lng, emp.pickup_lat, emp.pickup_lng);
        out_dist += d;
        
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
    
    // Go to Office (Dropoff All)
    double d_office = haversine_km(cur_lat, cur_lng, config.office_lat, config.office_lng);
    out_dist += d_office;
    cur_time += (d_office / v.speed_kmph) * 60.0;
    double drop_time = cur_time;

    // Calculate Penalties & Employee Time
    for (size_t i = 0; i < route.size(); ++i) {
        int e_idx = route[i];
        const auto& emp = employees[e_idx];
        
        double ride_time = drop_time - pickup_times[i];
        out_time += ride_time;
        
        // Priority-based delay penalty
        if (emp.latest_drop > 0) {
            int priority_idx = max(0, min(4, emp.priority - 1));
            int max_delay = config.priority_max_delay[priority_idx];
            double priority_mult = 6.0 - emp.priority;
            
            if (drop_time > emp.latest_drop + max_delay) {
                double delay = drop_time - emp.latest_drop - max_delay;
                out_penalty += (500.0 + delay * delay * 10.0) * priority_mult;
            } else if (drop_time > emp.latest_drop) {
                double delay = drop_time - emp.latest_drop;
                out_penalty += delay * priority_mult;
            }
        }
    }
}

// Helper to get all assigned employees (unique)
set<int> get_assigned_employees(const Solution& sol) {
    set<int> assigned;
    for (const auto& route : sol) {
        for (int e : route) {
            assigned.insert(e);
        }
    }
    return assigned;
}

// Validate and repair solution
void validate_solution(Solution& sol) {
    set<int> seen;
    
    // Remove duplicates within solution
    for (auto& route : sol) {
        vector<int> clean_route;
        for (int e : route) {
            if (seen.find(e) == seen.end()) {
                seen.insert(e);
                clean_route.push_back(e);
            }
        }
        route = clean_route;
    }
    
    // Find missing employees
    vector<int> missing;
    for (size_t i = 0; i < employees.size(); ++i) {
        if (seen.find(i) == seen.end()) {
            missing.push_back(i);
        }
    }
    
    // Assign missing employees to vehicles with least load
    for (int emp : missing) {
        int best_v = 0;
        size_t min_load = sol[0].size();
        for (size_t v = 1; v < sol.size(); ++v) {
            if (sol[v].size() < min_load) {
                min_load = sol[v].size();
                best_v = v;
            }
        }
        sol[best_v].push_back(emp);
    }
}

// Global evaluation
double global_evaluate(Solution& sol, double& out_cost, double& out_time, double& out_penalty) {
    double total_cost = 0;
    double total_emp_time = 0;
    double penalty = 0;
    
    // Validate solution first
    validate_solution(sol);
    
    // Penalty for missing employees
    set<int> assigned = get_assigned_employees(sol);
    int missing = (int)employees.size() - (int)assigned.size();
    if (missing > 0) {
        penalty += 100000.0 * missing;
    }
    
    for (size_t v_idx = 0; v_idx < sol.size(); ++v_idx) {
        auto& route = sol[v_idx];
        if (route.empty()) continue;

        const auto& v = vehicles[v_idx];
        
        // Capacity penalty
        if ((int)route.size() > v.capacity) {
            penalty += 50000.0 * (route.size() - v.capacity);
        }

        // Sharing preference penalty
        for (int e_idx : route) {
            int pref = employees[e_idx].sharing_pref;
            if (pref == 1 && route.size() > 1) penalty += 5000;
            if (pref == 2 && route.size() > 2) penalty += 2000;
            
            // Vehicle preference penalty
            int emp_vpref = employees[e_idx].vehicle_pref;
            int veh_cat = v.category;
            if (emp_vpref == 1 && veh_cat != 1) {
                // Premium employee in non-premium vehicle
                penalty += 800.0;
            } else if (emp_vpref == 2 && veh_cat != 2) {
                // Normal-preference employee in premium vehicle
                penalty += 100.0;
            }
        }

        double d, t, p;
        evaluate_route(v_idx, route, d, t, p);
        
        total_cost += d * v.cost_per_km;
        total_emp_time += t;
        penalty += p;
    }

    out_cost = total_cost;
    out_time = total_emp_time;
    out_penalty = penalty;

    return penalty + (total_cost * config.cost_weight) + (total_emp_time * config.time_weight);
}

// --- Simulated Annealing Neighborhood Operators ---

// Move: Transfer one employee from one vehicle to another
Solution move_employee(const Solution& sol) {
    Solution neighbor = sol;
    
    // Find vehicles with at least one employee
    vector<int> non_empty;
    for (size_t v = 0; v < neighbor.size(); ++v) {
        if (!neighbor[v].empty()) non_empty.push_back(v);
    }
    
    if (non_empty.empty()) return neighbor;
    
    // Pick random source vehicle
    int src_v = non_empty[rng() % non_empty.size()];
    if (neighbor[src_v].empty()) return neighbor;
    
    // Pick random employee from source
    int emp_idx = rng() % neighbor[src_v].size();
    int emp = neighbor[src_v][emp_idx];
    
    // Pick random destination vehicle (different from source)
    int dst_v = rng() % neighbor.size();
    if (neighbor.size() > 1) {
        while (dst_v == src_v) dst_v = rng() % neighbor.size();
    }
    
    // Move employee
    neighbor[src_v].erase(neighbor[src_v].begin() + emp_idx);
    neighbor[dst_v].push_back(emp);
    
    return neighbor;
}

// Swap: Exchange employees between two vehicles
Solution swap_employees(const Solution& sol) {
    Solution neighbor = sol;
    
    // Find vehicles with at least one employee
    vector<int> non_empty;
    for (size_t v = 0; v < neighbor.size(); ++v) {
        if (!neighbor[v].empty()) non_empty.push_back(v);
    }
    
    if (non_empty.size() < 2) return neighbor;
    
    // Pick two different vehicles
    int idx1 = rng() % non_empty.size();
    int idx2 = rng() % non_empty.size();
    while (idx2 == idx1) idx2 = rng() % non_empty.size();
    
    int v1 = non_empty[idx1];
    int v2 = non_empty[idx2];
    
    // Pick random employees
    int pos1 = rng() % neighbor[v1].size();
    int pos2 = rng() % neighbor[v2].size();
    
    // Swap
    swap(neighbor[v1][pos1], neighbor[v2][pos2]);
    
    return neighbor;
}

// Reorder: Change the sequence of employees within a vehicle (2-opt)
Solution reorder_route(const Solution& sol) {
    Solution neighbor = sol;
    
    // Find vehicles with at least 2 employees
    vector<int> candidates;
    for (size_t v = 0; v < neighbor.size(); ++v) {
        if (neighbor[v].size() >= 2) candidates.push_back(v);
    }
    
    if (candidates.empty()) return neighbor;
    
    int v = candidates[rng() % candidates.size()];
    
    // 2-opt: reverse a segment
    int i = rng() % neighbor[v].size();
    int j = rng() % neighbor[v].size();
    if (i > j) swap(i, j);
    
    reverse(neighbor[v].begin() + i, neighbor[v].begin() + j + 1);
    
    return neighbor;
}

// Shuffle: Randomly shuffle one vehicle's route
Solution shuffle_route(const Solution& sol) {
    Solution neighbor = sol;
    
    vector<int> non_empty;
    for (size_t v = 0; v < neighbor.size(); ++v) {
        if (neighbor[v].size() >= 2) non_empty.push_back(v);
    }
    
    if (non_empty.empty()) return neighbor;
    
    int v = non_empty[rng() % non_empty.size()];
    shuffle(neighbor[v].begin(), neighbor[v].end(), rng);
    
    return neighbor;
}

// Generate neighbor using random operator
Solution generate_neighbor(const Solution& sol) {
    int op = rng() % 4;
    
    switch (op) {
        case 0: return move_employee(sol);
        case 1: return swap_employees(sol);
        case 2: return reorder_route(sol);
        case 3: return shuffle_route(sol);
        default: return move_employee(sol);
    }
}

// --- Initial Solution (Greedy) ---
Solution create_initial_solution() {
    Solution sol(vehicles.size());
    
    // Sort employees by earliest pickup time
    vector<int> emp_order(employees.size());
    for (size_t i = 0; i < employees.size(); ++i) emp_order[i] = i;
    
    sort(emp_order.begin(), emp_order.end(), [](int a, int b) {
        return employees[a].earliest_pickup < employees[b].earliest_pickup;
    });
    
    // Greedy assignment: assign each employee to best vehicle
    for (int emp : emp_order) {
        double best_increase = 1e18;
        int best_v = 0;
        
        for (size_t v = 0; v < vehicles.size(); ++v) {
            // Prefer vehicles with capacity
            if ((int)sol[v].size() >= vehicles[v].capacity) {
                continue;  // Skip full vehicles in first pass
            }
            
            // Calculate insertion cost
            double d_before, t_before, p_before;
            evaluate_route(v, sol[v], d_before, t_before, p_before);
            
            sol[v].push_back(emp);
            double d_after, t_after, p_after;
            evaluate_route(v, sol[v], d_after, t_after, p_after);
            sol[v].pop_back();
            
            double increase = (d_after - d_before) * vehicles[v].cost_per_km + (p_after - p_before);
            
            if (increase < best_increase) {
                best_increase = increase;
                best_v = v;
            }
        }
        
        sol[best_v].push_back(emp);
    }
    
    return sol;
}

// --- Output ---
void print_status(int iter, double score, const Solution& sol, double c, double t, double p, double temp) {
    cout << "{\"generation\": " << iter << ", \"score\": " << fixed << setprecision(2) << score 
         << ", \"stats\": {\"cost\": " << c << ", \"time\": " << t << ", \"penalty\": " << p 
         << ", \"temperature\": " << temp << "}"
         << ", \"assignment\": {";
    
    bool firstV = true;
    for (size_t v = 0; v < vehicles.size(); ++v) {
        if (sol[v].empty()) continue;
        if (!firstV) cout << ", ";
        firstV = false;
        cout << "\"" << vehicles[v].id << "\": [";
        for (size_t i = 0; i < sol[v].size(); ++i) {
            if (i > 0) cout << ",";
            cout << "\"" << employees[sol[v][i]].id << "\"";
        }
        cout << "]";
    }
    cout << "}}" << endl;
}

// --- Main ---
int main(int argc, char** argv) {
    // Setup
    rng.seed(time(0));
    
    // Get input file
    string input_file = "cpp_input.txt";
    if (argc > 1) {
        input_file = argv[1];
    }
    
    // Read input
    ifstream fin(input_file);
    if (!fin.is_open()) {
        cerr << "Error: Could not open " << input_file << endl;
        return 1;
    }

    // Read config
    fin >> config.cost_weight >> config.time_weight >> config.pop_size >> config.iterations;
    fin >> config.office_lat >> config.office_lng;
    
    // Read priority max delay limits
    for (int i = 0; i < 5; ++i) {
        fin >> config.priority_max_delay[i];
    }

    // Read employees
    int num_employees;
    fin >> num_employees;
    employees.resize(num_employees);
    
    for (int i = 0; i < num_employees; ++i) {
        Employee& e = employees[i];
        fin >> e.id >> e.pickup_lat >> e.pickup_lng 
            >> e.earliest_pickup >> e.latest_drop 
            >> e.sharing_pref >> e.baseline_cost >> e.baseline_time
            >> e.priority >> e.vehicle_pref;
        e.int_id = i;
        baseline_total_cost += e.baseline_cost;
        baseline_total_time += e.baseline_time;
    }

    // Read vehicles
    int num_vehicles;
    fin >> num_vehicles;
    vehicles.resize(num_vehicles);
    
    for (int i = 0; i < num_vehicles; ++i) {
        Vehicle& v = vehicles[i];
        fin >> v.id >> v.capacity >> v.speed_kmph >> v.cost_per_km 
            >> v.current_lat >> v.current_lng >> v.available_from >> v.category;
        v.int_id = i;
    }
    
    fin.close();

    // Calculate cooling rate based on iterations
    // We want temp to go from initial_temp to final_temp in 'iterations' steps
    config.cooling_rate = pow(config.final_temp / config.initial_temp, 1.0 / config.iterations);

    // Create initial solution
    Solution current_sol = create_initial_solution();
    double current_c, current_t, current_p;
    double current_score = global_evaluate(current_sol, current_c, current_t, current_p);
    
    Solution best_sol = current_sol;
    double best_score = current_score;
    double best_c = current_c, best_t = current_t, best_p = current_p;

    double temperature = config.initial_temp;

    // Print initial state
    print_status(0, best_score, best_sol, best_c, best_t, best_p, temperature);

    // Simulated Annealing Main Loop
    int no_improve_count = 0;
    
    for (int iter = 1; iter <= config.iterations; ++iter) {
        // Generate neighbor
        Solution neighbor = generate_neighbor(current_sol);
        
        double n_c, n_t, n_p;
        double n_score = global_evaluate(neighbor, n_c, n_t, n_p);

        // Calculate acceptance probability
        double delta = n_score - current_score;
        bool accept = false;
        
        if (delta < 0) {
            // Better solution - always accept
            accept = true;
        } else {
            // Worse solution - accept with probability exp(-delta/T)
            double prob = exp(-delta / temperature);
            double rand_val = (double)(rng() % 10000) / 10000.0;
            accept = (rand_val < prob);
        }

        if (accept) {
            current_sol = neighbor;
            current_score = n_score;
            
            // Update best if improved
            if (n_score < best_score) {
                best_sol = neighbor;
                best_score = n_score;
                best_c = n_c;
                best_t = n_t;
                best_p = n_p;
                no_improve_count = 0;
            } else {
                no_improve_count++;
            }
        } else {
            no_improve_count++;
        }

        // Cool down
        temperature *= config.cooling_rate;

        // Reheat if stuck
        if (no_improve_count > config.iterations / 10) {
            temperature = config.initial_temp * 0.5;
            no_improve_count = 0;
        }

        // Report progress
        if (iter % 100 == 0 || iter == config.iterations) {
            print_status(iter, best_score, best_sol, best_c, best_t, best_p, temperature);
        }
    }

    // Final output
    double final_c, final_t, final_p;
    global_evaluate(best_sol, final_c, final_t, final_p);
    print_status(config.iterations, best_score, best_sol, final_c, final_t, final_p, temperature);

    return 0;
}
