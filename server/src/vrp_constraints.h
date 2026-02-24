#ifndef VRP_CONSTRAINTS_H
#define VRP_CONSTRAINTS_H

#include "vrp_types.h"
#include <vector>
#include <iostream>

class ConstraintEngine {
public:
    std::vector<IntVar> employee_vars;
    std::vector<std::vector<int>> incompatible_pairs;
    
    void setup(bool enforce_soft, const std::vector<Employee>& emps,
               const std::vector<Vehicle>& virt_vehs, const Metadata& /*meta*/) {
        
        int n_emp = emps.size();
        int n_veh = virt_vehs.size();
        
        employee_vars.clear();
        employee_vars.reserve(n_emp);
        for (int i = 0; i < n_emp; i++) {
            employee_vars.push_back(IntVar(n_veh));
            employee_vars[i].employee_id = i;
        }
        
        incompatible_pairs.clear();
        incompatible_pairs.resize(n_emp);
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "CONSTRAINT PROPAGATION\n";
        std::cout << std::string(60, '=') << std::endl;
        
        int pruned = 0;
        
        // Vehicle preference constraints - REMOVED
        // Vehicle preferences are SOFT constraints, not hard domain restrictions.
        // Removing vehicles from domain prevents ALNS from exploring better solutions
        // where breaking a preference reduces lateness for high-priority employees.
        // Instead, preferences are penalized in the cost function via pref_violation_penalty.
        std::cout << "\nVehicle preferences treated as SOFT constraints (not domain pruning)" << std::endl;
        
        // Time incompatibility
        std::cout << "\nComputing TIME INCOMPATIBILITIES..." << std::endl;
        int incomp_count = 0;
        for (int i = 0; i < n_emp; i++) {
            for (int j = i+1; j < n_emp; j++) {
                // Basic check: deadline before earliest pickup
                if (emps[i].latest_arrival_deadline < emps[j].earliest_pickup ||
                    emps[j].latest_arrival_deadline < emps[i].earliest_pickup) {
                    incompatible_pairs[i].push_back(j);
                    incompatible_pairs[j].push_back(i);
                    incomp_count++;
                    continue;
                }
                
                // Advanced check: if waiting for one employee would cause the other to miss deadline
                // Assume ~30 min minimum for pickup + travel to office
                const int min_trip_time = 30;
                
                // If j's earliest pickup + min_trip_time exceeds i's deadline, they're incompatible
                if (emps[j].earliest_pickup + min_trip_time > emps[i].latest_arrival_deadline) {
                    incompatible_pairs[i].push_back(j);
                    incompatible_pairs[j].push_back(i);
                    incomp_count++;
                    continue;
                }
                // Vice versa
                if (emps[i].earliest_pickup + min_trip_time > emps[j].latest_arrival_deadline) {
                    incompatible_pairs[i].push_back(j);
                    incompatible_pairs[j].push_back(i);
                    incomp_count++;
                    continue;
                }
            }
        }
        std::cout << "   Found " << incomp_count << " incompatible pairs" << std::endl;
        
        // Sharing preferences (Stage 1 only)
        if (enforce_soft) {
            std::cout << "\nEnforcing SHARING PREFERENCES..." << std::endl;
            for (int i = 0; i < n_emp; i++) {
                if (emps[i].sharing_pref == 1) {  // Single preference
                    for (int j = 0; j < n_emp; j++) {
                        if (i != j) {
                            bool exists = false;
                            for (int k : incompatible_pairs[i]) if (k == j) { exists = true; break; }
                            if (!exists) incompatible_pairs[i].push_back(j);
                        }
                    }
                }
            }
        }
        
        std::cout << "\nSummary: " << pruned << " pruned, " 
                  << incomp_count << " incompatible\n";
        std::cout << std::string(60, '=') << std::endl;
    }
    
    bool are_compatible(int i, int j) const {
        for (int k : incompatible_pairs[i]) if (k == j) return false;
        return true;
    }
};

#endif
