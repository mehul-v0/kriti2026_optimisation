#ifndef VRP_PARSER_H
#define VRP_PARSER_H

#include "vrp_types.h"
#include "vrp_utils.h"
#include "json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

class VRPParser {
public:
    static bool load(const std::string& file, std::vector<Employee>& emps,
                     std::vector<Vehicle>& phys_vehs, std::vector<Vehicle>& virt_vehs,
                     Metadata& meta, std::vector<std::vector<double>>& dist, int& office) {
        
        std::ifstream f(file);
        if (!f.is_open()) { std::cerr << "Error: Cannot open " << file << std::endl; return false; }
        
        json data;
        try { f >> data; }
        catch (const std::exception& e) { std::cerr << "Error: " << e.what() << std::endl; return false; }
        
        // Metadata
        if (data.contains("metadata")) {
            auto& m = data["metadata"];
            meta.priority_max_delays[1] = m.value("priority_1_max_delay_min", 5);
            meta.priority_max_delays[2] = m.value("priority_2_max_delay_min", 10);
            meta.priority_max_delays[3] = m.value("priority_3_max_delay_min", 15);
            meta.priority_max_delays[4] = m.value("priority_4_max_delay_min", 20);
            meta.priority_max_delays[5] = m.value("priority_5_max_delay_min", 30);
            meta.cost_weight = m.value("objective_cost_weight", 0.7);
            meta.time_weight = m.value("objective_time_weight", 0.3);
        }
        
        // Office location
        if (!data.contains("employees") || data["employees"].empty()) {
            std::cerr << "Error: No employees found in input" << std::endl;
            return false;
        }
        double office_lat = data["employees"][0].value("drop_lat", 0.0);
        double office_lng = data["employees"][0].value("drop_lng", 0.0);
        
        // Locations: [Office, Emp1, ..., EmpN, Veh1, ..., VehM]
        std::vector<std::pair<double,double>> locs;
        locs.push_back({office_lat, office_lng});
        office = 0;
        
        // Employees
        int node_idx = 1;
        for (auto& e : data["employees"]) {
            Employee emp;
            emp.id = emps.size();
            emp.node_idx = node_idx++;
            emp.employee_id = e.value("employee_id", "EMP_" + std::to_string(emp.id));
            emp.pickup_lat = e.value("pickup_lat", 0.0);
            emp.pickup_lng = e.value("pickup_lng", 0.0);
            emp.drop_lat = e.value("drop_lat", office_lat);
            emp.drop_lng = e.value("drop_lng", office_lng);
            emp.priority = e.value("priority", 3);
            // Clamp priority to valid range [1, 5]
            emp.priority = std::max(1, std::min(5, emp.priority));
            emp.earliest_pickup = time_to_min(e.value("earliest_pickup", "08:00"));
            emp.latest_drop = time_to_min(e.value("latest_drop", "18:00"));
            emp.latest_arrival_deadline = emp.latest_drop + meta.priority_max_delays[emp.priority];
            emp.vehicle_pref = get_category_code(e.value("vehicle_preference", "any"));
            emp.sharing_pref = get_sharing_code(e.value("sharing_preference", "triple"));
            emps.push_back(emp);
            locs.push_back({emp.pickup_lat, emp.pickup_lng});
        }
        
        // Vehicles
        for (auto& v : data["vehicles"]) {
            Vehicle veh;
            veh.id = phys_vehs.size();
            veh.physical_id = veh.id;
            veh.node_idx = node_idx++;
            veh.start_node = veh.node_idx;
            veh.vehicle_id = v.value("vehicle_id", "VEH_" + std::to_string(veh.id));
            veh.current_lat = v.value("current_lat", 0.0);
            veh.current_lng = v.value("current_lng", 0.0);
            veh.capacity = v.value("capacity", 4);
            veh.cost_per_km = v.value("cost_per_km", 10.0);
            veh.speed_kmph = v.value("avg_speed_kmph", 40.0);
            veh.available_from = time_to_min(v.value("available_from", "08:00"));
            veh.category = get_category_code(v.value("category", "normal"));
            phys_vehs.push_back(veh);
            locs.push_back({veh.current_lat, veh.current_lng});
        }
        
        // Expand virtual vehicles (4 trips per physical)
        // Stagger by 20min per trip - allows tighter packing
        // The actual start times will be adjusted in output based on when previous trip ends
        for (const auto& pv : phys_vehs) {
            for (int trip = 0; trip < 4; trip++) {
                Vehicle vv = pv;
                vv.id = virt_vehs.size();
                vv.start_node = (trip == 0) ? pv.start_node : office;
                vv.vehicle_id = pv.vehicle_id + "_trip" + std::to_string(trip+1);
                vv.available_from = pv.available_from + (trip * 20);
                virt_vehs.push_back(vv);
            }
        }
        
        build_distance_matrix(locs, dist);
        
        std::cout << "Loaded: " << emps.size() << " employees, " 
                  << phys_vehs.size() << " vehicles (" << virt_vehs.size() << " virtual)" << std::endl;
        return true;
    }
};

#endif
