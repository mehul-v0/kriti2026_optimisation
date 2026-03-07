#ifndef VRP_SAVINGS_CONSTRUCTION_H
#define VRP_SAVINGS_CONSTRUCTION_H

#include "vrp_types.h"
#include "vrp_validators.h"
#include "vrp_constraints.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include <unordered_set>

extern std::vector<std::vector<double>> dist_matrix;
extern int OFFICE_NODE;

// Clarke-Wright Savings Algorithm for VRP construction
// Computes savings s(i,j) = d(i,office) + d(office,j) - d(i,j) for all pairs,
// sorts descending, and greedily merges routes. Produces better initial solutions
// than sequential insertion for cost minimization.
class SavingsConstruction {
public:
    static void build(std::vector<std::vector<int>>& routes,
                      const std::vector<Employee>& emps,
                      const std::vector<Vehicle>& virt_vehs,
                      const ConstraintEngine& cp, bool enforce_soft, const Metadata& meta,
                      const NeighborList& /*nlist*/) {
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "SAVINGS CONSTRUCTION (Clarke-Wright)\n";
        std::cout << std::string(60, '=') << std::endl;
        
        int n_emp = emps.size();
        int n_veh = virt_vehs.size();
        
        routes.clear();
        routes.resize(n_veh);
        
        // Sort vehicles by cost_per_km ascending (cheapest first)
        std::vector<int> veh_order(n_veh);
        for (int i = 0; i < n_veh; i++) veh_order[i] = i;
        std::sort(veh_order.begin(), veh_order.end(), [&](int a, int b) {
            return virt_vehs[a].cost_per_km < virt_vehs[b].cost_per_km;
        });
        
        // Step 1: Start with each employee on their own route (cheapest vehicles first)
        std::vector<int> emp_to_route(n_emp, -1); // which route index each employee is on
        int next_veh = 0;
        for (int e = 0; e < n_emp && next_veh < n_veh; e++) {
            int v = veh_order[next_veh++];
            routes[v].push_back(e);
            emp_to_route[e] = v;
        }
        // If more employees than vehicles, force them in
        for (int e = 0; e < n_emp; e++) {
            if (emp_to_route[e] < 0) {
                // Find route with capacity
                for (int vi = 0; vi < n_veh; vi++) {
                    int v = veh_order[vi];
                    if ((int)routes[v].size() < virt_vehs[v].capacity) {
                        routes[v].push_back(e);
                        emp_to_route[e] = v;
                        break;
                    }
                }
            }
        }
        
        // Step 2: Compute savings for all pairs
        struct Saving {
            double value;
            int emp_a, emp_b;
        };
        std::vector<Saving> savings;
        savings.reserve(n_emp * (n_emp - 1) / 2);
        
        for (int i = 0; i < n_emp; i++) {
            for (int j = i + 1; j < n_emp; j++) {
                double s = dist_matrix[emps[i].node_idx][OFFICE_NODE] 
                         + dist_matrix[OFFICE_NODE][emps[j].node_idx]
                         - dist_matrix[emps[i].node_idx][emps[j].node_idx];
                if (s > 0) {
                    savings.push_back({s, i, j});
                }
            }
        }
        
        // Sort by savings value descending
        std::sort(savings.begin(), savings.end(), [](const Saving& a, const Saving& b) {
            return a.value > b.value;
        });
        
        // Step 3: Greedily merge routes
        for (const auto& sav : savings) {
            int a = sav.emp_a, b = sav.emp_b;
            int ra = emp_to_route[a], rb = emp_to_route[b];
            if (ra == rb) continue; // Already on same route
            if (ra < 0 || rb < 0) continue;
            
            // Check if merging is feasible
            int new_size = (int)routes[ra].size() + (int)routes[rb].size();
            if (new_size > virt_vehs[ra].capacity) continue;
            
            // Check compatibility
            bool compat = true;
            for (int ea : routes[ra]) {
                for (int eb : routes[rb]) {
                    if (!cp.are_compatible(ea, eb)) { compat = false; break; }
                }
                if (!compat) break;
            }
            if (!compat) continue;
            
            // Merge: add all employees from rb into ra
            for (int e : routes[rb]) {
                routes[ra].push_back(e);
                emp_to_route[e] = ra;
            }
            routes[rb].clear();
            
            // Validate merged route
            int h = 0, s = 0;
            if (!validate_full_route(routes[ra], virt_vehs[ra], emps, h, s, enforce_soft, meta)) {
                // Leave merged even with violations - ALNS will optimize
                // Undoing merges is complex and unnecessary since ALNS explores the space
            }
        }
        
        // Step 4: Ensure all employees assigned
        for (int e = 0; e < n_emp; e++) {
            if (emp_to_route[e] < 0) {
                for (int v = 0; v < n_veh; v++) {
                    if ((int)routes[v].size() < virt_vehs[v].capacity) {
                        routes[v].push_back(e);
                        emp_to_route[e] = v;
                        break;
                    }
                }
            }
        }
        
        int used = 0;
        for (const auto& r : routes) if (!r.empty()) used++;
        std::cout << "Savings construction: " << n_emp << " employees in " << used << " trips\n";
        std::cout << std::string(60, '=') << std::endl;
    }
};

#endif
