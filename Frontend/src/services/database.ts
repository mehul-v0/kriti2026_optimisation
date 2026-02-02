import { supabase } from '../lib/supabase';
import type {
  Scenario,
  EmployeeRequest,
  Vehicle,
  OptimizationResult,
  VehicleAssignment,
  Route,
  DataDigest,
} from '../types';

export class DatabaseService {
  async createScenario(name: string, description: string): Promise<Scenario> {
    const { data, error } = await supabase
      .from('scenarios')
      .insert({ name, description })
      .select()
      .single();

    if (error) throw error;
    return data;
  }

  async getScenario(id: string): Promise<Scenario | null> {
    const { data, error } = await supabase.from('scenarios').select('*').eq('id', id).maybeSingle();

    if (error) throw error;
    return data;
  }

  async getAllScenarios(): Promise<Scenario[]> {
    const { data, error } = await supabase.from('scenarios').select('*').order('created_at', { ascending: false });

    if (error) throw error;
    return data || [];
  }

  async updateScenarioStatus(id: string, status: Scenario['status']): Promise<void> {
    const { error } = await supabase.from('scenarios').update({ status }).eq('id', id);

    if (error) throw error;
  }

  async saveEmployeeRequests(requests: Omit<EmployeeRequest, 'id'>[]): Promise<void> {
    const { error } = await supabase.from('employee_requests').insert(requests);

    if (error) throw error;
  }

  async getEmployeeRequests(scenarioId: string): Promise<EmployeeRequest[]> {
    const { data, error } = await supabase.from('employee_requests').select('*').eq('scenario_id', scenarioId);

    if (error) throw error;
    return data || [];
  }

  async saveVehicles(vehicles: Omit<Vehicle, 'id'>[]): Promise<void> {
    const { error } = await supabase.from('vehicles').insert(vehicles);

    if (error) throw error;
  }

  async getVehicles(scenarioId: string): Promise<Vehicle[]> {
    const { data, error } = await supabase.from('vehicles').select('*').eq('scenario_id', scenarioId);

    if (error) throw error;
    return data || [];
  }

  async saveOptimizationResult(result: Omit<OptimizationResult, 'id' | 'completed_at'>): Promise<void> {
    const { error } = await supabase.from('optimization_results').insert(result);

    if (error) throw error;
  }

  async getOptimizationResult(scenarioId: string): Promise<OptimizationResult | null> {
    const { data, error } = await supabase
      .from('optimization_results')
      .select('*')
      .eq('scenario_id', scenarioId)
      .maybeSingle();

    if (error) throw error;
    return data;
  }

  async saveVehicleAssignments(assignments: Omit<VehicleAssignment, 'id'>[]): Promise<void> {
    const { error } = await supabase.from('vehicle_assignments').insert(assignments);

    if (error) throw error;
  }

  async getVehicleAssignments(scenarioId: string): Promise<VehicleAssignment[]> {
    const { data, error } = await supabase.from('vehicle_assignments').select('*').eq('scenario_id', scenarioId);

    if (error) throw error;
    return data || [];
  }

  async saveRoutes(routes: Omit<Route, 'id'>[]): Promise<void> {
    const { error } = await supabase.from('routes').insert(routes);

    if (error) throw error;
  }

  async getRoutes(scenarioId: string): Promise<Route[]> {
    const { data, error } = await supabase.from('routes').select('*').eq('scenario_id', scenarioId);

    if (error) throw error;
    return data || [];
  }

  async getDataDigest(scenarioId: string): Promise<DataDigest> {
    const employees = await this.getEmployeeRequests(scenarioId);
    const vehicles = await this.getVehicles(scenarioId);

    const highPriorityCount = employees.filter((e) => e.priority === 'high').length;
    const highPriorityPercent = (highPriorityCount / employees.length) * 100;

    const timeWindows = employees.map((e) => ({
      start: e.time_window_start,
      end: e.time_window_end,
    }));
    const earliestStart = timeWindows.reduce((min, tw) => (tw.start < min ? tw.start : min), timeWindows[0].start);
    const latestEnd = timeWindows.reduce((max, tw) => (tw.end > max ? tw.end : max), timeWindows[0].end);

    const fleetComposition = {
      electric: vehicles.filter((v) => v.fuel_type === 'electric').length,
      petrol: vehicles.filter((v) => v.fuel_type === 'petrol').length,
      diesel: vehicles.filter((v) => v.fuel_type === 'diesel').length,
    };

    const vehicleModes = {
      '2-wheeler': vehicles.filter((v) => v.vehicle_mode === '2-wheeler').length,
      '4-wheeler': vehicles.filter((v) => v.vehicle_mode === '4-wheeler').length,
      van: vehicles.filter((v) => v.vehicle_mode === 'van').length,
    };

    return {
      employees_count: employees.length,
      vehicles_count: vehicles.length,
      time_window_span: `${earliestStart} - ${latestEnd}`,
      high_priority_percent: highPriorityPercent,
      fleet_composition: fleetComposition,
      vehicle_modes: vehicleModes,
    };
  }
}

export const db = new DatabaseService();
