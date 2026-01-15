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

using namespace std;

// data structures
struct Employee {
    string id;
    int int_id;
    double pickup_lat;
    double pickup_lng;
    int earliest_pickup; // minutes
    int latest_drop;     // minutes
    int sharing_pref;    // 0=any, 1=single, 2=double, 3=triple
    double baseline_cost;
    double baseline_time;
};

struct Vehicle {
    string id;
    int int_id;
    int capacity;
    double speed_kmph;
    double cost_per_km;
    double current_lat;
    double current_lng;
    int available_from; // minutes
};

struct Config {
    double cost_weight;
    double time_weight;
    int pop_size;
    int generations;
    double office_lat;
    double office_lng;
    // max delay constraint slacks
    int slack = 30; 
};

Config config;
vector<Employee> employees;
vector<Vehicle> vehicles;
double baseline_total_cost = 0;
double baseline_total_time = 0;

// Random engine
mt19937 rng;

// Global map for quick lookup if needed? indices are enough.
// We assign employees to vehicles.
// Solution representation: vector of vector of employee indices for each vehicle.
typedef vector<vector<int>> Solution;

double haversine_km(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0;
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double dphi = (lat2 - lat1) * M_PI / 180.0;
    double dlambda = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dphi/2.0) * sin(dphi/2.0) +
               cos(phi1) * cos(phi2) *
               sin(dlambda/2.0) * sin(dlambda/2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return R * c;
}

double evaluate(const Solution& sol, double& out_cost, double& out_time, double& out_penalty) {
    double total_cost = 0;
    double total_emp_time = 0;
    double penalty = 0;

    for (size_t v_idx = 0; v_idx < sol.size(); ++v_idx) {
        const auto& route = sol[v_idx]; // list of emp indices
        if (route.empty()) continue;

        const auto& v = vehicles[v_idx];
        
        // Capacity penalty
        if (route.size() > (size_t)v.capacity) {
            penalty += 1000.0 * (route.size() - v.capacity);
        }

        // Sharing preference penalty
        for (int e_idx : route) {
            int pref = employees[e_idx].sharing_pref;
            if (pref == 1 && route.size() > 1) penalty += 500;
            if (pref == 2 && route.size() > 2) penalty += 200;
            // triple (3) usually means max 3, or at least 3? 
            // In python code: triple check wasn't explicitly penalizing >3, just ensuring 'capacity' usually covers it. 
            // We'll stick to python logic: single/double checks only.
        }

        // Simulate route
        double cur_lat = v.current_lat;
        double cur_lng = v.current_lng;
        double cur_time = v.available_from;
        double total_dist = 0;

        // Sequence of pickups
        vector<double> pickup_times;
        for (int e_idx : route) {
            const auto& emp = employees[e_idx];
            double d = haversine_km(cur_lat, cur_lng, emp.pickup_lat, emp.pickup_lng);
            total_dist += d;
            double travel = (d / v.speed_kmph) * 60.0;
            cur_time += travel;
            
            // Wait if early
            if (emp.earliest_pickup > 0 && cur_time < emp.earliest_pickup) {
                cur_time = emp.earliest_pickup;
            }
            
            pickup_times.push_back(cur_time); // actual pickup time
            cur_lat = emp.pickup_lat;
            cur_lng = emp.pickup_lng;
        }

        // Go to office
        double d_office = haversine_km(cur_lat, cur_lng, config.office_lat, config.office_lng);
        total_dist += d_office;
        double travel_office = (d_office / v.speed_kmph) * 60.0;
        cur_time += travel_office; // drop time at office

        double drop_time = cur_time;

        // Calculate Cost
        total_cost += total_dist * v.cost_per_km;

        // Calculate Employee Times and Delay Penalties
        for (size_t i = 0; i < route.size(); ++i) {
            int e_idx = route[i];
            const auto& emp = employees[e_idx];
            
            // Re-calc ride time approx?
            // Python code logic: 
            // "employee ride times (pickup->drop): approximate by drop_time - pickup_arrival"
            // Wait, Python re-simulated to get specific ride time. 
            // Here we have `drop_time` (everyone drops at office at same time in this simplified model?)
            // Yes, standard pooling to office usually implies one drop location.
            // Ride time = Drop Time - Pickup Time
            
            double ride_time = drop_time - pickup_times[i];
            total_emp_time += ride_time;

            // Latest drop penalty
            if (emp.latest_drop > 0) {
                if (drop_time > emp.latest_drop + config.slack) { // simplified slack
                     penalty += 1000.0; // + (drop_time - emp.latest_drop);
                }
            }
        }
    }

    out_cost = total_cost;
    out_time = total_emp_time;
    out_penalty = penalty;

    // Objective
    double norm_cost = total_cost / (baseline_total_cost + 1e-6);
    double norm_time = total_emp_time / (baseline_total_time + 1e-6);
    
    return config.cost_weight * norm_cost + config.time_weight * norm_time + penalty / 10000.0;
}

Solution create_random_solution() {
    Solution sol(vehicles.size());
    // Random assign
    for (size_t i = 0; i < employees.size(); ++i) {
        int v_idx = rng() % vehicles.size();
        sol[v_idx].push_back(i);
    }
    // Shuffle routes
    for (auto& route : sol) {
        shuffle(route.begin(), route.end(), rng);
    }
    return sol;
}

Solution mutate(const Solution& p) {
    Solution child = p;
    if (employees.empty() || vehicles.empty()) return child;

    double r = (double)rng() / rng.max();
    if (r < 0.7) {
        // move employee
        // Pick random vehicle source
        int v_from = rng() % vehicles.size();
        if (child[v_from].empty()) {
            // try to find one with emps
            for(int k=0; k<10; ++k) {
                v_from = rng() % vehicles.size();
                if(!child[v_from].empty()) break;
            }
        }
        
        if (!child[v_from].empty()) {
             int idx_in_route = rng() % child[v_from].size();
             int e_idx = child[v_from][idx_in_route];
             
             child[v_from].erase(child[v_from].begin() + idx_in_route);
             
             int v_to = rng() % vehicles.size();
             child[v_to].push_back(e_idx);
        }
    } else {
        // shuffle a route
        int v = rng() % vehicles.size();
        if (!child[v].empty()) {
            shuffle(child[v].begin(), child[v].end(), rng);
        }
    }
    return child;
}

Solution crossover(const Solution& A, const Solution& B) {
    // Single point crossover of vehicles?
    // Child takes vehicles 0..k from A, k+1..N from B
    // Then repair duplicates/missing
    Solution child(vehicles.size());
    int cut = rng() % max(1, (int)vehicles.size());
    
    for (int i = 0; i < cut; ++i) child[i] = A[i];
    for (size_t i = cut; i < vehicles.size(); ++i) child[i] = B[i];

    // Repair
    vector<int> counts(employees.size(), 0);
    for (const auto& route : child) {
         for (int e : route) counts[e]++;
    }

    vector<int> missing;
    for (size_t i = 0; i < employees.size(); ++i) {
        if (counts[i] == 0) missing.push_back(i);
    }

    for (size_t v = 0; v < vehicles.size(); ++v) {
        for (size_t i = 0; i < child[v].size(); ) {
            int e = child[v][i];
            if (counts[e] > 1) {
                // remove
                child[v].erase(child[v].begin() + i);
                counts[e]--;
            } else {
                i++;
            }
        }
    }
    
    // Add missing
    for (int e : missing) {
        int v = rng() % vehicles.size();
        child[v].push_back(e);
    }

    return child;
}

void print_status(int gen, double score, const Solution& sol, double c, double t, double p) {
    // Print JSON-like string
    // {"generation": 1, "score": 0.5, "stats": {"cost": 100, "time": 200, "penalty": 0}, "assignment": {"V01": ["E01", ...], ...}}
    cout << "{\"generation\": " << gen << ", \"score\": " << score 
         << ", \"stats\": {\"cost\": " << c << ", \"time\": " << t << ", \"penalty\": " << p << "}"
         << ", \"assignment\": {";
    
    bool firstV = true;
    for (size_t v=0; v<vehicles.size(); ++v) {
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
    if (argc < 2) {
        cerr << "Usage: solver <input_file>" << endl;
        return 1;
    }

    rng.seed(time(0));

    // READ INPUT
    // Format assumed:
    // cost_weight time_weight pop_size generations
    // office_lat office_lng
    // num_employees
    // id pickup_lat pickup_lng earliest latest sharing base_cost base_time
    // ...
    // num_vehicles
    // id cap speed cost lat lng avail
    // ...

    ifstream fin(argv[1]);
    if (!fin.is_open()) {
        cerr << "Cannot open input file" << endl;
        return 1;
    }

    fin >> config.cost_weight >> config.time_weight >> config.pop_size >> config.generations;
    fin >> config.office_lat >> config.office_lng;

    int nE;
    fin >> nE;
    for (int i=0; i<nE; ++i) {
        Employee e;
        e.int_id = i;
        fin >> e.id >> e.pickup_lat >> e.pickup_lng >> e.earliest_pickup >> e.latest_drop 
            >> e.sharing_pref >> e.baseline_cost >> e.baseline_time;
        employees.push_back(e);
        baseline_total_cost += e.baseline_cost;
        baseline_total_time += e.baseline_time;
    }

    int nV;
    fin >> nV;
    for (int i=0; i<nV; ++i) {
        Vehicle v;
        v.int_id = i;
        fin >> v.id >> v.capacity >> v.speed_kmph >> v.cost_per_km 
            >> v.current_lat >> v.current_lng >> v.available_from;
        vehicles.push_back(v);
    }

    // GA Loop
    vector<pair<double, Solution>> population;
    for(int i=0; i<config.pop_size; ++i) {
         Solution s = create_random_solution();
         double c, t, p;
         double score = evaluate(s, c, t, p);
         population.push_back({score, s});
    }

    for (int gen = 0; gen <= config.generations; ++gen) {
        // Sort
        sort(population.begin(), population.end(), 
             [](const pair<double, Solution>& a, const pair<double, Solution>& b){
                 return a.first < b.first;
             });
        
        // Report
        if (gen % 5 == 0 || gen == config.generations) {
            double cc, tt, pp;
            evaluate(population[0].second, cc, tt, pp);
            print_status(gen, population[0].first, population[0].second, cc, tt, pp);
        }
        
        if (gen == config.generations) break;

        // Elitism: keep top 20%
        int elite_count = max(2, (int)(0.2 * config.pop_size));
        vector<pair<double, Solution>> next_pop;
        for(int i=0; i<elite_count; ++i) next_pop.push_back(population[i]);

        // Breed
        while(next_pop.size() < (size_t)config.pop_size) {
            // Tournament or random selection? Random top 50% is easy
            int idx1 = rng() % (config.pop_size / 2);
            int idx2 = rng() % (config.pop_size / 2);
            Solution child = crossover(population[idx1].second, population[idx2].second);
            if (((double)rng() / rng.max()) < 0.3) {
                 child = mutate(child);
            }
            double c, t, p;
            double score = evaluate(child, c, t, p);
            next_pop.push_back({score, child});
        }
        population = next_pop;
    }

    return 0;
}
