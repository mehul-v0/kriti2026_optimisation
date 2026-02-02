/*
  # Velora Corporate Mobility Optimization Schema

  ## Overview
  This migration creates the complete database schema for Velora's corporate mobility
  optimization platform, including employee requests, vehicle fleet management,
  optimization results, and route tracking.

  ## New Tables
  
  ### 1. `scenarios`
  Stores optimization test cases/scenarios
  - `id` (uuid, primary key)
  - `name` (text) - scenario name
  - `description` (text) - scenario description
  - `created_at` (timestamptz)
  - `status` (text) - 'pending', 'processing', 'completed', 'failed'
  
  ### 2. `employee_requests`
  Stores employee ride requests for each scenario
  - `id` (uuid, primary key)
  - `scenario_id` (uuid, foreign key)
  - `employee_id` (text) - employee identifier
  - `priority` (text) - 'high', 'medium', 'low'
  - `pickup_lat` (numeric) - pickup latitude
  - `pickup_lng` (numeric) - pickup longitude
  - `pickup_address` (text)
  - `destination_lat` (numeric)
  - `destination_lng` (numeric)
  - `destination_address` (text)
  - `time_window_start` (time)
  - `time_window_end` (time)
  - `vehicle_preference` (text) - 'premium', 'normal'
  - `sharing_preference` (text) - 'single', 'double', 'triple'
  
  ### 3. `vehicles`
  Stores vehicle fleet information
  - `id` (uuid, primary key)
  - `scenario_id` (uuid, foreign key)
  - `vehicle_id` (text) - vehicle identifier
  - `fuel_type` (text) - 'petrol', 'diesel', 'electric'
  - `vehicle_mode` (text) - '2-wheeler', '4-wheeler', 'van'
  - `capacity` (integer)
  - `cost_per_km` (numeric)
  - `avg_mileage` (numeric)
  - `avg_speed` (numeric)
  - `vehicle_age` (numeric)
  - `current_lat` (numeric)
  - `current_lng` (numeric)
  - `current_address` (text)
  - `availability_time` (time)
  
  ### 4. `optimization_results`
  Stores overall optimization results for each scenario
  - `id` (uuid, primary key)
  - `scenario_id` (uuid, foreign key)
  - `total_cost` (numeric)
  - `baseline_cost` (numeric)
  - `cost_savings` (numeric)
  - `cost_savings_percent` (numeric)
  - `total_distance` (numeric)
  - `total_time` (numeric)
  - `vehicles_used` (integer)
  - `vehicles_available` (integer)
  - `completed_at` (timestamptz)
  
  ### 5. `vehicle_assignments`
  Stores employee-to-vehicle assignments
  - `id` (uuid, primary key)
  - `scenario_id` (uuid, foreign key)
  - `vehicle_id` (text)
  - `employee_id` (text)
  - `pickup_time` (time)
  - `dropoff_time` (time)
  - `sequence_order` (integer)
  - `is_pickup` (boolean)
  
  ### 6. `routes`
  Stores route details for each vehicle
  - `id` (uuid, primary key)
  - `scenario_id` (uuid, foreign key)
  - `vehicle_id` (text)
  - `route_points` (jsonb) - array of lat/lng points
  - `total_distance` (numeric)
  - `total_cost` (numeric)
  - `passengers_count` (integer)
  - `capacity_utilization` (numeric)

  ## Security
  - Enable RLS on all tables
  - Add policies for authenticated users to manage their data
*/

-- Create scenarios table
CREATE TABLE IF NOT EXISTS scenarios (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  name text NOT NULL,
  description text DEFAULT '',
  created_at timestamptz DEFAULT now(),
  status text DEFAULT 'pending' CHECK (status IN ('pending', 'processing', 'completed', 'failed'))
);

-- Create employee_requests table
CREATE TABLE IF NOT EXISTS employee_requests (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  scenario_id uuid REFERENCES scenarios(id) ON DELETE CASCADE,
  employee_id text NOT NULL,
  priority text DEFAULT 'medium' CHECK (priority IN ('high', 'medium', 'low')),
  pickup_lat numeric NOT NULL,
  pickup_lng numeric NOT NULL,
  pickup_address text DEFAULT '',
  destination_lat numeric NOT NULL,
  destination_lng numeric NOT NULL,
  destination_address text DEFAULT '',
  time_window_start time NOT NULL,
  time_window_end time NOT NULL,
  vehicle_preference text DEFAULT 'normal' CHECK (vehicle_preference IN ('premium', 'normal')),
  sharing_preference text DEFAULT 'double' CHECK (sharing_preference IN ('single', 'double', 'triple'))
);

-- Create vehicles table
CREATE TABLE IF NOT EXISTS vehicles (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  scenario_id uuid REFERENCES scenarios(id) ON DELETE CASCADE,
  vehicle_id text NOT NULL,
  fuel_type text DEFAULT 'petrol' CHECK (fuel_type IN ('petrol', 'diesel', 'electric')),
  vehicle_mode text DEFAULT '4-wheeler' CHECK (vehicle_mode IN ('2-wheeler', '4-wheeler', 'van')),
  capacity integer DEFAULT 4,
  cost_per_km numeric DEFAULT 10.0,
  avg_mileage numeric DEFAULT 15.0,
  avg_speed numeric DEFAULT 30.0,
  vehicle_age numeric DEFAULT 2.0,
  current_lat numeric NOT NULL,
  current_lng numeric NOT NULL,
  current_address text DEFAULT '',
  availability_time time DEFAULT '08:00:00'
);

-- Create optimization_results table
CREATE TABLE IF NOT EXISTS optimization_results (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  scenario_id uuid REFERENCES scenarios(id) ON DELETE CASCADE,
  total_cost numeric DEFAULT 0,
  baseline_cost numeric DEFAULT 0,
  cost_savings numeric DEFAULT 0,
  cost_savings_percent numeric DEFAULT 0,
  total_distance numeric DEFAULT 0,
  total_time numeric DEFAULT 0,
  vehicles_used integer DEFAULT 0,
  vehicles_available integer DEFAULT 0,
  completed_at timestamptz DEFAULT now()
);

-- Create vehicle_assignments table
CREATE TABLE IF NOT EXISTS vehicle_assignments (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  scenario_id uuid REFERENCES scenarios(id) ON DELETE CASCADE,
  vehicle_id text NOT NULL,
  employee_id text NOT NULL,
  pickup_time time,
  dropoff_time time,
  sequence_order integer DEFAULT 0,
  is_pickup boolean DEFAULT true
);

-- Create routes table
CREATE TABLE IF NOT EXISTS routes (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  scenario_id uuid REFERENCES scenarios(id) ON DELETE CASCADE,
  vehicle_id text NOT NULL,
  route_points jsonb DEFAULT '[]'::jsonb,
  total_distance numeric DEFAULT 0,
  total_cost numeric DEFAULT 0,
  passengers_count integer DEFAULT 0,
  capacity_utilization numeric DEFAULT 0
);

-- Enable Row Level Security
ALTER TABLE scenarios ENABLE ROW LEVEL SECURITY;
ALTER TABLE employee_requests ENABLE ROW LEVEL SECURITY;
ALTER TABLE vehicles ENABLE ROW LEVEL SECURITY;
ALTER TABLE optimization_results ENABLE ROW LEVEL SECURITY;
ALTER TABLE vehicle_assignments ENABLE ROW LEVEL SECURITY;
ALTER TABLE routes ENABLE ROW LEVEL SECURITY;

-- Create policies for public access (since no auth is required for this demo)
CREATE POLICY "Allow public read access to scenarios"
  ON scenarios FOR SELECT
  TO public
  USING (true);

CREATE POLICY "Allow public insert access to scenarios"
  ON scenarios FOR INSERT
  TO public
  WITH CHECK (true);

CREATE POLICY "Allow public update access to scenarios"
  ON scenarios FOR UPDATE
  TO public
  USING (true)
  WITH CHECK (true);

CREATE POLICY "Allow public read access to employee_requests"
  ON employee_requests FOR SELECT
  TO public
  USING (true);

CREATE POLICY "Allow public insert access to employee_requests"
  ON employee_requests FOR INSERT
  TO public
  WITH CHECK (true);

CREATE POLICY "Allow public read access to vehicles"
  ON vehicles FOR SELECT
  TO public
  USING (true);

CREATE POLICY "Allow public insert access to vehicles"
  ON vehicles FOR INSERT
  TO public
  WITH CHECK (true);

CREATE POLICY "Allow public read access to optimization_results"
  ON optimization_results FOR SELECT
  TO public
  USING (true);

CREATE POLICY "Allow public insert access to optimization_results"
  ON optimization_results FOR INSERT
  TO public
  WITH CHECK (true);

CREATE POLICY "Allow public read access to vehicle_assignments"
  ON vehicle_assignments FOR SELECT
  TO public
  USING (true);

CREATE POLICY "Allow public insert access to vehicle_assignments"
  ON vehicle_assignments FOR INSERT
  TO public
  WITH CHECK (true);

CREATE POLICY "Allow public read access to routes"
  ON routes FOR SELECT
  TO public
  USING (true);

CREATE POLICY "Allow public insert access to routes"
  ON routes FOR INSERT
  TO public
  WITH CHECK (true);

-- Create indexes for better query performance
CREATE INDEX IF NOT EXISTS idx_employee_requests_scenario ON employee_requests(scenario_id);
CREATE INDEX IF NOT EXISTS idx_vehicles_scenario ON vehicles(scenario_id);
CREATE INDEX IF NOT EXISTS idx_optimization_results_scenario ON optimization_results(scenario_id);
CREATE INDEX IF NOT EXISTS idx_vehicle_assignments_scenario ON vehicle_assignments(scenario_id);
CREATE INDEX IF NOT EXISTS idx_routes_scenario ON routes(scenario_id);